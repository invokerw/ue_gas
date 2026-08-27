#include "Combat/Release/CombatReleaseContract.h"

#include "Combat/Core/CombatNumericPolicy.h"
#include "Combat/Core/CombatRngSubsystem.h"
#include "Combat/Core/CombatTags.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "Combat/Log/CombatEventSubsystem.h"

bool FCombatReleaseContract::IsSelfConsistent(FString& OutError) const
{
	OutError.Reset();
	auto Require = [&OutError](const bool bCondition, const TCHAR* Message)
	{
		if (!bCondition && OutError.IsEmpty())
		{
			OutError = Message;
		}
		return bCondition;
	};

	bool bValid = true;
	bValid &= Require(ContractVersion == 1, TEXT("发布契约版本必须为 1"));
	bValid &= Require(ReleaseId == TEXT("combat_v1_rc1"), TEXT("候选发布标识必须为 combat_v1_rc1"));
	bValid &= Require(ContentVersion == FCombatDefinitionRegistry::CombatContentVersion, TEXT("内容版本与 CombatDefinitionRegistry 不一致"));
	bValid &= Require(GameplayTagSchemaVersion == CombatTags::SchemaVersion, TEXT("标签结构版本与 CombatTags 不一致"));
	bValid &= Require(FormulaVersion == FCombatNumericPolicyV1::FormulaVersion, TEXT("公式版本与 CombatNumericPolicyV1 不一致"));
	bValid &= Require(RngAlgorithmVersion == FCombatRngPolicyV1::AlgorithmVersion, TEXT("随机算法版本与 Combat RNG v1 不一致"));
	bValid &= Require(EventSchemaVersion == UCombatEventSubsystem::CurrentSchemaVersion, TEXT("事件结构版本与 CombatEventSubsystem 不一致"));
	bValid &= Require(bServerAuthoritativeGameplay, TEXT("v1 必须保持服务器权威 gameplay 结算"));
	bValid &= Require(bProjectileVisualPrediction, TEXT("v1 必须保留弹体纯视觉预测协调能力"));
	bValid &= Require(!bGameplayRollback, TEXT("v1 不承诺 gameplay 预测回滚"));
	bValid &= Require(!bDeterministicReplay, TEXT("v1 不承诺跨进程确定性重放"));
	bValid &= Require(!bSummonsAndIllusions, TEXT("召唤物与幻象必须保持为 v1 后延期能力"));
	bValid &= Require(!bItemsAndEconomy, TEXT("物品与经济必须保持为 v1 后延期能力"));
	return bValid;
}

FCombatReleaseContract UCombatReleaseContractLibrary::GetCombatReleaseContract()
{
	return FCombatReleaseContract();
}

bool UCombatReleaseContractLibrary::ValidateCombatReleaseContract(
	const FCombatReleaseContract& Contract,
	FString& OutError)
{
	return Contract.IsSelfConsistent(OutError);
}
