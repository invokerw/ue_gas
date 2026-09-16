#pragma once

#include "CoreMinimal.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "CombatEconomyData.generated.h"

/**
 * 一个关卡/对局的经济规则。关卡 GameMode 在服务器初始化时选择并冻结这些值；
 * 运行中的组件不会重新读取资产，因而编辑资产或异步加载不会改变已开始的对局。
 */
UCLASS(BlueprintType)
class COMBAT_API UCombatEconomyData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 单个玩家可持有的金币上限。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Economy", meta=(ClampMin="1", DisplayName="金币上限", ToolTip="本关卡每个玩家可持有的最大金币；所有收入在服务器限制到该值。"))
	int64 GoldCap = 99999;

	/** 玩家加入本局时获得的金币。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Economy", meta=(ClampMin="0", DisplayName="起始金币", ToolTip="玩家首次初始化本局经济组件时获得的金币，不因死亡或复活重复发放。"))
	int64 StartingGold = 600;

	/** 每个完整游戏分钟发放的金币。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Economy", meta=(ClampMin="0", DisplayName="每分钟被动金币", ToolTip="服务器每经过 60 秒游戏时间发放的金币；暂停期间不增长。"))
	int64 PassiveGoldPerMinute = 100;

	/** 未使用购买链可按实付金额退款的时间。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Economy", meta=(ClampMin="0", Units="s", DisplayName="全额退款时间", ToolTip="购买后在该秒数内、且购买链未使用或破坏时，出售返还实际支付金币；0 表示关闭全额退款。"))
	float FullRefundSeconds = 10.0f;

	/** 超出全额退款窗口后的出售比例，以万分比表示。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Economy", meta=(ClampMin="0", ClampMax="10000", DisplayName="普通出售万分比", ToolTip="超出全额退款窗口后按定义总价返还的万分比；5000 表示 50%。"))
	int32 SellValueBasisPoints = 5000;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** 校验本局数值边界；失败时不会初始化玩家经济组件。 */
	bool ValidateRuntime(FString& OutError) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
