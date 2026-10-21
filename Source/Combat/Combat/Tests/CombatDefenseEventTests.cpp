#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/AssetManager.h"
#include "Combat/Defense/DefenseData.h"
#include "Combat/Defense/DefenseEventData.h"
#include "Combat/Validation/CombatAssetValidationCommandlet.h"
/** 冷进程验证真实资产、扫描身份与配置值；缺失正式资产必须红灯，不能落回瞬态原型。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseEventAssetsTest,"Combat.Defense.Event.AuthoredAssetsAndScan",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefenseEventAssetsTest::RunTest(const FString&)
{
 auto* Run=LoadObject<UDefenseRunData>(nullptr,TEXT("/Game/Combat/Demo/Defense/DA_DefenseRun.DA_DefenseRun"));
 if(!TestNotNull(TEXT("Authored run"),Run)) return false;
 if(!TestEqual(TEXT("Two formal event references"),Run->Events.Num(),2)) return false;
 FString Error;TestTrue(TEXT("Run definition valid"),Run->ValidateRuntime(Error));
 TArray<FPrimaryAssetId> Scanned;UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("DefenseEvent")),Scanned);
 TestEqual(TEXT("Two event identities scanned"),Scanned.Num(),2);
 for(int32 Index=0;Index<2;++Index)
 {
  auto* Event=Run->Events[Index].Get();if(!TestNotNull(TEXT("Formal event asset"),Event)) continue;
  TestFalse(TEXT("Event has saved package"),Event->GetOutermost()==GetTransientPackage());
  TestTrue(TEXT("Valid event values"),Event->ValidateRuntime(Error));TestEqual(TEXT("Trigger wave"),Event->TriggerAfterWave,2+2*Index);
  TestTrue(TEXT("Stable event identity registered"),Scanned.Contains(Event->GetPrimaryAssetId()));
  TestEqual(TEXT("AssetManager resolves formal path"),UAssetManager::Get().GetPrimaryAssetPath(Event->GetPrimaryAssetId()).ToString(),Event->GetPathName());
 }
 TestEqual(TEXT("Vault safe gold"),Run->Events[0]->SafeGold,240);TestEqual(TEXT("Vault safe XP"),Run->Events[0]->SafeExperience,60);
 TestTrue(TEXT("Oath heals twelve percent"),FMath::IsNearlyEqual(Run->Events[1]->SafeCoreHealFraction,.12f));
 auto Report=FCombatAssetValidator::ValidateProjectAssets();TestTrue(TEXT("Asset scan includes new definitions"),Report.ScannedAssets>=146);TestEqual(TEXT("No asset errors"),Report.ErrorCount,0);
 return true;
}
#endif
