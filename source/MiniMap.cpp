#include "Minimap.h"

#include "utils/Logger.h"

#include "RE/B/BSTimer.h"
#include "RE/I/ImageSpaceManager.h"
#include "RE/I/ImageSpaceShaderParam.h"
#include "RE/L/LocalMapCamera.h"
#include "RE/R/Renderer.h"
#include "RE/S/ShaderAccumulator.h"
#include "RE/S/ShadowState.h"
#include "RE/R/RenderPassCache.h"
#include "RE/R/RenderTargetManager.h"

namespace RE
{
	class BSPortalGraphEntry : public NiRefObject
	{
	public:

		~BSPortalGraphEntry() override; // 00

		// members
		BSPortalGraph* portalGraph; // 10
		// ...
	};
	//static_assert(sizeof(BSPortalGraphEntry) == 0x140);

	RE::BSPortalGraphEntry* Main__GetPortalGraphEntry(Main* a_this)
	{
		using func_t = decltype(&Main__GetPortalGraphEntry);
		REL::Relocation<func_t> func{ RELOCATION_ID(35607, 36617) };

		return func(a_this);
	}

	void* NiCamera__Accumulate(NiCamera* a_this, BSGraphics::BSShaderAccumulator* a_shaderAccumulator, std::uint32_t a_unk2)
	{
		using func_t = decltype(&NiCamera__Accumulate);
		REL::Relocation<func_t> func{ RELOCATION_ID(99789, 106436) };

		return func(a_this, a_shaderAccumulator, a_unk2);
	}
}

namespace debug
{
	std::uint32_t GetCurrentNumOfUsedPasses()
	{
		RE::RenderPassCache* renderPassCache = RE::RenderPassCache::GetSingleton();

		RE::RenderPassCache::Pool& pool = renderPassCache->pools[0];

		uint32_t usedPasses = 0;
		static constexpr uint32_t passCount = 65535;

		for (uint32_t passIndex = 0; passIndex < passCount; ++passIndex)
		{
			const RE::BSRenderPass& pass = pool.passes[passIndex];
			if (pass.passEnum != 0)
			{
				usedPasses++;
			}
		}

		return usedPasses;
	}
}

namespace DEM
{
	bool Minimap::ProcessMessage(RE::UIMessage* a_message)
	{
		if (!localMap)
		{
			InitLocalMap();
		}

		return false;
	}

	void Minimap::RegisterHUDComponent(RE::FxDelegateArgs& a_params)
	{
		RE::HUDObject::RegisterHUDComponent(a_params);
		displayObj.Invoke("AddToHudElements");
	}

	void Minimap::InitLocalMap()
	{
		localMap = static_cast<RE::LocalMapMenu*>(std::malloc(sizeof(RE::LocalMapMenu)));
		if (localMap)
		{
			localMap->Ctor();
			localMap_ = &localMap->GetRuntimeData();
			cullingProcess = &localMap->localCullingProcess;
			cameraContext = cullingProcess->GetLocalMapCamera();

			RE::MenuControls::GetSingleton()->RemoveHandler(localMap_->inputHandler.get());
			RE::MenuControls::GetSingleton()->AddHandler(inputHandler.get());

			localMap_->movieView = view.get(); 

			view->GetVariable(&localMap_->root, (std::string(DEM::Minimap::path) + ".MapClip").c_str());

			localMap_->root.Invoke("InitMap");

			view->CreateArray(&localMap->markerData);
			localMap_->root.GetMember("IconDisplay", &localMap_->iconDisplay);
			localMap_->iconDisplay.SetMember("MarkerData", localMap->markerData);

			localMap_->enabled = true;
			localMap_->usingCursor = 0;
			localMap_->inForeground = localMap_->enabled;

			view->CreateArray(&extraMarkerData);
			localMap_->iconDisplay.SetMember("ExtraMarkerData", extraMarkerData);

			cameraContext->currentState->Begin();

			RE::GFxValue gfxEnable = localMap_->enabled;
			localMap_->root.Invoke("Show", nullptr, &gfxEnable, 1);

			RE::NiPoint3 playerPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
			cameraContext->SetDefaultStateInitialPosition(playerPos);
		}
	}

