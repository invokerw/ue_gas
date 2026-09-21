#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CombatAIAssetBuilder.generated.h"

class UStateTree;
enum class ECombatAIRoleOperation : uint8;

#if WITH_EDITOR
/** 编辑器与 Automation 共用的原生树编排；不在游戏运行时动态拼树。 */
struct COMBAT_API FCombatAIAssetBuilder
{
	/** 构建只消费父作用域意图的链接执行资产，调用真实 StateTree Compiler。 */
	static UStateTree* BuildActionTree(UObject* Outer, FName Name = NAME_None);
	/** 构建 Idle → Scope(Prepare/Linked Execute/Resolve) → Scheduler Wait 的最小根树。 */
	static UStateTree* BuildRootTree(UObject* Outer, UStateTree* ActionTree, FName Name = NAME_None);
	/** 构建共享准备/执行/确认职责子树；路线通过 Scheduler 到点停留。 */
	static UStateTree* BuildRoleActionTree(UObject* Outer, ECombatAIRoleOperation Operation, FName Slot, FName Name = NAME_None);
	/** 构建等待观察或职责更新的守点子树，不重复注册感知。 */
	static UStateTree* BuildGuardTree(UObject* Outer, FName Name = NAME_None);
	/** 构建有序条件根树；Duty 可为空，其余子树共享父 Scope。 */
	static UStateTree* BuildRoleRootTree(UObject* Outer, UStateTree* Home, UStateTree* Engage, UStateTree* Duty, UStateTree* Guard, FName Name = NAME_None);
	/** 遍历实际活动路径及链接资产，拒绝并行写入、缺失 Scope、轮询转移和引擎 Delay。 */
	static bool ValidateRootTree(const UStateTree* Tree, FString& Diagnostic);
};
#endif

/** 使用 UE 原生 API 创建和保存阶段 A 演示定义；二进制资产不经过文本改写。非 Editor Target 返回失败。 */
UCLASS()
class COMBAT_API UCombatAIAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCombatAIAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
