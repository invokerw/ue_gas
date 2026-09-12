#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

class UWorld;

/** 自动化测试的临时 World 持有者：构造时创建，离开作用域时依次结束 Actor、清理网络驱动并销毁 World；不代表已启动真实多人连接或完整编辑器 PIE 会话。 */
class FCombatAutomationWorldFixture
{
public:
	explicit FCombatAutomationWorldFixture(ENetMode RequestedNetMode = NM_Standalone);
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
