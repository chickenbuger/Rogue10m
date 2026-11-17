#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Rogue10mAttackTargetInterface.h"
#include "Templates/Function.h"
#include "Rogue10mHitInterruptionTestTarget.generated.h"

/** Native automation target. It records virtual damage; it is not production monster logic. */
UCLASS(Transient, NotBlueprintable)
class ARogue10mHitInterruptionTestTarget final : public AActor, public IRogue10mAttackTargetInterface
{
	GENERATED_BODY()
public:
	ARogue10mHitInterruptionTestTarget()
	{
		PrimaryActorTick.bCanEverTick = false;
		TargetCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("TargetCollision"));
		SetRootComponent(TargetCollision);
		TargetCollision->SetBoxExtent(FVector(20.0));
		TargetCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TargetCollision->SetCollisionObjectType(ECC_WorldDynamic);
		TargetCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
	}

	virtual bool CanReceiveRogue10mAttack_Implementation(AActor* AttackSource) const override
	{
		++FilterCalls;
		if (FilterCallback && !bFilterCallbackFired && FilterCalls == TriggerFilterCall)
		{
			bFilterCallbackFired = true;
			FilterCallback();
		}
		return AttackSource != nullptr;
	}
	virtual FVector GetRogue10mDamageIndicatorLocation_Implementation() const override
	{
		return GetActorLocation();
	}
	virtual float TakeDamage(float DamageAmount, const FDamageEvent&, AController*, AActor*) override
	{
		if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f) { return 0.0f; }
		++DamageCalls;
		TotalDamage += DamageAmount;
		if (DamageCallback && !bDamageCallbackFired)
		{
			bDamageCallbackFired = true;
			DamageCallback();
		}
		return DamageAmount;
	}

	int32 TriggerFilterCall = INDEX_NONE;
	mutable int32 FilterCalls = 0;
	int32 DamageCalls = 0;
	float TotalDamage = 0.0f;
	TFunction<void()> FilterCallback;
	TFunction<void()> DamageCallback;
private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> TargetCollision;
	mutable bool bFilterCallbackFired = false;
	bool bDamageCallbackFired = false;
};
