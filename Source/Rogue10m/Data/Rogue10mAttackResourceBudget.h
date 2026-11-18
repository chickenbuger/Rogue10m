#pragma once

#include "CoreMinimal.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"

/** One accepted attack's aggregate costs. Invalid data never produces a partial payment. */
struct FRogue10mAttackResourceBudget
{
	float Health = 0.0f;
	float Stamina = 0.0f;
	float Mana = 0.0f;
	float Energy = 0.0f;
	bool bValid = true;

	static FRogue10mAttackResourceBudget Build(const TArray<FRogue10mAttackResourceCost>& Costs, float StaminaMultiplier)
	{
		FRogue10mAttackResourceBudget Result;
		Result.bValid = FMath::IsFinite(StaminaMultiplier) && StaminaMultiplier >= 0.0f;
		for (const auto& Cost : Costs)
		{
			if (!FMath::IsFinite(Cost.Cost) || Cost.Cost < 0.0f) { Result.bValid = false; return Result; }
			float* Total = nullptr;
			switch (Cost.ResourceType)
			{
			case ERogue10mAttackResourceType::Health: Total = &Result.Health; break;
			case ERogue10mAttackResourceType::Stamina: Total = &Result.Stamina; break;
			case ERogue10mAttackResourceType::Mana: Total = &Result.Mana; break;
			case ERogue10mAttackResourceType::Energy: Total = &Result.Energy; break;
			default: Result.bValid = false; return Result;
			}
			*Total += Cost.Cost * (Cost.ResourceType == ERogue10mAttackResourceType::Stamina ? StaminaMultiplier : 1.0f);
			if (!FMath::IsFinite(*Total)) { Result.bValid = false; return Result; }
		}
		return Result;
	}

	bool CanPay(const URogue10mAttributeSet& Attributes) const
	{
		return bValid && FMath::IsFinite(Mana) && Mana >= 0.0f
			&& FMath::IsFinite(Attributes.GetHealth()) && FMath::IsFinite(Attributes.GetStamina())
			&& FMath::IsFinite(Attributes.GetMana()) && FMath::IsFinite(Attributes.GetIdentity())
			&& (Health == 0.0f || Attributes.GetHealth() > Health)
			&& Attributes.GetStamina() >= Stamina && Attributes.GetMana() >= Mana && Attributes.GetIdentity() >= Energy;
	}

	/** Caller serializes attack acceptance on the game thread and prevents reentrant casts. */
	bool TryPay(URogue10mAttributeSet& Attributes) const
	{
		if (!CanPay(Attributes)) { return false; }
		const float HealthAfter = Attributes.GetHealth() - Health;
		const float StaminaAfter = Attributes.GetStamina() - Stamina;
		const float ManaAfter = Attributes.GetMana() - Mana;
		const float EnergyAfter = Attributes.GetIdentity() - Energy;
		if (Health > 0.0f) { Attributes.SetHealth(HealthAfter); }
		if (Stamina > 0.0f) { Attributes.SetStamina(StaminaAfter); }
		if (Mana > 0.0f) { Attributes.SetMana(ManaAfter); }
		if (Energy > 0.0f) { Attributes.SetIdentity(EnergyAfter); }
		return true;
	}
};
