#pragma once

namespace Hooks
{
    void Install() noexcept;

    // TESObjectREFR::RemoveItem and Actor::RemoveItem occupy vtable slot 0x56,
    // but Actor overrides it, so containers (TESObjectREFR) and bodies
    // (Character) must each be patched in their own vtable.
    struct ContainerRemoveItem
    {
        static RE::ObjectRefHandle Thunk(
            RE::TESObjectREFR*     a_this,
            RE::TESBoundObject*    a_item,
            std::int32_t           a_count,
            RE::ITEM_REMOVE_REASON a_reason,
            RE::ExtraDataList*     a_extraList,
            RE::TESObjectREFR*     a_moveToRef,
            const RE::NiPoint3*    a_dropLoc,
            const RE::NiPoint3*    a_rotate) noexcept;

        inline static REL::Relocation<decltype(&Thunk)> func;
        static constexpr std::size_t idx{ 0x56 };
    };

    struct ActorRemoveItem
    {
        static RE::ObjectRefHandle Thunk(
            RE::TESObjectREFR*     a_this,
            RE::TESBoundObject*    a_item,
            std::int32_t           a_count,
            RE::ITEM_REMOVE_REASON a_reason,
            RE::ExtraDataList*     a_extraList,
            RE::TESObjectREFR*     a_moveToRef,
            const RE::NiPoint3*    a_dropLoc,
            const RE::NiPoint3*    a_rotate) noexcept;

        inline static REL::Relocation<decltype(&Thunk)> func;
        static constexpr std::size_t idx{ 0x56 };
    };
}
