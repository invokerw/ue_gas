#include "Combat/Tests/CombatAutomationWorldFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/World.h"

FCombatAutomationWorldFixture::FCombatAutomationWorldFixture(const ENetMode RequestedNetMode)
{
#if WITH_EDITOR
	// Editor 自动化使用 PIE WorldType，确保 NetMode 和 WorldSubsystem 路径与真实 PIE 一致。
	const EWorldType::Type WorldType = EWorldType::PIE;
#else
	const EWorldType::Type WorldType = EWorldType::Game;
#endif
	const FName WorldName = MakeUniqueObjectName(
		GetTransientPackage(), UWorld::StaticClass(), TEXT("CombatAutomationWorld"), EUniqueObjectNameOptions::GloballyUnique);
	World = UWorld::CreateWorld(WorldType, false, WorldName, GetTransientPackage());
	if (!World)
	{
		return;
	}

	FWorldContext& Context = GEngine->CreateNewWorldContext(WorldType);
	World->AddToRoot();
	Context.SetCurrentWorld(World);
#if WITH_EDITOR
	World->SetPlayInEditorInitialNetMode(RequestedNetMode);
#else
	(void)RequestedNetMode;
#endif
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
}

FCombatAutomationWorldFixture::~FCombatAutomationWorldFixture()
{
	if (!World)
	{
		return;
	}

	// 先显式路由 EndPlay，再销毁 NetDriver/World，覆盖生产 teardown 的清理顺序。
	if (World->AreActorsInitialized())
	{
		for (AActor* Actor : FActorRange(World))
		{
			if (Actor)
			{
				Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
			}
		}
	}
	GEngine->ShutdownWorldNetDriver(World);
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
	World = nullptr;
}

#endif
