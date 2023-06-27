#pragma once

#include "IUI/GFxDisplayObject.h"

#include "RE/H/HUDObject.h"

namespace DEM
{
	struct ExtraMarker
	{
		struct Type
		{
			enum
			{
				kCombatant,
				kHostile,
				kGuard
			};
		};

		struct CreateData
		{
			enum
			{
				kName,
				kIconType,
				kStride
			};
		};

		struct RefreshData
		{
			enum
			{
				kX,
				kY,
				kStride
			};
		};
	};

	class Minimap : public RE::HUDObject
	{
	public:
		static constexpr inline std::string_view path = "_level0.HUDMovieBaseInstance.Minimap";

		// override HUDObject
		void Update() override {}								 // 01
		bool ProcessMessage(RE::UIMessage* a_message) override;	 // 02

		static void InitSingleton(const IUI::GFxDisplayObject& a_map)
		{
			if (!singleton)
			{
				static Minimap singletonInstance{ a_map };
				singleton = &singletonInstance;
			}
		}

		static Minimap* GetSingleton() { return singleton; }

		void SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs);

		// Per frame steps: UpdateOnEnterFrame, Advance, PreRender
		void UpdateOnEnterFrame(const RE::FxDelegateArgs& a_delegateArgs);
		void Advance();
		void PreRender();

		// members
		RE::HUDMenu* menu = nullptr;

	private:
		Minimap(const IUI::GFxDisplayObject& a_map) :
			RE::HUDObject{ a_map.GetMovieView() }
		{}

		void InitLocalMap();

		void CreateMarkers();
		void RefreshMarkers();

		void UpdateFogOfWar();
		void RenderOffscreen();

		std::array<RE::GFxValue, 2> GetCurrentLocationTitle() const;
		RE::NiPoint2 WorldToScreen(const RE::NiPoint3& a_position) const;

		static inline Minimap* singleton = nullptr;

		// members
		RE::LocalMapMenu* localMap = nullptr;
		RE::LocalMapMenu::RUNTIME_DATA* localMap_ = nullptr;
		RE::LocalMapMenu::LocalMapCullingProcess* cullingProcess = nullptr;
		RE::LocalMapCamera* cameraContext = nullptr;

		RE::BSTArray<RE::NiPointer<RE::Actor>> combatantActors;
		RE::BSTArray<RE::NiPointer<RE::Actor>> hostileActors;
		RE::BSTArray<RE::NiPointer<RE::Actor>> guardActors;
		RE::GFxValue extraMarkerData;
		bool frameUpdatePending = true;

		const char* const& clearedStr = RE::GameSettingCollection::GetSingleton()->GetSetting("sCleared")->data.s;
		const float& localMapHeight = RE::INISettingCollection::GetSingleton()->GetSetting("fMapLocalHeight:MapMenu")->data.f;
		const float& localMapMargin = *REL::Relocation<float*>{ RELOCATION_ID(234438, 0) }.get();

		bool& isFogOfWarEnabled = *REL::Relocation<bool*>{ RELOCATION_ID(501260, 0) }.get();
		bool& byte_141E0DC5C = *REL::Relocation<bool*>{ RELOCATION_ID(513141, 0) }.get();
		bool& byte_141E0DC5D = *REL::Relocation<bool*>{ RELOCATION_ID(513142, 0) }.get();
		bool& enableWaterRendering = *REL::Relocation<bool*>{ RELOCATION_ID(513342, 0) }.get();
		std::uint32_t& dword_1431D0D8C = *REL::Relocation<std::uint32_t*>{ RELOCATION_ID(527629, 0) }.get();
		bool& byte_1431D1D30 = *REL::Relocation<bool*>{ RELOCATION_ID(527793, 0) }.get();
		bool& useMapBrightnessAndContrastBoost = *REL::Relocation<bool*>{ RELOCATION_ID(528107, 0) }.get();
	};
}