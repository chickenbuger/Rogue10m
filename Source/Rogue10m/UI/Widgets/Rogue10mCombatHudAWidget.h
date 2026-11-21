// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "Rogue10mRunHUD.h"
#include "Rogue10mCombatHudAWidget.generated.h"

class APlayerController;
class SCombatHudAHitZone;
class UTexture2D;

/** Presentation for the approved 1672 by 941 combat HUD. The Main HUD owns its refresh cadence. */
UCLASS()
class ROGUE10M_API URogue10mCombatHudAWidget final : public UWidget
{
	GENERATED_BODY()

public:
	URogue10mCombatHudAWidget(const FObjectInitializer& ObjectInitializer);

	bool HasAtlas() const { return Atlas != nullptr; }
	void SetOwningPlayerController(APlayerController* Player);
	void SetFrequentViews(const FRogue10mHudVitalView& Health, const FRogue10mHudVitalView& Stamina,
		const FRogue10mHudVitalView& Mana, const FRogue10mHudProgressionView& Progression,
		const FRogue10mHudIdentityView& Identity, bool bUseMana, float CurrentDodgeCooldown, bool bCurrentDodging);
	void SetSlowViews(const TArray<FRogue10mHudQuickSlotView>& Skills, const TArray<FRogue10mHudQuickSlotView>& Items);

	const FRogue10mHudVitalView& GetDisplayedHealth() const { return HealthView; }
	const FRogue10mHudVitalView& GetDisplayedResource() const { return ResourceView; }
	const FRogue10mHudProgressionView& GetDisplayedProgression() const { return ProgressionView; }
	const FRogue10mHudIdentityView& GetDisplayedIdentity() const { return IdentityView; }
	const TArray<FRogue10mHudQuickSlotView>& GetDisplayedSkills() const { return SkillViews; }
	const TArray<FRogue10mHudQuickSlotView>& GetDisplayedItems() const { return ItemViews; }
	float GetDodgeCooldownRemaining() const { return DodgeCooldownRemaining; }
	bool IsDodging() const { return bIsDodging; }
	bool IsUsingMana() const { return bUsingMana; }
	bool ShowsIdentityPercent() const { return IdentityView.bHasIdentityResource; }
	bool ShowsIdentityKey() const { return IdentityView.bHasManualActivation; }
	UTexture2D* GetDisplayedIdentityIcon() const { return ResolvedIdentityIcon; }

	static FBox2D GetVitalsBounds(const FVector2D& ViewportSize);
	static FBox2D GetCombatBounds(const FVector2D& ViewportSize);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	friend class SCombatHudAHitZone;
	bool TryAssignSkill(int32 Index, URogue10mAttackSkillData* Skill);
	void InvalidatePaint();

	UPROPERTY()
	TObjectPtr<UTexture2D> Atlas;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ResolvedIdentityIcon;

	UPROPERTY(Transient)
	FRogue10mHudIdentityView IdentityView;

	UPROPERTY(Transient)
	TArray<FRogue10mHudQuickSlotView> SkillViews;

	UPROPERTY(Transient)
	TArray<FRogue10mHudQuickSlotView> ItemViews;

	TWeakObjectPtr<APlayerController> OwningPlayer;
	FRogue10mHudVitalView HealthView;
	FRogue10mHudVitalView ResourceView;
	FRogue10mHudProgressionView ProgressionView;
	float DodgeCooldownRemaining = 0.0f;
	bool bIsDodging = false;
	bool bUsingMana = false;
};
