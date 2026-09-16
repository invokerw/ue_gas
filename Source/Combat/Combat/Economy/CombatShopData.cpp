#include "Combat/Economy/CombatShopData.h"

#include "Combat/Items/CombatItemData.h"
#include "Misc/DataValidation.h"

namespace CombatShopDataPrivate
{
	constexpr int32 MaxRecipeDepth = 16;
	constexpr int32 MaxExpandedIngredients = 256;

	bool SafeAddPrice(const int64 Value, int64& Total)
	{
		if (Value < 0 || Total > MAX_int64 - Value)
		{
			return false;
		}
		Total += Value;
		return true;
	}
}

FPrimaryAssetType UCombatShopData::GetCombatPrimaryAssetType() const
{
	static const FPrimaryAssetType ShopType(TEXT("CombatShop"));
	return ShopType;
}

void UCombatShopData::GetCatalogItems(TArray<UCombatItemData*>& OutItems) const
{
	OutItems.Reset();
	TSet<FPrimaryAssetId> Seen;
	for (const FCombatShopCategory& Category : Categories)
	{
		for (const TSoftObjectPtr<UCombatItemData>& ItemReference : Category.Items)
		{
			UCombatItemData* Item = ItemReference.LoadSynchronous();
			const FPrimaryAssetId Id = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
			if (Item && Id.IsValid() && !Seen.Contains(Id))
			{
				Seen.Add(Id);
				OutItems.Add(Item);
			}
		}
	}
}

UCombatItemData* UCombatShopData::FindItem(const FPrimaryAssetId& ItemId) const
{
	if (!ItemId.IsValid()) return nullptr;
	for (const FCombatShopCategory& Category : Categories)
	{
		for (const TSoftObjectPtr<UCombatItemData>& ItemReference : Category.Items)
		{
			UCombatItemData* Item = ItemReference.LoadSynchronous();
			if (Item && Item->GetPrimaryAssetId() == ItemId) return Item;
		}
	}
	return nullptr;
}

bool UCombatShopData::CalculateItemPrice(const FPrimaryAssetId& ItemId, int64& OutPrice, FString& OutError) const
{
	OutPrice = 0;
	OutError.Reset();
	TSet<FPrimaryAssetId> Visiting;
	int32 Expanded = 0;
	TFunction<bool(UCombatItemData*, int32)> Visit = [&](UCombatItemData* Item, const int32 Depth)
	{
		if (!Item || Depth > CombatShopDataPrivate::MaxRecipeDepth || ++Expanded > CombatShopDataPrivate::MaxExpandedIngredients)
		{
			OutError = TEXT("Recipe exceeds the supported depth or expanded ingredient count");
			return false;
		}
		const FPrimaryAssetId Id = Item->GetPrimaryAssetId();
		if (!Id.IsValid() || Visiting.Contains(Id))
		{
			OutError = FString::Printf(TEXT("Recipe contains an invalid identity or cycle at %s"), *Id.ToString());
			return false;
		}
		if (Item->Recipe.IsEmpty())
		{
			if (!Item->bPurchasable || Item->PurchasePrice <= 0 || !CombatShopDataPrivate::SafeAddPrice(Item->PurchasePrice, OutPrice))
			{
				OutError = FString::Printf(TEXT("Recipe leaf is not purchasable or price overflowed: %s"), *Id.ToString());
				return false;
			}
			return true;
		}
		Visiting.Add(Id);
		for (const FCombatItemRecipeIngredient& Ingredient : Item->Recipe)
		{
			UCombatItemData* Child = Ingredient.Item.LoadSynchronous();
			for (int32 Index = 0; Index < Ingredient.Quantity; ++Index)
			{
				if (!Visit(Child, Depth + 1))
				{
					Visiting.Remove(Id);
					return false;
				}
			}
		}
		Visiting.Remove(Id);
		return true;
	};
	return Visit(FindItem(ItemId), 0);
}

