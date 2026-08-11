#include "Hooks.h"

namespace Hooks
{
    // Vanilla Lockpick misc item (Skyrim.esm)
    static constexpr RE::FormID LOCKPICK_FORM_ID = 0x0000000A;

    // Passed as the removal count to take an entire stack: the engine clamps
    // the count to what the container actually holds (the same behavior the
    // Papyrus RemoveItem(item, 999999) idiom relies on — it ends in this very
    // virtual). Kept within int16 range in case engine internals are 16-bit.
    static constexpr std::int32_t TAKE_ALL_COUNT = 30000;

    // In Skyrim VR, looting a body or container calls RemoveItem on the source
    // with reason kStoreInContainer (4); trading with a follower uses
    // kStoreInTeammate (5). Both observed in-game; gold arrives the same way
    // but with the full stack count. Only single-item takes (count == 1) are
    // promoted to take-all, so explicit quantity-prompt choices pass through.
    //
    // NOTE: the ref the VR menu passes as the source is a proxy object
    // (FormID 0xFFFFFFFE) whose memory layout does not match CommonLibSSE-NG's
    // VR model — touching its extraList or inventory through NG helpers
    // crashes. Do not inspect it; only forward it to the original function.
    static bool ShouldTakeAll(
        RE::TESObjectREFR*     a_source,
        RE::TESBoundObject*    a_item,
        std::int32_t           a_count,
        RE::ITEM_REMOVE_REASON a_reason,
        RE::TESObjectREFR*     a_moveToRef) noexcept
    {
        return a_count == 1
            && a_item && a_item->GetFormID() == LOCKPICK_FORM_ID
            && (a_reason == RE::ITEM_REMOVE_REASON::kStoreInContainer || a_reason == RE::ITEM_REMOVE_REASON::kStoreInTeammate)
            && a_moveToRef && a_moveToRef->IsPlayerRef()
            && a_source && !a_source->IsPlayerRef();
    }

    // Diagnostic: log every RemoveItem call involving a lockpick or a transfer
    // to the player, so the VR take path can be identified from the log.
    static void LogRemoveItem(
        const char*            a_tag,
        RE::TESObjectREFR*     a_source,
        RE::TESBoundObject*    a_item,
        std::int32_t           a_count,
        RE::ITEM_REMOVE_REASON a_reason,
        RE::ExtraDataList*     a_extraList,
        RE::TESObjectREFR*     a_moveToRef) noexcept
    {
        const bool lockpick = a_item && a_item->GetFormID() == LOCKPICK_FORM_ID;
        const bool toPlayer = a_moveToRef && a_moveToRef->IsPlayerRef();
        if (lockpick || toPlayer) {
            logger::info(
                "[{}] RemoveItem: item={:08X} count={} reason={} hasExtraList={} moveTo={:08X} source={:08X}",
                a_tag,
                a_item ? a_item->GetFormID() : 0,
                a_count,
                std::to_underlying(a_reason),
                a_extraList != nullptr,
                a_moveToRef ? a_moveToRef->GetFormID() : 0,
                a_source ? a_source->GetFormID() : 0);
        }
    }

    static RE::ObjectRefHandle RemoveItemImpl(
        const REL::Relocation<decltype(&ContainerRemoveItem::Thunk)>& a_func,
        const char*            a_tag,
        RE::TESObjectREFR*     a_this,
        RE::TESBoundObject*    a_item,
        std::int32_t           a_count,
        RE::ITEM_REMOVE_REASON a_reason,
        RE::ExtraDataList*     a_extraList,
        RE::TESObjectREFR*     a_moveToRef,
        const RE::NiPoint3*    a_dropLoc,
        const RE::NiPoint3*    a_rotate) noexcept
    {
        LogRemoveItem(a_tag, a_this, a_item, a_count, a_reason, a_extraList, a_moveToRef);

        if (ShouldTakeAll(a_this, a_item, a_count, a_reason, a_moveToRef)) {
            logger::info("[{}] Promoting single lockpick take to the whole stack", a_tag);
            return a_func(a_this, a_item, TAKE_ALL_COUNT, a_reason, nullptr, a_moveToRef, a_dropLoc, a_rotate);
        }
        return a_func(a_this, a_item, a_count, a_reason, a_extraList, a_moveToRef, a_dropLoc, a_rotate);
    }

    RE::ObjectRefHandle ContainerRemoveItem::Thunk(
        RE::TESObjectREFR*     a_this,
        RE::TESBoundObject*    a_item,
        std::int32_t           a_count,
        RE::ITEM_REMOVE_REASON a_reason,
        RE::ExtraDataList*     a_extraList,
        RE::TESObjectREFR*     a_moveToRef,
        const RE::NiPoint3*    a_dropLoc,
        const RE::NiPoint3*    a_rotate) noexcept
    {
        return RemoveItemImpl(func, "Container", a_this, a_item, a_count, a_reason, a_extraList, a_moveToRef, a_dropLoc, a_rotate);
    }

    RE::ObjectRefHandle ActorRemoveItem::Thunk(
        RE::TESObjectREFR*     a_this,
        RE::TESBoundObject*    a_item,
        std::int32_t           a_count,
        RE::ITEM_REMOVE_REASON a_reason,
        RE::ExtraDataList*     a_extraList,
        RE::TESObjectREFR*     a_moveToRef,
        const RE::NiPoint3*    a_dropLoc,
        const RE::NiPoint3*    a_rotate) noexcept
    {
        return RemoveItemImpl(func, "Actor", a_this, a_item, a_count, a_reason, a_extraList, a_moveToRef, a_dropLoc, a_rotate);
    }

    void Install() noexcept
    {
        if (!REL::Module::IsVR()) {
            logger::info("Not Skyrim VR — mod will not be active.");
            return;
        }

        stl::write_vfunc<RE::TESObjectREFR, ContainerRemoveItem>();
        logger::info("Installed TESObjectREFR::RemoveItem hook (containers)");

        stl::write_vfunc<RE::Character, ActorRemoveItem>();
        logger::info("Installed Character::RemoveItem hook (bodies)");
    }
}
