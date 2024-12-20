#pragma once

#include "utils/Trampoline.h"

void AcceptHUDMenu(RE::HUDMenu* a_hudMenu, RE::FxDelegateHandler::CallbackProcessor* a_gameDelegate);
void AdvanceMovieHUDMenu(RE::HUDMenu* a_hudMenu, float a_interval, std::uint32_t a_currentTime);
void PreDisplayHUDMenu(RE::HUDMenu* a_hudMenu);
void RefreshPlatformHUDMenu(RE::HUDMenu* a_hudMenu);

bool CanProcessMenuOpenHandler(RE::MenuOpenHandler* a_menuOpenHandler, RE::InputEvent* a_event);

namespace hooks
{
	class HUDMenu
	{
		static constexpr REL::RelocationID ProcessMessageId{ 50718, 51612 };

	public:
		static inline REL::Relocation<std::uintptr_t> vTable{ RE::VTABLE_HUDMenu[0] };

		static inline REL::Relocation<void (RE::HUDMenu::*)(RE::FxDelegateHandler::CallbackProcessor*)> Accept;
		static inline REL::Relocation<void (RE::HUDMenu::*)(float, std::uint32_t)> AdvanceMovie;
		static inline REL::Relocation<void (RE::HUDMenu::*)()> PreDisplay;
		static inline REL::Relocation<void (RE::HUDMenu::*)()> RefreshPlatform;
	};

	class LocalMapMenu
	{
		static constexpr REL::RelocationID CtorId{ 52076, 52964 };
		static constexpr REL::RelocationID RefreshMarkersId{ 52090, 52980 };

	public:
		class LocalMapCullingProcess
		{
			static constexpr REL::RelocationID RenderOffScreenId{ 16094, 16335 };

		public:
			static inline REL::Relocation<void (RE::LocalMapMenu::LocalMapCullingProcess::*)()> RenderOffScreen{ RenderOffScreenId };
		};

		static inline REL::Relocation<RE::LocalMapMenu* (RE::LocalMapMenu::*)()> Ctor{ CtorId };
		static inline REL::Relocation<void (RE::LocalMapMenu::*)()> RefreshMarkers{ RefreshMarkersId };
	};

	class NiCamera
	{
	public:
		static inline REL::Relocation<bool (RE::NiCamera::*)(const RE::NiPoint3&, float&, float&, float&, float)> WorldPtToScreenPt3;
	};

	class MenuOpenHandler
	{
	public:
		static inline REL::Relocation<std::uintptr_t> vTable{ RE::VTABLE_MenuOpenHandler[0] };

		static inline REL::Relocation<bool (RE::MenuOpenHandler::*)(RE::InputEvent*)> CanProcess;
	};

	static inline void Install()
	{
		HUDMenu::Accept = HUDMenu::vTable.write_vfunc(1, AcceptHUDMenu);
		HUDMenu::AdvanceMovie = HUDMenu::vTable.write_vfunc(5, AdvanceMovieHUDMenu);
		HUDMenu::PreDisplay = HUDMenu::vTable.write_vfunc(7, PreDisplayHUDMenu);
		HUDMenu::RefreshPlatform = HUDMenu::vTable.write_vfunc(8, RefreshPlatformHUDMenu);
		
		MenuOpenHandler::CanProcess = MenuOpenHandler::vTable.write_vfunc(1, CanProcessMenuOpenHandler);
	}
}