bool UCombatShopData::BuildPurchasePlan(const FPrimaryAssetId& ItemId,
	const TArray<FPrimaryAssetId>& OwnedDefinitions, FCombatPurchasePlan& OutPlan, FString& OutError) const
{
	OutPlan = FCombatPurchasePlan();
	OutError.Reset();
	UCombatItemData* Target = FindItem(ItemId);
	if (!Target)
	{
		OutError = FString::Printf(TEXT("Target is not present in the shop catalog: %s"), *ItemId.ToString());
		return false;
	}
	TMap<FPrimaryAssetId, int32> RemainingOwned;
	for (const FPrimaryAssetId& Owned : OwnedDefinitions)
	{
		if (Owned.IsValid()) ++RemainingOwned.FindOrAdd(Owned);
	}
	TSet<FPrimaryAssetId> Visiting;
	int32 Expanded = 0;
	TFunction<bool(UCombatItemData*, int32)> Acquire = [&](UCombatItemData* Item, const int32 Depth)
	{
		if (!Item || Depth > CombatShopDataPrivate::MaxRecipeDepth || ++Expanded > CombatShopDataPrivate::MaxExpandedIngredients)
		{
			OutError = TEXT("Purchase plan exceeds the supported recipe bounds");
			return false;
		}
		const FPrimaryAssetId Id = Item->GetPrimaryAssetId();
		if (!Id.IsValid() || !FindItem(Id))
		{
			OutError = FString::Printf(TEXT("Purchase plan references an item outside the catalog: %s"), *Id.ToString());
			return false;
		}
		if (int32* Count = RemainingOwned.Find(Id); Count && *Count > 0)
		{
			--*Count;
			OutPlan.ConsumedDefinitions.Add(Id);
			return true;
		}
		if (Item->Recipe.IsEmpty())
		{
			if (!Item->bPurchasable || Item->PurchasePrice <= 0
				|| !CombatShopDataPrivate::SafeAddPrice(Item->PurchasePrice, OutPlan.TotalCost))
			{
				OutError = FString::Printf(TEXT("Missing recipe leaf cannot be purchased: %s"), *Id.ToString());
				return false;
			}
			OutPlan.PurchasedDefinitions.Add(Id);
			return true;
		}
		if (Visiting.Contains(Id))
		{
			OutError = FString::Printf(TEXT("Purchase plan contains a recipe cycle at %s"), *Id.ToString());
			return false;
		}
		Visiting.Add(Id);
		for (const FCombatItemRecipeIngredient& Ingredient : Item->Recipe)
		{
			UCombatItemData* Child = Ingredient.Item.LoadSynchronous();
			for (int32 Index = 0; Index < Ingredient.Quantity; ++Index)
			{
				if (!Acquire(Child, Depth + 1))
				{
					Visiting.Remove(Id);
					return false;
				}
			}
		}
		Visiting.Remove(Id);
		return true;
	};

	// 购买基础物品必须总是创建一件新物品；已有同名物品只用于升级配方抵扣，不能把重复购买变成零金币重建。
	if (Target->Recipe.IsEmpty())
	{
		if (!Target->bPurchasable || Target->PurchasePrice <= 0
			|| !CombatShopDataPrivate::SafeAddPrice(Target->PurchasePrice, OutPlan.TotalCost))
		{
			OutError = FString::Printf(TEXT("Shop leaf cannot be purchased: %s"), *ItemId.ToString());
			return false;
		}
		OutPlan.PurchasedDefinitions.Add(ItemId);
		return true;
	}
	Visiting.Add(ItemId);
	for (const FCombatItemRecipeIngredient& Ingredient : Target->Recipe)
	{
		UCombatItemData* Child = Ingredient.Item.LoadSynchronous();
		for (int32 Index = 0; Index < Ingredient.Quantity; ++Index)
		{
			if (!Acquire(Child, 1))
			{
				return false;
			}
		}
	}
	return true;
}

bool UCombatShopData::ValidateRuntime(FString& OutError) const
{
	OutError.Reset();
	if (!GetPrimaryAssetId().IsValid() || Categories.IsEmpty())
	{
		OutError = TEXT("Shop requires a valid identity and at least one category");
		return false;
	}
	TSet<FName> CategoryIds;
	TSet<FPrimaryAssetId> ItemIds;
	TArray<UCombatItemData*> Catalog;
	for (const FCombatShopCategory& Category : Categories)
	{
		if (!UCombatDefinitionData::IsValidDefinitionName(Category.CategoryId) || CategoryIds.Contains(Category.CategoryId)
			|| Category.Items.IsEmpty())
		{
			OutError = TEXT("Shop categories require unique lower_snake_case ids and non-empty item lists");
			return false;
		}
		CategoryIds.Add(Category.CategoryId);
		for (const TSoftObjectPtr<UCombatItemData>& ItemReference : Category.Items)
		{
			UCombatItemData* Item = ItemReference.LoadSynchronous();
			FString ItemError;
			const FPrimaryAssetId Id = Item ? Item->GetPrimaryAssetId() : FPrimaryAssetId();
			if (!Item || !Item->ValidateRuntime(ItemError) || !Id.IsValid() || ItemIds.Contains(Id))
			{
				OutError = FString::Printf(TEXT("Shop contains an invalid or duplicate item: %s (%s)"), *Id.ToString(), *ItemError);
				return false;
			}
			ItemIds.Add(Id);
			Catalog.Add(Item);
		}
	}
	for (UCombatItemData* Item : Catalog)
	{
		for (const FCombatItemRecipeIngredient& Ingredient : Item->Recipe)
		{
			UCombatItemData* Child = Ingredient.Item.LoadSynchronous();
			if (!Child || !ItemIds.Contains(Child->GetPrimaryAssetId()))
			{
				OutError = FString::Printf(TEXT("Recipe component is outside the catalog: %s"), *Item->GetPrimaryAssetId().ToString());
				return false;
			}
		}
		int64 Price = 0;
		if (!CalculateItemPrice(Item->GetPrimaryAssetId(), Price, OutError)) return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UCombatShopData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Base = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateRuntime(Error))
	{
		Context.AddError(FText::FromString(Error));
		return EDataValidationResult::Invalid;
	}
	return Base;
}
#endif
