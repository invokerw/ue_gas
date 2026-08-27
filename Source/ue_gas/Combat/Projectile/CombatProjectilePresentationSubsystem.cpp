#include "Combat/Projectile/CombatProjectilePresentationSubsystem.h"

#include "Combat/Projectile/CombatProjectileActor.h"

bool UCombatProjectilePresentationSubsystem::RegisterPredictedVisual(
	const int32 PredictionKey,
	AActor* VisualActor)
{
	if (PredictionKey <= 0 || !IsValid(VisualActor) || VisualActor->GetWorld() != GetWorld())
	{
		return false;
	}
	if (TWeakObjectPtr<AActor>* Existing = PredictedVisuals.Find(PredictionKey))
	{
		if (Existing->IsValid() && Existing->Get() != VisualActor)
		{
			Existing->Get()->Destroy();
		}
	}
	PredictedVisuals.Add(PredictionKey, VisualActor);
	return true;
}

void UCombatProjectilePresentationSubsystem::ReconcileServerProjectile(ACombatProjectileActor* ServerActor)
{
	if (!IsValid(ServerActor) || ServerActor->GetWorld() != GetWorld()
		|| !ServerActor->GetProjectileHandle().IsValid())
	{
		return;
	}
	const FCombatProjectileHandle Handle = ServerActor->GetProjectileHandle();
	if (const TWeakObjectPtr<ACombatProjectileActor>* Existing = ServerVisuals.Find(Handle))
	{
		if (Existing->IsValid())
		{
			++DuplicateServerIdentityCount;
			return;
		}
	}
	const int32 PredictionKey = ServerActor->GetPredictionKey();
	if (PredictionKey > 0)
	{
		if (TWeakObjectPtr<AActor>* Predicted = PredictedVisuals.Find(PredictionKey))
		{
			if (Predicted->IsValid() && Predicted->Get() != ServerActor)
			{
				Predicted->Get()->Destroy();
			}
			PredictedVisuals.Remove(PredictionKey);
			++ReconcileCount;
		}
	}
	ServerVisuals.Add(Handle, ServerActor);
}

void UCombatProjectilePresentationSubsystem::NotifyServerProjectileEnded(ACombatProjectileActor* ServerActor)
{
	if (!ServerActor)
	{
		return;
	}
	const FCombatProjectileHandle Handle = ServerActor->GetProjectileHandle();
	if (const TWeakObjectPtr<ACombatProjectileActor>* Existing = ServerVisuals.Find(Handle))
	{
		if (!Existing->IsValid() || Existing->Get() == ServerActor)
		{
			ServerVisuals.Remove(Handle);
		}
	}
}

FCombatProjectilePresentationStats UCombatProjectilePresentationSubsystem::GetPresentationStats() const
{
	FCombatProjectilePresentationStats Stats;
	for (const TPair<int32, TWeakObjectPtr<AActor>>& Pair : PredictedVisuals)
	{
		Stats.ActivePredictedVisuals += Pair.Value.IsValid() ? 1 : 0;
	}
	for (const TPair<FCombatProjectileHandle, TWeakObjectPtr<ACombatProjectileActor>>& Pair : ServerVisuals)
	{
		Stats.ActiveServerVisuals += Pair.Value.IsValid() ? 1 : 0;
	}
	Stats.ReconcileCount = static_cast<int64>(ReconcileCount);
	Stats.DuplicateServerIdentityCount = static_cast<int64>(DuplicateServerIdentityCount);
	return Stats;
}

void UCombatProjectilePresentationSubsystem::Deinitialize()
{
	for (const TPair<int32, TWeakObjectPtr<AActor>>& Pair : PredictedVisuals)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	PredictedVisuals.Reset();
	ServerVisuals.Reset();
	Super::Deinitialize();
}
