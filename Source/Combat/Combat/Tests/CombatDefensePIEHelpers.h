#pragma once
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "NavigationSystem.h"

/** 真实地图交互测试先等导航可投射；计时从命令首次执行开始，排除前序地图载入耗时。 */
class FDefensePIENavigationReady : public IAutomationLatentCommand
{
public:
 explicit FDefensePIENavigationReady(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(Started==0) Started=FPlatformTime::Seconds();
  auto* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
  auto* Nav=W?FNavigationSystem::GetCurrent<UNavigationSystemV1>(W):nullptr;
  FNavLocation Point;
  if(Nav&&!UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(W)&&Nav->ProjectPointToNavigation(FVector::ZeroVector,Point,FVector(300,300,500))) return true;
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Defense PIE requires unlocked, projectable navigation")); return true; }
  return false;
 }
private:
 FAutomationTestBase* Test;
 double Started=0;
};
#endif
