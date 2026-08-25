#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class UWorld;

/** 为 Combat 自动化创建并完整销毁一个真实 UWorld 的 RAII fixture。 */
class FCombatAutomationWorldFixture
{
public:
	/** 创建指定 NetMode 的最小 Game World，并初始化 World Subsystem。 */
	explicit FCombatAutomationWorldFixture(ENetMode RequestedNetMode = NM_Standalone);
	/** 销毁 World、清理 Context，并触发 Subsystem teardown。 */
	~FCombatAutomationWorldFixture();

	/** 禁止复制，确保 World 只有一个 fixture owner。 */
	FCombatAutomationWorldFixture(const FCombatAutomationWorldFixture&) = delete;
	/** 禁止复制赋值，避免重复销毁 World。 */
	FCombatAutomationWorldFixture& operator=(const FCombatAutomationWorldFixture&) = delete;

	/** 返回 fixture 持有的测试 World。 */
	UWorld* GetWorld() const { return World; }
	/** 返回 World 是否已成功创建。 */
	bool IsValid() const { return World != nullptr; }

private:
	/** fixture 唯一持有的临时测试 World。 */
	TObjectPtr<UWorld> World;
};

#endif
