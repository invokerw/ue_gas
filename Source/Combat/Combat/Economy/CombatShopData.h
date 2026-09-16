#pragma once

#include "CoreMinimal.h"
#include "Combat/Data/CombatDefinitionData.h"
#include "CombatShopData.generated.h"

class UCombatItemData;

/** 商店的两个固定页面；分类与物品顺序仍完全由关卡目录资产配置。 */
UENUM(BlueprintType)
enum class ECombatShopPage : uint8
{
	Basic UMETA(DisplayName="基础物品"),
	Upgrade UMETA(DisplayName="升级物品")
};

/** 唯一商店中的一个展示分类。 */
USTRUCT(BlueprintType)
struct COMBAT_API FCombatShopCategory
{
	GENERATED_BODY()

	/** lower_snake_case 稳定分类键，只用于本目录内排序和诊断。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="分类 ID", ToolTip="本商店内唯一的 lower_snake_case 分类键。"))
	FName CategoryId;
	/** 玩家看到的本地化分类名称。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="分类名称", ToolTip="显示在分类物品网格上方的本地化名称。"))
	FText DisplayName;
	/** 分类所在的基础或升级页。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="商店页面", ToolTip="决定该分类显示在基础物品页还是升级物品页。"))
	ECombatShopPage Page = ECombatShopPage::Basic;
	/** 当前页面内从小到大的显示顺序，同值按分类 ID 排序。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="显示顺序", ToolTip="当前页面内从小到大的显示顺序；同值按分类 ID 稳定排序。"))
	int32 SortOrder = 0;
	/** 当前分类中的物品，数组顺序即网格顺序。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="物品", ToolTip="当前分类内可搜索和购买的物品；同一物品不能出现在多个分类。"))
	TArray<TSoftObjectPtr<UCombatItemData>> Items;
};

/** 购买升级物品前由服务器生成的不可变计划。 */
struct COMBAT_API FCombatPurchasePlan
{
	/** 需要从当前合成域消耗的已有定义；重复项表示重复数量。 */
	TArray<FPrimaryAssetId> ConsumedDefinitions;
	/** 需要用金币补购的叶子定义；重复项表示重复数量。 */
	TArray<FPrimaryAssetId> PurchasedDefinitions;
	/** 本次只对缺失叶子收费的总价。 */
	int64 TotalCost = 0;
};

/**
 * 一个关卡使用的唯一商店目录。目录只描述基础/升级页、分类和可购买定义；
 * 服务器使用同一资产解析价格与配方，客户端搜索结果不能改变结算。
 */
UCLASS(BlueprintType)
class COMBAT_API UCombatShopData : public UCombatDefinitionData
{
	GENERATED_BODY()

public:
	/** 基础/升级页中的有序分类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Shop", meta=(DisplayName="商品分类", ToolTip="唯一商店的分类及物品列表；分类和物品都必须唯一。", TitleProperty="CategoryId"))
	TArray<FCombatShopCategory> Categories;

	virtual FPrimaryAssetType GetCombatPrimaryAssetType() const override;
	/** 校验分类、唯一物品、配方闭包、环和递归价格。 */
	bool ValidateRuntime(FString& OutError) const;
	/** 解析目录中的稳定物品 ID；缺失或未加载返回空。 */
	UCombatItemData* FindItem(const FPrimaryAssetId& ItemId) const;
	/** 返回去重后的目录物品，保持页、分类、数组的配置顺序。 */
	void GetCatalogItems(TArray<UCombatItemData*>& OutItems) const;
	/** 递归计算一个物品的完整定义价格；环、溢出或不可购买叶子失败。 */
	bool CalculateItemPrice(const FPrimaryAssetId& ItemId, int64& OutPrice, FString& OutError) const;
	/** 优先消费 OwnedDefinitions，再为缺失叶子计价；不修改输入。 */
	bool BuildPurchasePlan(const FPrimaryAssetId& ItemId, const TArray<FPrimaryAssetId>& OwnedDefinitions,
		FCombatPurchasePlan& OutPlan, FString& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