	void Minimap::SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs)
	{
		float localLeft = a_delegateArgs[0].GetNumber();
		float localTop = a_delegateArgs[1].GetNumber();
		float localRight = a_delegateArgs[2].GetNumber();
		float localBottom = a_delegateArgs[3].GetNumber();

		float identityMat2D[2][3] = { { 1.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } };

		RE::GPointF localTopLeft{ localLeft, localTop };
		localMap->topLeft = view->TranslateToScreen(localTopLeft, identityMat2D);

		RE::GPointF localBottomRight{ localRight, localBottom };
		localMap->bottomRight = view->TranslateToScreen(localBottomRight, identityMat2D);

		float aspectRatio = (localMap->bottomRight.x - localMap->topLeft.x) / (localMap->bottomRight.y - localMap->topLeft.y);

		cameraContext->defaultState->minFrustumHalfWidth = aspectRatio * cameraContext->defaultState->minFrustumHalfHeight;
	}

	void Minimap::Advance()
	{
		if (localMap && localMap_->enabled)
		{
			std::array<RE::GFxValue, 2> title;

			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

			RE::TESObjectCELL* parentCell = player->parentCell;
			if (parentCell)
			{
				float northRotation = parentCell->GetNorthRotation();
				cameraContext->SetNorthRotation(-northRotation);

				if (parentCell->IsInteriorCell())
				{
					title[0] = parentCell->GetFullName();
				}
				else if (RE::BGSLocation* location = parentCell->GetLocation())
				{
					title[0] = location->GetFullName();

					if (location->everCleared)
					{
						title[1] = clearedStr;
					}
				}
				else
				{
					RE::TESWorldSpace* worldSpace = player->GetWorldspace();
					title[0] = worldSpace->GetFullName();
				}
			}
			localMap_->root.Invoke("SetTitle", nullptr, title);

			CreateMarkers();
			RefreshMarkers();

			isCameraUpdatePending = true;
		}
	}

	void Minimap::PreRender()
	{
		if (localMap && localMap_->enabled)
		{
			if (isCameraUpdatePending)
			{
				RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

				RE::NiPoint3 playerPos = player->GetPosition();
				cameraContext->defaultState->initialPosition.x = playerPos.x;
				cameraContext->defaultState->initialPosition.y = playerPos.y;

				if (!inputControlledMode)
				{
					cameraContext->defaultState->translation = RE::NiPoint3::Zero();
				}

				cameraContext->Update();

				isCameraUpdatePending = false;

				RE::LoadedAreaBound* loadedAreaBound = RE::TES::GetSingleton()->GetRuntimeData2().loadedAreaBound;
				cameraContext->SetAreaBounds(loadedAreaBound->maxExtent, loadedAreaBound->minExtent);

				UpdateFogOfWar();
			}

			RenderOffscreen();
		}

		using namespace std::chrono;
		static time_point<system_clock> lastLog;
		time_point<system_clock> now = system_clock::now();
		if (duration_cast<milliseconds>(now - lastLog) > 500ms)
		{
			size_t usedPassesAfter = debug::GetCurrentNumOfUsedPasses();

			logger::debug("Render Passes: {}", usedPassesAfter);

			lastLog = now;
		}
	}

	void Minimap::CreateMarkers()
	{
		localMap->PopulateData();

		enemyActors.clear();
		hostileActors.clear();
		guardActors.clear();

		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

		for (RE::ActorHandle& highActorHandle : RE::ProcessLists::GetSingleton()->highActorHandles)
		{
			RE::NiPointer<RE::Actor> actor = highActorHandle.get();
			if (actor && RE::NiCamera::PointInFrustum(actor->GetPosition(), cameraContext->camera.get(), 1)) 
			{
				bool isActorCombatant = false;

				for (RE::ActorHandle& combatantActorHandle : player->GetPlayerRuntimeData().actorsToDisplayOnTheHUDArray)
				{
					if (highActorHandle == combatantActorHandle)
					{
						enemyActors.push_back(actor);
						isActorCombatant = true;
						break;
					}
				}

				if (!isActorCombatant && !actor->IsDead() &&
					actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAggression) != 0.0F)
				{
					if (actor->IsHostileToActor(player))
					{
						hostileActors.push_back(actor);
					}
					else if (actor->IsGuard())
					{
						guardActors.push_back(actor);
					}
				}
			}
		}

		extraMarkerData.ClearElements();

		std::size_t numEnemyActors = enemyActors.size();
		std::size_t numHostileActors = hostileActors.size();
		std::size_t numGuardActors = guardActors.size();

		extraMarkerData.SetArraySize((numEnemyActors + numHostileActors + numGuardActors) * ExtraMarker::CreateData::kStride);

		int j = 0;

		for (int i = 0; i < numHostileActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = hostileActors[i];
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kName, actor->GetName());
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kIconType, ExtraMarker::Type::kHostile);
			j += ExtraMarker::CreateData::kStride;
		}

		for (int i = 0; i < numGuardActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = guardActors[i];
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kName, actor->GetName());
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kIconType, ExtraMarker::Type::kGuard);
			j += ExtraMarker::CreateData::kStride;
		}

		for (int i = 0; i < numEnemyActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = enemyActors[i];
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kName, actor->GetName());
			extraMarkerData.SetElement(j + ExtraMarker::CreateData::kIconType, ExtraMarker::Type::kCombatant);
			j += ExtraMarker::CreateData::kStride;
		}

		localMap_->iconDisplay.Invoke("CreateMarkers", nullptr, nullptr, 0);
	}

	void Minimap::RefreshMarkers()
	{
		std::size_t numEnemyActors = enemyActors.size();
		std::size_t numHostileActors = hostileActors.size();
		std::size_t numGuardActors = guardActors.size();

		extraMarkerData.SetArraySize((numEnemyActors + numHostileActors + numGuardActors) * ExtraMarker::RefreshData::kStride);
				
		int j = 0;

		for (int i = 0; i < numHostileActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = hostileActors[i];
			RE::NiPoint2 screenPos = cameraContext->WorldToScreen(actor->GetPosition());
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kX, screenPos.x);
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kY, screenPos.y);
			j += ExtraMarker::RefreshData::kStride;
		}

		for (int i = 0; i < numGuardActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = guardActors[i];
			RE::NiPoint2 screenPos = cameraContext->WorldToScreen(actor->GetPosition());
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kX, screenPos.x);
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kY, screenPos.y);
			j += ExtraMarker::RefreshData::kStride;
		}

		for (int i = 0; i < numEnemyActors; i++)
		{
			RE::NiPointer<RE::Actor>& actor = enemyActors[i];
			RE::NiPoint2 screenPos = cameraContext->WorldToScreen(actor->GetPosition());
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kX, screenPos.x);
			extraMarkerData.SetElement(j + ExtraMarker::RefreshData::kY, screenPos.y);
			j += ExtraMarker::RefreshData::kStride;
		}

		localMap->RefreshMarkers();
	}

	void Minimap::UpdateFogOfWar()
	{
		auto& fogOfWarOverlayHolder = reinterpret_cast< RE::NiPointer<RE::NiNode>&>(cullingProcess->GetFogOfWarOverlay());

		if (!fogOfWarOverlayHolder)
		{
			localMap->localCullingProcess.CreateFogOfWar();
		}
		else
		{
			// Clear last-frame fog of war overlay because I don't know how to update BSTriShapes.
			// Maybe that would improve performance.
			std::uint32_t childrenSize = fogOfWarOverlayHolder->GetChildren().size();
			for (std::uint32_t i = 0; i < childrenSize; i++)
			{
				fogOfWarOverlayHolder->DetachChildAt(i);
			}

			RE::LocalMapMenu::FogOfWar fogOfWar;

			fogOfWar.overlayHolder = fogOfWarOverlayHolder.get();

			RE::TESObjectCELL* interiorCell = RE::TES::GetSingleton()->interiorCell;

			if (interiorCell)
			{
				fogOfWar.minExtent = cameraContext->minExtent;
				fogOfWar.maxExtent = cameraContext->maxExtent;
				fogOfWar.gridDivisions = 32;
		
				cullingProcess->AttachFogOfWarOverlay(fogOfWar, interiorCell);
			}
			else
			{
				fogOfWar.gridDivisions = 16;
		
				RE::GridCellArray* gridCells = RE::TES::GetSingleton()->gridCells;
				std::uint32_t gridLength = gridCells->length;
		
				if (gridLength)
				{
					for (std::uint32_t gridX = 0; gridX < gridLength; gridX++)
					{	
						for (std::uint32_t gridY = 0; gridY < gridLength; gridY++)
						{					
							RE::TESObjectCELL* cell = gridCells->GetCell(gridX, gridY);
							if (cell && cell->IsAttached())
							{
								cullingProcess->AttachFogOfWarOverlay(fogOfWar, cell);
							}
						}
					}
				}

				RE::TESWorldSpace* worldSpace = RE::TES::GetSingleton()->GetRuntimeData2().worldSpace;
				if (worldSpace)
				{
					RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
					if (skyCell && skyCell->IsAttached())
					{
						cullingProcess->AttachFogOfWarOverlay(fogOfWar, skyCell);
					}
				}
			}
	
			fogOfWarOverlayHolder->local.translate.z = (cameraContext->maxExtent.z - cameraContext->minExtent.z) * 0.5;
			
			RE::NiUpdateData niUpdateData;
			niUpdateData.time = 0.0F;

			fogOfWarOverlayHolder->Update(niUpdateData);
		}
	}

	void Minimap::RenderOffscreen()
	{
		// 1. Setup culling step ///////////////////////////////////////////////////////////////////////////////////////////

		RE::ShadowSceneNode* mainShadowSceneNode = RE::ShadowSceneNode::GetMain();

        RE::NiPointer<RE::BSGraphics::BSShaderAccumulator>& shaderAccumulator = cullingProcess->GetShaderAccumulator();

		shaderAccumulator->GetRuntimeData().activeShadowSceneNode = mainShadowSceneNode;

		RE::NiTObjectArray<RE::NiPointer<RE::NiAVObject>>& mainShadowSceneChildren = mainShadowSceneNode->GetChildren();

		RE::NiPointer<RE::NiAVObject>& objectLODRoot = mainShadowSceneChildren[3];

		bool unk219 = mainShadowSceneNode->GetRuntimeData().unk219;
		mainShadowSceneNode->GetRuntimeData().unk219 = true;

		bool areLODsHidden = objectLODRoot->GetFlags().any(RE::NiAVObject::Flag::kHidden);
		objectLODRoot->GetFlags().reset(RE::NiAVObject::Flag::kHidden);
		bool isByte_1431D1D30 = byte_1431D1D30;
		bool isByte_141E0DC5C_D = byte_141E0DC5C;
		byte_1431D1D30 = true;
		byte_141E0DC5D = byte_141E0DC5C = false;
		dword_1431D0D8C = 0;

		RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
		renderer->SetClearColor(0.0F, 0.0F, 0.0F, 1.0F);

        RE::TES* tes = RE::TES::GetSingleton();
		RE::TESWorldSpace* worldSpace = tes->GetRuntimeData2().worldSpace;

		RE::LocalMapMenu::LocalMapCullingProcess::UnkData unkData{ cullingProcess };
		
		bool isWaterRenderingEnabled = enableWaterRendering;
		enableWaterRendering = false;
		bool isUsingMapBrightnessAndContrastBoost = useMapBrightnessAndContrastBoost;
		useMapBrightnessAndContrastBoost = true;

		if (worldSpace && worldSpace->flags.any(RE::TESWorldSpace::Flag::kFixedDimensions))
		{
			unkData.unk8 = false;
		}
		else 
		{
			unkData.unk8 = true;
		}

        RE::TESObjectCELL* currentCell = nullptr;

		RE::TESObjectCELL* interiorCell = tes->interiorCell;

		// 2. Culling step /////////////////////////////////////////////////////////////////////////////////////////////////

		if (interiorCell)
		{
			currentCell = interiorCell;
		}
		else if (worldSpace)
		{
			if (worldSpace->flags.none(RE::TESWorldSpace::Flag::kSmallWorld))
			{
				enableWaterRendering = true;
			}

			RE::TESObjectCELL* skyCell = worldSpace->GetSkyCell();
			if (skyCell && skyCell->IsAttached())
            {
				if (cullingProcess->CullTerrain(tes->gridCells, unkData, nullptr) == 1)
				{
					currentCell = skyCell;
				}
            }
		}

        if (currentCell)
        {
			cullingProcess->CullCellObjects(unkData, currentCell);
		}

        RE::CullJobDescriptor& cullJobDesc = cullingProcess->cullJobDesc;
		RE::NiPointer<RE::NiCamera> camera = cameraContext->camera;

        cullJobDesc.camera = camera;

        RE::BSPortalGraphEntry* portalGraphEntry = RE::Main__GetPortalGraphEntry(RE::Main::GetSingleton());
		
        if (portalGraphEntry)
        {
			RE::BSPortalGraph* portalGraph = portalGraphEntry->portalGraph;
            if (portalGraph)
            {
				cullJobDesc.cullingObjects = reinterpret_cast<RE::BSTArray<RE::NiPointer<RE::NiAVObject>>*>(&portalGraph->unk58);
				cullJobDesc.Cull(1, 0);
            }
        }

        if (mainShadowSceneChildren.capacity() > 9)
        {
			RE::NiPointer<RE::NiAVObject>& portalSharedNode = mainShadowSceneChildren[9];
			cullJobDesc.scene = portalSharedNode;
        }
		else
		{
			cullJobDesc.scene = nullptr;
		}
		cullJobDesc.Cull(0, 0);

        if (mainShadowSceneChildren.capacity() > 8)
        {
			RE::NiPointer<RE::NiAVObject>& multiBoundNode = mainShadowSceneChildren[8];
			cullJobDesc.scene = multiBoundNode;
        }
		else
		{
			cullJobDesc.scene = nullptr; 
		}
		cullJobDesc.Cull(0, 0);

		// 3. Rendering step ///////////////////////////////////////////////////////////////////////////////////////////////

        RE::BSGraphics::RenderTargetManager* renderTargetManager = RE::BSGraphics::RenderTargetManager::GetSingleton();

        int depthStencil = renderTargetManager->GetDepthStencil();
		renderTargetManager->SetupDepthStencilAt(depthStencil, 0, 0, false);

		RE::BSGraphics::SetRenderTargetMode worldRenderTargetMode = isFogOfWarEnabled ? RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR : RE::BSGraphics::SetRenderTargetMode::SRTM_RESTORE;

		renderTargetManager->SetupRenderTargetAt(0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, worldRenderTargetMode, true);
		RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);

		// 4. Post process step (Add fog of war) ///////////////////////////////////////////////////////////////////////////

		if (isFogOfWarEnabled)
		{
			shaderAccumulator->ClearActiveRenderPasses(false);

			RE::BSGraphics::BSShaderAccumulator::SetRenderMode(19);

			RE::NiPointer<RE::NiAVObject> fogOfWarOverlayHolder = cullingProcess->GetFogOfWarOverlay();
			cullJobDesc.scene = fogOfWarOverlayHolder;
			cullJobDesc.Cull(0, 0);

			RE::BSGraphics::RendererShadowState* rendererShadowState = RE::BSGraphics::RendererShadowState::GetSingleton();

			if (rendererShadowState->alphaBlendWriteMode != 8)
			{
				rendererShadowState->alphaBlendWriteMode = 8;
				rendererShadowState->stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
			}

			if (rendererShadowState->depthStencilDepthMode != RE::BSGraphics::DepthStencilDepthMode::kDisabled)
			{
				rendererShadowState->depthStencilDepthMode = RE::BSGraphics::DepthStencilDepthMode::kDisabled;
				if (rendererShadowState->depthStencilDepthModePrevious != RE::BSGraphics::DepthStencilDepthMode::kDisabled)
				{
					rendererShadowState->stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
				}
				else
				{
					rendererShadowState->stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
				}
			}

			renderTargetManager->SetupRenderTargetAt(0, RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::BSGraphics::SetRenderTargetMode::SRTM_RESTORE, true);
			RE::NiCamera__Accumulate(camera.get(), shaderAccumulator.get(), 0);

			if (rendererShadowState->depthStencilDepthMode != RE::BSGraphics::DepthStencilDepthMode::kTestWrite)
			{
				rendererShadowState->depthStencilDepthMode = RE::BSGraphics::DepthStencilDepthMode::kTestWrite;
				if (rendererShadowState->depthStencilDepthModePrevious != RE::BSGraphics::DepthStencilDepthMode::kTestWrite)
				{
					rendererShadowState->stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
				}
				else
				{
					rendererShadowState->stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
				}
			}

			if (rendererShadowState->alphaBlendWriteMode != 1)
			{
				rendererShadowState->alphaBlendWriteMode = 1;
				rendererShadowState->stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
			}
		}
		// 5. Finish rendering and dispatch ////////////////////////////////////////////////////////////////////////////////

        renderTargetManager->SetupDepthStencilAt(-1, 3, 0, false);
		RE::ImageSpaceShaderParam& imageSpaceShaderParam = cullingProcess->GetImageSpaceShaderParam();
		RE::BSGraphics::RenderTargetProperties& renderLocalMapSwapData = renderTargetManager->renderTargetData[RE::RENDER_TARGET::kLOCAL_MAP_SWAP];
		float localMapSwapWidth = renderLocalMapSwapData.width;
		float localMapSwapHeight = renderLocalMapSwapData.height;

		imageSpaceShaderParam.SetupPixelConstantGroup(0, 1.0 / localMapSwapWidth, 1.0 / localMapSwapHeight, 0.0, 0.0);
		
        RE::ImageSpaceManager* imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
		imageSpaceManager->CopyWithImageSpaceEffect(RE::ImageSpaceManager::ImageSpaceEffectEnum::ISLocalMap,
													RE::RENDER_TARGET::kLOCAL_MAP_SWAP, RE::RENDER_TARGET::kLOCAL_MAP,
													&imageSpaceShaderParam);

		if (areLODsHidden)
		{
			objectLODRoot->GetFlags().set(RE::NiAVObject::Flag::kHidden);
		}

		mainShadowSceneNode->GetRuntimeData().unk219 = unk219;
		enableWaterRendering = isWaterRenderingEnabled;
		useMapBrightnessAndContrastBoost = isUsingMapBrightnessAndContrastBoost;
		byte_1431D1D30 = isByte_1431D1D30;
		byte_141E0DC5D = byte_141E0DC5C = isByte_141E0DC5C_D;
		dword_1431D0D8C = 0;

        shaderAccumulator->ClearActiveRenderPasses(false);

		RE::RenderPassCache::CleanupThisThreadPool();

		ClearPendingRenderPasses();
	}

	// For some reason, some render passes are not cleared after rendering the minimap.
	// Get from the pool the passes used, trace them to the shader properties of the
	// geometries rendered and clear them here. This is the best I can to avoid CTD
	// because of memory leakage allocating render passes.
	void Minimap::ClearPendingRenderPasses()
	{
		std::set<RE::BSShaderProperty*> shaderProperties;

		RE::RenderPassCache::Pool& pool = RE::RenderPassCache::GetSingleton()->pools[0];
		static constexpr uint32_t poolMaxAlloc = 65535;

		for (uint32_t passIndex = 0; passIndex < poolMaxAlloc; ++passIndex)
		{
			RE::BSRenderPass* pass = &pool.passes[passIndex];
			if (pass->passEnum != 0)
			{
				shaderProperties.insert(pass->shaderProperty);
			}
		}

		for (RE::BSShaderProperty* shaderProperty : shaderProperties)
		{
			shaderProperty->DoClearRenderPasses();
		}
	}
}
