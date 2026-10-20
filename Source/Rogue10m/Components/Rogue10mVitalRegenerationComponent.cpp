// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mVitalRegenerationComponent.h"

#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "TimerManager.h"

URogue10mVitalRegenerationComponent::URogue10mVitalRegenerationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URogue10mVitalRegenerationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			RegenerationTimerHandle, this, &URogue10mVitalRegenerationComponent::ApplyRegeneration,
			RegenerationInterval, true, RegenerationInterval);
	}
}

void URogue10mVitalRegenerationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RegenerationTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void URogue10mVitalRegenerationComponent::ConfigureRegeneration(
	float HealthPerSecond, float StaminaPerSecond, float ManaPerSecond)
{
	HealthRegenerationPerSecond = FMath::Max(0.0f, HealthPerSecond);
	StaminaRegenerationPerSecond = FMath::Max(0.0f, StaminaPerSecond);
	ManaRegenerationPerSecond = FMath::Max(0.0f, ManaPerSecond);
}

URogue10mAttributeSet* URogue10mVitalRegenerationComponent::ResolveAttributeSet() const
{
	if (const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner()))
	{
		return Character->GetRogueAttributeSet();
	}
	if (const ARogue10mBasicMonster* Monster = Cast<ARogue10mBasicMonster>(GetOwner()))
	{
		return Monster->GetRogueAttributeSet();
	}
	return nullptr;
}

void URogue10mVitalRegenerationComponent::ApplyRegeneration()
{
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwner());
	if (Character && Character->IsDead()) { return; }
	URogue10mAttributeSet* Attributes = ResolveAttributeSet();
	if (!Attributes || Attributes->GetHealth() <= 0.0f)
	{
		return;
	}

	const float Interval = FMath::Max(0.05f, RegenerationInterval);
	const URogue10mTraitRuntimeComponent* Traits = Character ? Character->GetTraitRuntimeComponent() : nullptr;
	const float TraitStaminaPerSecond = Traits ? Traits->GetTraitBonus(ERogue10mTownTrait::StaminaRecovery) : 0.0f;
	Attributes->SetHealth(FMath::Min(Attributes->GetMaxHealth(),
		Attributes->GetHealth() + HealthRegenerationPerSecond * Interval));
	Attributes->SetStamina(FMath::Min(Attributes->GetMaxStamina(),
		Attributes->GetStamina() + (StaminaRegenerationPerSecond + TraitStaminaPerSecond) * Interval));
	Attributes->SetMana(FMath::Min(Attributes->GetMaxMana(),
		Attributes->GetMana() + ManaRegenerationPerSecond * Interval));
}
