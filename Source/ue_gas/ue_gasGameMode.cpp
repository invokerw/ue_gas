// Copyright Epic Games, Inc. All Rights Reserved.

#include "ue_gasGameMode.h"

#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"
#include "ue_gasCharacter.h"
#include "ue_gasPlayerController.h"

Aue_gasGameMode::Aue_gasGameMode()
{
	PlayerControllerClass = Aue_gasPlayerController::StaticClass();
	// 蓝图 GameMode 可继续用 DefaultPawnClass 选择玩家 Unit；SpawnDefaultPawnAtTransform 会单独返回 Command Pawn。
	DefaultPawnClass = ACombatUnitCharacter::StaticClass();
}

APawn* Aue_gasGameMode::SpawnDefaultPawnAtTransform_Implementation(
	AController* NewPlayer,
	const FTransform& SpawnTransform)
{
	Aue_gasPlayerController* CombatPlayer = Cast<Aue_gasPlayerController>(NewPlayer);
	UWorld* World = GetWorld();
	if (!CombatPlayer || !World)
	{
		UE_LOG(LogCombat, Error,
			TEXT("SAMDefaultSpawnRejected Player=%s Reason=InvalidCombatPlayerOrWorld"),
			*GetNameSafe(NewPlayer));
		return nullptr;
	}

	UClass* const ConfiguredUnitClass = DefaultPawnClass.Get();
	if (!ConfiguredUnitClass || !ConfiguredUnitClass->IsChildOf(ACombatUnitCharacter::StaticClass()))
	{
		UE_LOG(LogCombat, Error,
			TEXT("SAMDefaultSpawnRejected Player=%s Reason=DefaultPawnClassMustBeCombatUnit Class=%s"),
			*GetNameSafe(NewPlayer), *GetNameSafe(ConfiguredUnitClass));
		return nullptr;
	}

	ACombatUnitCharacter* Unit = CombatPlayer->GetCommandedUnit();
	bool bSpawnedUnit = false;
	if (!IsValid(Unit))
	{
		FActorSpawnParameters UnitParameters;
		UnitParameters.Instigator = GetInstigator();
		UnitParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Unit = Cast<ACombatUnitCharacter>(World->SpawnActor<APawn>(
			ConfiguredUnitClass, SpawnTransform, UnitParameters));
		bSpawnedUnit = Unit != nullptr;
	}
	if (!Unit)
	{
		UE_LOG(LogCombat, Error,
			TEXT("SAMDefaultSpawnRejected Player=%s Reason=CombatUnitSpawnFailed Class=%s"),
			*GetNameSafe(NewPlayer), *GetNameSafe(ConfiguredUnitClass));
		return nullptr;
	}

	TSubclassOf<Aue_gasCharacter> CommandClass = CombatPlayer->GetCommandPawnClass();
	if (!CommandClass)
	{
		CommandClass = Aue_gasCharacter::StaticClass();
	}
	FActorSpawnParameters CommandParameters;
	CommandParameters.Owner = CombatPlayer;
	CommandParameters.Instigator = GetInstigator();
	CommandParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Aue_gasCharacter* CommandPawn = World->SpawnActor<Aue_gasCharacter>(
		CommandClass, SpawnTransform, CommandParameters);
	if (!CommandPawn)
	{
		if (bSpawnedUnit)
		{
			World->DestroyActor(Unit);
		}
		UE_LOG(LogCombat, Error,
			TEXT("SAMDefaultSpawnRejected Player=%s Reason=CommandPawnSpawnFailed Class=%s"),
			*GetNameSafe(NewPlayer), *GetNameSafe(CommandClass.Get()));
		return nullptr;
	}

	// Unit 在 PlayerController Possess Command Pawn 之前完成 AI Possess 与网络 Owner 绑定，避免嵌套占有破坏 Crowd/CMC 状态。
	if (!CombatPlayer->SetCommandedUnitAuthority(Unit))
	{
		World->DestroyActor(CommandPawn);
		if (bSpawnedUnit)
		{
			World->DestroyActor(Unit);
		}
		UE_LOG(LogCombat, Error,
			TEXT("SAMDefaultSpawnRejected Player=%s Reason=CommandBindingFailed Unit=%s"),
			*GetNameSafe(NewPlayer), *GetNameSafe(Unit));
		return nullptr;
	}

	UE_LOG(LogCombat, Log,
		TEXT("SAMDefaultSpawnReady Player=%s CommandPawn=%s Unit=%s UnitController=%s ReusedUnit=%s"),
		*GetNameSafe(NewPlayer), *GetNameSafe(CommandPawn), *GetNameSafe(Unit),
		*GetNameSafe(Unit->GetController()), bSpawnedUnit ? TEXT("No") : TEXT("Yes"));
	return CommandPawn;
}
