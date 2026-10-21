#include "Combat/Defense/DefensePlayerController.h"
#include "Combat/Defense/DefenseChoiceComponent.h"
#include "Combat/Defense/DefenseHeroSelectionComponent.h"
#include "Combat/Defense/DefenseEventComponent.h"
#include "Combat/Defense/DefenseHUD.h"
#include "Combat/Defense/DefenseBuildWidget.h"
#include "Framework/Application/SlateApplication.h"
ADefensePlayerController::ADefensePlayerController()
{
	HeroSelection=CreateDefaultSubobject<UDefenseHeroSelectionComponent>(TEXT("DefenseHeroSelection"));
	Choice=CreateDefaultSubobject<UDefenseChoiceComponent>(TEXT("DefenseChoice"));
	EventComponent=CreateDefaultSubobject<UDefenseEventComponent>(TEXT("DefenseEvent"));
}
bool ADefensePlayerController::IsPointerOverCombatUI() const
{
	if(Super::IsPointerOverCombatUI()) return true;
	const auto* H=Cast<ADefenseHUD>(GetHUD());
	return H && H->GetBuildWidget() && FSlateApplication::IsInitialized()
		&& H->GetBuildWidget()->IsScreenPositionOverUI(FSlateApplication::Get().GetCursorPos());
}
