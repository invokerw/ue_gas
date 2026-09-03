#include "Combat/UI/CombatOverheadWidgetComponent.h"

#include "Combat/UI/CombatOverheadWidget.h"
#include "Combat/Unit/CombatUnitCharacter.h"
#include "Engine/World.h"

UCombatOverheadWidgetComponent::UCombatOverheadWidgetComponent()
{
	SetIsReplicatedByDefault(true);
	SetWidgetClass(UCombatOverheadWidget::StaticClass());
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawSize(FVector2D(250.0f, 180.0f));
	SetPivot(FVector2D(0.5f, 1.0f));
	SetDrawAtDesiredSize(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

void UCombatOverheadWidgetComponent::ShowDamageNumber(
	const float AppliedAmount,
	const ECombatDamageType DamageType)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !FMath::IsFinite(AppliedAmount)
		|| AppliedAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ECombatFloatingTextType Type = ECombatFloatingTextType::PhysicalDamage;
	switch (DamageType)
	{
	case ECombatDamageType::Magical:
		Type = ECombatFloatingTextType::MagicalDamage;
		break;
	case ECombatDamageType::Pure:
		Type = ECombatFloatingTextType::PureDamage;
		break;
	case ECombatDamageType::Physical:
	default:
		break;
	}
	MulticastShowFloatingText(AppliedAmount, Type);
}

void UCombatOverheadWidgetComponent::ShowHealingNumber(const float AppliedAmount)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !FMath::IsFinite(AppliedAmount)
		|| AppliedAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	MulticastShowFloatingText(AppliedAmount, ECombatFloatingTextType::Healing);
}

void UCombatOverheadWidgetComponent::InitWidget()
{
	Super::InitWidget();
	// WidgetComponent 也会为编辑器预览创建临时 Widget；这里只允许游戏 World 绑定，
	// 避免预览对象通过动态委托进入 World Partition External Actor 的保存依赖。
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (UCombatOverheadWidget* OverheadWidget = Cast<UCombatOverheadWidget>(GetUserWidgetObject()))
	{
		OverheadWidget->InitializeForUnit(Cast<ACombatUnitCharacter>(GetOwner()));
	}
}

void UCombatOverheadWidgetComponent::BeginPlay()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetVisibility(false);
	}
	Super::BeginPlay();
}

void UCombatOverheadWidgetComponent::MulticastShowFloatingText_Implementation(
	const float Amount,
	const ECombatFloatingTextType Type)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!GetUserWidgetObject())
	{
		InitWidget();
	}
	if (UCombatOverheadWidget* OverheadWidget = Cast<UCombatOverheadWidget>(GetUserWidgetObject()))
	{
		OverheadWidget->AddFloatingText(Amount, Type);
	}
}
