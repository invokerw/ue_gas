#include "Combat/Tests/CombatAINetworkScenario.h"
#include "Combat/AI/CombatAIBrainComponent.h"
#include "Combat/AI/CombatAIProfileData.h"
#include "Combat/Attack/CombatAttackComponent.h"
#include "Combat/Demo/CombatAIDemoArena.h"
#include "Combat/Order/CombatOrderComponent.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

ACombatAINetworkScenario::ACombatAINetworkScenario() { PrimaryActorTick.bCanEverTick = true; }

void ACombatAINetworkScenario::Finish(bool bPassed, const FString& Detail)
{
	const FString Result = FString::Printf(TEXT("AINetworkSmoke Result=%s NetMode=%d %s"), bPassed ? TEXT("Pass") : TEXT("Fail"), int32(GetNetMode()), *Detail);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Result);
	FString Report;
	if (FParse::Value(FCommandLine::Get(), TEXT("CombatAIReport="), Report) && !Report.Contains(TEXT("/")) && !Report.Contains(TEXT("\\")) && !Report.Contains(TEXT("..")))
	{
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("AI-002");
		IFileManager::Get().MakeDirectory(*Directory, true);
		FFileHelper::SaveStringToFile(Result, *(Directory / (Report + TEXT(".txt"))));
	}
	SetActorTickEnabled(false);
}

void ACombatAINetworkScenario::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	if (Elapsed > 55) { Finish(false, TEXT("Timed out waiting for AI state/replication")); return; }
	if (Elapsed < 3) return;
	if (!Arena.IsValid())
	{
		for (TActorIterator<ACombatAIDemoArena> It(GetWorld()); It; ++It) { Arena = *It; break; }
		if (!Arena.IsValid() && HasAuthority() && GetNetMode() != NM_Client)
		{
			const FTransform Transform(FRotator::ZeroRotator, FVector(-450, -450, 110));
			auto* Created = GetWorld()->SpawnActorDeferred<ACombatAIDemoArena>(ACombatAIDemoArena::StaticClass(), Transform);
			Created->Profile = LoadObject<UCombatAIProfileData>(nullptr, TEXT("/Game/Combat/Demo/AI/DA_CombatAI_Basic.DA_CombatAI_Basic"));
			Created->AgentClass = LoadClass<ACombatUnitCharacter>(nullptr, TEXT("/Game/Combat/Demo/Heros/DrowRanger/BP_DrowRanger.BP_DrowRanger_C"));
			Created->TargetClass = LoadClass<ACombatUnitCharacter>(nullptr, TEXT("/Game/Combat/Demo/Heros/WoodenDummy/BP_WoodenDummy.BP_WoodenDummy_C"));
			Created->FinishSpawning(Transform);
			Arena = Created;
		}
	}
	if (!Arena.IsValid() || !IsValid(Arena->Agent) || !IsValid(Arena->Target)) return;
	auto* Unit = Arena->Agent.Get();
	auto* Brain = Unit->GetCombatAIBrainComponent();
	if (GetNetMode() == NM_Client)
	{
		if (Elapsed < 15) return;
		Finish(!Brain->IsRunning() && Brain->GetRunSerial() == 0 && !Unit->HasAuthority(),
			FString::Printf(TEXT("ClientBrain=Stopped Unit=%s Life=%u Position=%s"), *Unit->GetName(), Unit->GetLifeGeneration(), *Unit->GetActorLocation().ToCompactString()));
		return;
	}
	bObservedServerOrder |= Brain->IsRunning() && Unit->GetCombatAttackComponent()->GetLastFinalizedResult().AppliedDamage > 0
		&& FVector::Dist2D(Unit->GetActorLocation(), Arena->GetReturnLocation()) > 100;
	if (!bReturnRequested && Elapsed > 9 && bObservedServerOrder)
	{
		SubmittedBeforeReturn = Brain->GetSubmittedCount();
		Arena->RequestReturn();
		bReturnRequested = true;
	}
	if (Elapsed > 17 && bReturnRequested && Brain->GetSubmittedCount() > SubmittedBeforeReturn && Brain->GetResolvedCount() >= 2)
	{
		Finish(Brain->IsRunning() && Brain->GetLastReceipt().Result.bSuccess && !Unit->GetCombatOrderComponent()->GetCurrentOrderHandle().IsValid()
			&& FVector::Dist2D(Unit->GetActorLocation(), Arena->GetReturnLocation()) < 100,
			FString::Printf(TEXT("ServerBrain=Running AttackLandedAndMoved=%d Submitted=%llu Resolved=%llu MoveSuccess=%d"),
				bObservedServerOrder, Brain->GetSubmittedCount(), Brain->GetResolvedCount(), Brain->GetLastReceipt().Result.bSuccess));
	}
}
