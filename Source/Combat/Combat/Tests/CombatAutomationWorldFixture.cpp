#include "Combat/Tests/CombatAutomationWorldFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/Package.h"

FCombatAutomationWorldFixture::FCombatAutomationWorldFixture(const ENetMode RequestedNetMode)
{
#if WITH_EDITOR
	// Editor 自动化选用 PIE 类型，使网络模式判断和 World 子系统创建走对应分支；测试仍需自行设置连接、推进帧或派发生命周期。
	const EWorldType::Type WorldType = EWorldType::PIE;
#else
	const EWorldType::Type WorldType = EWorldType::Game;
#endif
	// PIE World 会给其 Outer Package 添加 PKG_PlayInEditor。不能使用全局 TransientPackage，
	// 否则一次自动化测试就会把 /Engine/Transient 中的所有预览对象标记成 PIE 对象，
	// 导致下一次停止 PIE 时被误判为 World 泄漏。
	UPackage* const WorldPackage = CreatePackage(nullptr);
	WorldPackage->SetFlags(RF_Transient);
	const FName WorldName = MakeUniqueObjectName(
		WorldPackage, UWorld::StaticClass(), TEXT("CombatAutomationWorld"), EUniqueObjectNameOptions::GloballyUnique);
	World = UWorld::CreateWorld(WorldType, false, WorldName, WorldPackage);
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
	UPackage* const WorldPackage = World->GetOutermost();

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
#if WITH_EDITOR
	// 测试 Package 可能存活到下一次 GC；提前移除 PIE 身份，避免真实 PIE teardown 将其当成泄漏。
	WorldPackage->ClearPackageFlags(PKG_PlayInEditor);
	WorldPackage->SetPIEInstanceID(INDEX_NONE);
#endif
	World = nullptr;
}

#endif
