// Copyright Epic Games, Inc. All Rights Reserved.


#include "TwinStickAoEAttack.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

#include "Combat/Scheduling/CombatSchedulerSubsystem.h"

ATwinStickAoEAttack::ATwinStickAoEAttack()
{
	PrimaryActorTick.bCanEverTick = false;

	// 创建根场景组件。
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 创建无碰撞的 AoE 表现网格。
	SphereVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sphere Visual"));
	SphereVisual->SetupAttachment(RootComponent);

	SphereVisual->SetCollisionProfileName(FName("NoCollision"));

	// 范围球只保留给表现或蓝图读取，不负责 Combat 命中。
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Sphere"));
	CollisionSphere->SetupAttachment(RootComponent);

	CollisionSphere->SetSphereRadius(750.0f);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

}

void ATwinStickAoEAttack::BeginPlay()
{
	Super::BeginPlay();
	
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		// 模板表现也复用统一时钟，避免继续示范 Actor Timer gameplay。
		StartAoESchedule = Scheduler->ScheduleOnce(
			this, StartAoETime, 0,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this](const FCombatScheduledTickContext& Context) { StartAoE(Context); }));
		StopAoESchedule = Scheduler->ScheduleOnce(
			this, StopAoETime, 0,
			FCombatScheduledDelegate::CreateWeakLambda(this,
				[this](const FCombatScheduledTickContext& Context) { StopAoE(Context); }));
	}
}

void ATwinStickAoEAttack::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>() : nullptr)
	{
		Scheduler->Cancel(StartAoESchedule);
		Scheduler->Cancel(StopAoESchedule);
	}
	StartAoESchedule = FCombatScheduleHandle();
	StopAoESchedule = FCombatScheduleHandle();
	Super::EndPlay(EndPlayReason);
}

void ATwinStickAoEAttack::StartAoE(const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	StartAoESchedule = FCombatScheduleHandle();
	// 这里只切换表现状态；实际 AoE 目标查询和伤害由 CombatThinkerSubsystem 完成。
	bIsAoEActive = true;
}

void ATwinStickAoEAttack::StopAoE(const FCombatScheduledTickContext& TickContext)
{
	(void)TickContext;
	StopAoESchedule = FCombatScheduleHandle();
	// 关闭表现状态。
	bIsAoEActive = false;
	if (UCombatSchedulerSubsystem* Scheduler = GetWorld()->GetSubsystem<UCombatSchedulerSubsystem>())
	{
		Scheduler->Cancel(StartAoESchedule);
	}
	StartAoESchedule = FCombatScheduleHandle();
	// 蓝图负责淡出并最终销毁 Actor。
	BP_AoEFinished();
}
