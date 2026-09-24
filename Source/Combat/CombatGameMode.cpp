// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatGameMode.h"

#include "Combat/Log/CombatEventSubsystem.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"
#include "Combat/Economy/CombatEconomyComponent.h"
#include "Combat/Economy/CombatEconomyData.h"
#include "Combat/Economy/CombatShopData.h"
#include "Combat/Tests/CombatEconomyNetworkScenario.h"
#include "EngineUtils.h"
#include "Misc/Parse.h"

ACombatGameMode::ACombatGameMode()
{
	PlayerControllerClass = ACombatPlayerController::StaticClass();
	// 蓝图 GameMode 可继续用 DefaultPawnClass 选择玩家 Unit；SpawnDefaultPawnAtTransform 会单独返回 Command Pawn。
	DefaultPawnClass = ACombatUnitCharacter::StaticClass();
}

void ACombatGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ACombatPlayerController* CombatPlayer = Cast<ACombatPlayerController>(NewPlayer);
	if (!CombatPlayer || !EconomyData || !ShopData) return;
	FString Error;
	if (!CombatPlayer->GetCombatEconomyComponent()->InitializeForMatch(
		EconomyData, ShopData, bEnableEconomyDebugCommands, Error))
	{
		UE_LOG(LogCombat, Error, TEXT("EconomyPlayerInitializationFailed Player=%s Error=%s"),
			*GetNameSafe(CombatPlayer), *Error);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("CombatEconomySmoke")) && GetWorld())
	{
		CombatPlayer->GetCombatEconomyComponent()->AddGold(5000, FName(TEXT("EconomySmokeFixture")));
		bool bScenarioExists = false;
		for (TActorIterator<ACombatEconomyNetworkScenario> It(GetWorld()); It; ++It)
		{
			bScenarioExists = true;
			break;
		}
		if (!bScenarioExists)
		{
			if (ACombatEconomyNetworkScenario* Scenario = GetWorld()->SpawnActor<ACombatEconomyNetworkScenario>())
			{
				Scenario->SetReplicates(true);
				Scenario->bAlwaysRelevant = true;
			}
		}
	}
}

APawn* ACombatGameMode::SpawnDefaultPawnAtTransform_Implementation(
	AController* NewPlayer,
	const FTransform& SpawnTransform)
{
	ACombatPlayerController* CombatPlayer = Cast<ACombatPlayerController>(NewPlayer);
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

	TSubclassOf<ACombatCharacter> CommandClass = CombatPlayer->GetCommandPawnClass();
	if (!CommandClass)
	{
		CommandClass = ACombatCharacter::StaticClass();
	}
	FActorSpawnParameters CommandParameters;
	CommandParameters.Owner = CombatPlayer;
	CommandParameters.Instigator = GetInstigator();
	CommandParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatCharacter* CommandPawn = World->SpawnActor<ACombatCharacter>(
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
	if (bSpawnedUnit)
	{
		for (int32 Index = 0; Index < FMath::Clamp(AdditionalControlledUnitCount, 0, 7); ++Index)
		{
			FActorSpawnParameters ExtraParameters;
			ExtraParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FVector Offset(0, 180.0f * (Index + 1), 0);
			auto* Extra = World->SpawnActor<ACombatUnitCharacter>(ConfiguredUnitClass,
				SpawnTransform.GetLocation() + Offset, SpawnTransform.Rotator(), ExtraParameters);
			if (Extra && !CombatPlayer->GrantUnitControlAuthority(Extra)) World->DestroyActor(Extra);
		}
	}
	return CommandPawn;
}
