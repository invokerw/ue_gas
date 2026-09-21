#include "Combat/Validation/CombatNavigationBuildCommandlet.h"

#include "Misc/Parse.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#include "NavigationSystem.h"
#endif

UCombatNavigationBuildCommandlet::UCombatNavigationBuildCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCombatNavigationBuildCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogTemp, Error, TEXT("CombatNavigationBuild requires an Editor target."));
	return 1;
#else
	FString MapPath;
	if (!FParse::Value(*Params, TEXT("Map="), MapPath) || MapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CombatNavigationBuild requires -Map=/Game/.../Map."));
		return 1;
	}

	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("CombatNavigationBuild failed to load map %s"), *MapPath);
		return 1;
	}

	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation)
	{
		UE_LOG(LogTemp, Error, TEXT("CombatNavigationBuild found no navigation system Map=%s"), *MapPath);
		return 1;
	}

	// Commandlet 没有编辑器帧来解除异步载入锁。先完成资产载入/碰撞编译，
	// 再释放对应锁并同步构建；不能在几何体未就绪时保存空网格。
	FlushAsyncLoading();
	Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,
		UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
	Navigation->Build();

	FVector ProbePoint = FVector::ZeroVector;
	FString ProbeString;
	if (FParse::Value(*Params, TEXT("Probe="), ProbeString) && !ProbePoint.InitFromString(ProbeString))
	{
		UE_LOG(LogTemp, Error, TEXT("CombatNavigationBuild invalid Probe; expected X=0,Y=0,Z=0."));
		return 1;
	}
	FNavLocation Probe;
	const bool bProjected = Navigation->ProjectPointToNavigation(ProbePoint, Probe, FVector(100, 100, 250));
	const bool bSaved = bProjected && UEditorLoadingAndSavingUtils::SaveMap(World, MapPath)
		&& FEditorFileUtils::SaveMapDataPackages(World, true);
	UE_LOG(LogTemp, Display, TEXT("CombatNavigationBuild Map=%s Projected=%d Point=%s Saved=%d"),
		*MapPath, bProjected ? 1 : 0, *Probe.Location.ToCompactString(), bSaved ? 1 : 0);
	return bSaved ? 0 : 1;
#endif
}
