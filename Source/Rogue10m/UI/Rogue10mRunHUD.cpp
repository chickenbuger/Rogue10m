// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mRunHUD.h"

#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mPlayerState.h"

namespace
{
	FText GetIdentityResourceLabel(ERogue10mIdentityType Type)
	{
		switch (Type)
		{
		case ERogue10mIdentityType::StoneFist: return FText::FromString(TEXT("돌주먹"));
		case ERogue10mIdentityType::Vigor: return FText::FromString(TEXT("기력"));
		case ERogue10mIdentityType::Mana: return FText::FromString(TEXT("마력"));
		case ERogue10mIdentityType::Focus: return FText::FromString(TEXT("집중"));
		case ERogue10mIdentityType::Rage: return FText::FromString(TEXT("분노"));
		case ERogue10mIdentityType::None:
		default: return FText::GetEmpty();
		}
	}

	TSoftObjectPtr<UTexture2D> GetIdentityIconTexture(ERogue10mIdentityType Type)
	{
		const TCHAR* Path = nullptr;
		switch (Type)
		{
		case ERogue10mIdentityType::StoneFist: Path = TEXT("/Game/UI/Icons/T_Identity_StoneFist.T_Identity_StoneFist"); break;
		case ERogue10mIdentityType::Vigor: Path = TEXT("/Game/UI/Icons/T_Identity_Vigor.T_Identity_Vigor"); break;
		case ERogue10mIdentityType::Mana: Path = TEXT("/Game/UI/Icons/T_Identity_Mana.T_Identity_Mana"); break;
		case ERogue10mIdentityType::Focus: Path = TEXT("/Game/UI/Icons/T_Identity_Focus.T_Identity_Focus"); break;
		case ERogue10mIdentityType::Rage: Path = TEXT("/Game/UI/Icons/T_Identity_Rage.T_Identity_Rage"); break;
		case ERogue10mIdentityType::None:
		default: break;
		}
		return Path ? TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)) : TSoftObjectPtr<UTexture2D>();
	}

	FString GetWeaponTypeText(ERogue10mWeaponType Type)
	{
		switch (Type)
		{
		case ERogue10mWeaponType::Unarmed: return TEXT("권");
		case ERogue10mWeaponType::Dagger: return TEXT("단검");
		case ERogue10mWeaponType::GreatSword: return TEXT("대검");
		case ERogue10mWeaponType::DualBlades: return TEXT("쌍검");
		case ERogue10mWeaponType::Bow: return TEXT("활");
		case ERogue10mWeaponType::Staff: return TEXT("지팡이");
		case ERogue10mWeaponType::Knuckle: return TEXT("권");
		case ERogue10mWeaponType::Shuriken: return TEXT("표창");
		case ERogue10mWeaponType::DualDaggers: return TEXT("쌍단검");
		case ERogue10mWeaponType::LongSword: return TEXT("장검");
		case ERogue10mWeaponType::Shield: return TEXT("방패");
		case ERogue10mWeaponType::SwordBuckler: return TEXT("한손검·작은방패");
		default: return TEXT("무기");
		}
	}
}

void URogue10mRunHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bBroadcastHudDataUpdatedEveryTick)
	{
		BP_OnHudDataUpdated(InDeltaTime);
	}
}

ARogue10mPlayerController* URogue10mRunHUD::GetRoguePlayerController() const
{
	return Cast<ARogue10mPlayerController>(GetOwningPlayer());
}

FRogue10mHudVitalView URogue10mRunHUD::GetHealthView() const
{
	const ARogue10mPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	const URogue10mAttributeSet* Attributes = State ? State->GetRogueAttributeSet() : nullptr;
	return Attributes
		? MakeVitalView(Attributes->GetHealth(), Attributes->GetMaxHealth(), true, FLinearColor(0.92f, 0.12f, 0.12f, 1.0f))
		: FRogue10mHudVitalView();
}

FRogue10mHudVitalView URogue10mRunHUD::GetStaminaView() const
{
	const ARogue10mPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	const URogue10mAttributeSet* Attributes = State ? State->GetRogueAttributeSet() : nullptr;
	return Attributes
		? MakeVitalView(Attributes->GetStamina(), Attributes->GetMaxStamina(), true, FLinearColor(0.96f, 0.66f, 0.04f, 1.0f))
		: FRogue10mHudVitalView();
}

FRogue10mHudVitalView URogue10mRunHUD::GetManaView() const
{
	const ARogue10mPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	const URogue10mAttributeSet* Attributes = State ? State->GetRogueAttributeSet() : nullptr;
	if (!State || !State->IsManaEnabled() || !Attributes)
	{
		return FRogue10mHudVitalView();
	}
	return MakeVitalView(Attributes->GetMana(), Attributes->GetMaxMana(), true, FLinearColor(0.45f, 0.72f, 1.0f, 1.0f));
}

FRogue10mHudProgressionView URogue10mRunHUD::GetProgressionView() const
{
	FRogue10mHudProgressionView View;
	const ARogue10mPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	if (State)
	{
		View.Level = State->GetPlayerLevel();
		View.CurrentExperience = State->GetCurrentExperience();
		View.ExperienceToNextLevel = State->GetExperienceToNextLevel();
		View.ExperienceNormalized = State->GetExperienceNormalized();
	}
	return View;
}

FRogue10mHudIdentityView URogue10mRunHUD::GetIdentityView() const
{
	FRogue10mHudIdentityView View;
	const ARogue10mPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	if (!State)
	{
		return View;
	}

	const ERogue10mWeaponType WeaponType = Character ? Character->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed;
	const ERogue10mIdentityType IdentityType = State->GetIdentityType();
	const int32 MasteryLevel = State->GetWeaponMasteryLevel(WeaponType);
	View.Label = FText::FromString(FString::Printf(TEXT("%s 숙련도 %d"), *GetWeaponTypeText(WeaponType), MasteryLevel));
	View.WeaponType = WeaponType;
	View.IdentityType = IdentityType;
	View.MasteryLevel = MasteryLevel;
	View.MasteryNormalized = FMath::Clamp(static_cast<float>(MasteryLevel) / 10.0f, 0.0f, 1.0f);
	View.OutlineColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.36f, 0.36f, 0.36f, 1.0f),
		FLinearColor(0.92f, 0.78f, 0.34f, 1.0f),
		View.MasteryNormalized);
	View.OutlineThickness = FMath::Lerp(1.0f, 6.0f, View.MasteryNormalized);
	View.ResourceLabel = GetIdentityResourceLabel(IdentityType);
	View.IconTexture = GetIdentityIconTexture(IdentityType);
	if (View.IconTexture.IsNull() && (WeaponType == ERogue10mWeaponType::Unarmed || WeaponType == ERogue10mWeaponType::Knuckle))
	{
		View.IconTexture = TSoftObjectPtr<UTexture2D>(
			FSoftObjectPath(TEXT("/Game/UI/Icons/T_Identity_StoneFist.T_Identity_StoneFist")));
	}
	View.Current = State->GetIdentityCurrent();
	View.Max = State->GetIdentityMax();
	View.Normalized = State->GetIdentityNormalized();
	View.bHasIdentityResource = State->HasIdentityResource();
	return View;
}

FRogue10mHudMonsterInfoView URogue10mRunHUD::GetLookedAtMonsterInfoView() const
{
	FRogue10mHudMonsterInfoView View;
	const ARogue10mPlayerController* Controller = GetRoguePlayerController();
	const ARogue10mBasicMonster* Monster = Controller ? Controller->FindLookedAtMonster() : nullptr;
	const URogue10mAttributeSet* Attributes = Monster ? Monster->GetRogueAttributeSet() : nullptr;
	if (!Monster || !Attributes)
	{
		return View;
	}

	View.bHasMonster = true;
	View.Name = Monster->GetMonsterDisplayName();
	View.Level = Monster->GetMonsterLevel();
	View.AttributeText = Monster->GetMonsterAttributeText();
	View.StateText = FText::FromString(Monster->IsDead() ? TEXT("사망") : TEXT("정상"));
	View.Health = MakeVitalView(
		Attributes->GetHealth(), Attributes->GetMaxHealth(), true,
		FLinearColor(0.92f, 0.12f, 0.12f, 1.0f));
	return View;
}

TArray<FRogue10mHudQuickSlotView> URogue10mRunHUD::GetSkillQuickSlotViews() const
{
	TArray<FRogue10mHudQuickSlotView> Views;
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	const URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	if (!Combat)
	{
		return Views;
	}

	struct FSkillSlotDefinition
	{
		ERogue10mAttackInputSlot InputSlot;
		const TCHAR* InputLabel;
	};
	const FSkillSlotDefinition Definitions[] = {
		{ ERogue10mAttackInputSlot::Primary, TEXT("좌클릭") },
		{ ERogue10mAttackInputSlot::Special, TEXT("우클릭") },
		{ ERogue10mAttackInputSlot::ChargedPrimary, TEXT("좌클릭 차징") },
		{ ERogue10mAttackInputSlot::ChargedSpecial, TEXT("우클릭 차징") }
	};

	Views.Reserve(5);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Definitions); ++Index)
	{
		const FSkillSlotDefinition& Definition = Definitions[Index];
		const URogue10mAttackSkillData* Skill = Combat->GetEquippedSkill(Definition.InputSlot);

		FRogue10mHudQuickSlotView View;
		View.SlotNumber = Index + 1;
		View.InputSlot = Definition.InputSlot;
		View.InputText = FText::FromString(Definition.InputLabel);
		View.SkillData = const_cast<URogue10mAttackSkillData*>(Skill);
		View.bUnlocked = Skill && Combat->IsAttackSkillUnlocked(Skill);
		View.IconColor = View.bUnlocked ? Skill->IconTint : FLinearColor(0.18f, 0.18f, 0.2f, 1.0f);
		View.SkillIcon = Skill ? Skill->SkillIcon : nullptr;

		if (View.bUnlocked)
		{
			TArray<FString> UnlockedComboNames;
			const URogue10mAttackSkillData* ComboSkill = Skill;
			while (ComboSkill && Combat->IsAttackSkillUnlocked(ComboSkill))
			{
				UnlockedComboNames.Add(ComboSkill->SkillName.ToString());
				ComboSkill = ComboSkill->bEnableCombo ? ComboSkill->NextComboSkill : nullptr;
			}
			View.DisplayName = FText::FromString(FString::Join(UnlockedComboNames, TEXT(" → ")));
			View.CooldownRemaining = Combat->GetAttackCooldownRemaining();
			View.CooldownDuration = Combat->GetAttackCooldownDuration();
			View.CooldownNormalized = View.CooldownDuration > 0.0f
				? FMath::Clamp(View.CooldownRemaining / View.CooldownDuration, 0.0f, 1.0f)
				: 0.0f;
		}
		Views.Add(View);
	}

	FRogue10mHudQuickSlotView DodgeView;
	DodgeView.SlotNumber = 5;
	DodgeView.InputText = FText::FromString(TEXT("E"));
	DodgeView.bDodgeSlot = true;
	DodgeView.bUnlocked = true;
	if (const URogue10mDodgeSkillDataAsset* DodgeSkill = Combat->GetActiveDodgeSkill())
	{
		DodgeView.DisplayName = DodgeSkill->DisplayName;
		DodgeView.SkillIcon = DodgeSkill->Icon;
		DodgeView.IconColor = FLinearColor(0.32f, 0.68f, 1.0f, 1.0f);
	}
	else
	{
		DodgeView.DisplayName = FText::FromString(TEXT("기본 회피"));
		DodgeView.IconColor = FLinearColor(0.32f, 0.68f, 1.0f, 1.0f);
	}
	Views.Add(DodgeView);
	return Views;
}
TArray<FRogue10mHudQuickSlotView> URogue10mRunHUD::GetItemQuickSlotViews() const
{
	TArray<FRogue10mHudQuickSlotView> Views;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FRogue10mHudQuickSlotView View;
		View.SlotNumber = Index + 1;
		View.DisplayName = FText::GetEmpty();
		View.IconColor = FLinearColor(0.2f, 0.22f, 0.26f, 1.0f);
		View.bUnlocked = false;
		Views.Add(View);
	}
	return Views;
}

TArray<FRogue10mHudLogEntryView> URogue10mRunHUD::GetSystemLogEntries() const
{
	TArray<FRogue10mHudLogEntryView> Views;
	const ARogue10mPlayerController* Controller = GetRoguePlayerController();
	if (!Controller || !GetWorld())
	{
		return Views;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	for (const FRogue10mRuntimeLogEntry& Entry : Controller->GetCombatLogEntries())
	{
		if (Entry.ExpireTime <= CurrentTime)
		{
			continue;
		}
		FRogue10mHudLogEntryView View;
		View.Message = FText::FromString(Entry.Message);
		View.Color = Entry.Color;
		View.RemainingSeconds = Entry.ExpireTime - CurrentTime;
		Views.Add(View);
	}
	return Views;
}

TArray<FRogue10mHudLogEntryView> URogue10mRunHUD::GetItemAcquisitionEntries() const
{
	TArray<FRogue10mHudLogEntryView> Views;
	const ARogue10mPlayerController* Controller = GetRoguePlayerController();
	if (!Controller || !GetWorld())
	{
		return Views;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	for (const FRogue10mRuntimeLogEntry& Entry : Controller->GetItemAcquisitionEntries())
	{
		if (Entry.ExpireTime <= CurrentTime || Entry.Quantity <= 0)
		{
			continue;
		}
		FRogue10mHudLogEntryView View;
		View.Message = FText::FromString(Entry.Message);
		View.Color = Entry.Color;
		View.RemainingSeconds = Entry.ExpireTime - CurrentTime;
		View.ItemIcon = Entry.ItemIcon.Get();
		View.Quantity = Entry.Quantity;
		View.bItemAcquisition = true;
		Views.Add(View);
	}
	return Views;
}

FRogue10mHudPanelStateView URogue10mRunHUD::GetPanelStateView() const
{
	FRogue10mHudPanelStateView View;
	if (const ARogue10mPlayerController* Controller = GetRoguePlayerController())
	{
		View.bEquipmentVisible = Controller->IsInventoryVisible();
		View.bItemWindowVisible = Controller->IsItemWindowVisible();
		View.bSkillTreeVisible = Controller->IsSkillTreeVisible();
		View.bSettingsVisible = Controller->IsSettingsVisible();
	}
	return View;
}

TArray<FRogue10mHudMinimapMarkerView> URogue10mRunHUD::GetPrototypeMinimapMarkers() const
{
	TArray<FRogue10mHudMinimapMarkerView> Views;
	Views.Add({ FVector2D(0.5f, 0.5f), FLinearColor(0.28f, 0.86f, 1.0f, 1.0f), FText::FromString(TEXT("플레이어")) });
	Views.Add({ FVector2D(0.23f, 0.32f), FLinearColor(0.92f, 0.18f, 0.12f, 1.0f), FText::FromString(TEXT("적")) });
	Views.Add({ FVector2D(0.81f, 0.44f), FLinearColor(0.35f, 0.9f, 0.42f, 1.0f), FText::FromString(TEXT("NPC")) });
	Views.Add({ FVector2D(0.38f, 0.78f), FLinearColor(0.96f, 0.78f, 0.28f, 1.0f), FText::FromString(TEXT("목표")) });
	return Views;
}

FRogue10mHudRunTimerView URogue10mRunHUD::GetRunTimerView() const
{
	FRogue10mHudRunTimerView View;
	const ARogue10mGameState* State = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (State && State->IsRunActive())
	{
		View.bVisible = true;
		View.RemainingSeconds = State->GetRemainingRunSeconds();
		View.TotalSeconds = State->GetRunDurationSeconds();
		View.ProgressAlpha = State->GetRunProgressAlpha();
		View.RemainingText = State->GetRemainingTimeText();
	}
	return View;
}

FRogue10mHudRunResultView URogue10mRunHUD::GetRunResultView() const
{
	FRogue10mHudRunResultView View;
	const ARogue10mGameState* State = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (!State || State->IsRunActive())
	{
		return View;
	}

	View.RunPhase = State->GetRunPhase();
	if (View.RunPhase == ERogue10mRunPhase::Victory)
	{
		View.bVisible = true;
		View.ResultText = FText::FromString(TEXT("승리"));
		View.DescriptionText = FText::FromString(TEXT("보스를 처치하고 생존에 성공했습니다."));
		View.ResultColor = FLinearColor(0.2f, 1.0f, 0.35f, 1.0f);
	}
	else if (View.RunPhase == ERogue10mRunPhase::Defeat)
	{
		View.bVisible = true;
		View.ResultText = FText::FromString(TEXT("게임 오버"));
		View.DescriptionText = FText::FromString(TEXT("제한 시간이 끝나 캐릭터가 사망했습니다."));
		View.ResultColor = FLinearColor(1.0f, 0.2f, 0.15f, 1.0f);
	}
	return View;
}

FRogue10mHudAimCrossLineView URogue10mRunHUD::GetAimCrossLineView() const
{
	FRogue10mHudAimCrossLineView View;
	const ARogue10mPlayerController* Controller = GetRoguePlayerController();
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	View.bVisible = Character && !Character->IsDead() && (!Controller || !Controller->IsAnyBlockingWindowVisible());
	if (Controller)
	{
		View.Color = Controller->GetAimCrossLineColor();
		View.LineLength = Controller->GetAimCrossLineLength();
		View.Gap = Controller->GetAimCrossLineGap();
		View.Thickness = Controller->GetAimCrossLineThickness();
	}
	return View;
}

FRogue10mHudDamageFeedbackView URogue10mRunHUD::GetDamageFeedbackView() const
{
	FRogue10mHudDamageFeedbackView View;
	if (const ARogue10mPlayerController* Controller = GetRoguePlayerController())
	{
		View.Alpha = Controller->GetPlayerDamageFeedbackAlpha();
		View.Strength = Controller->GetPlayerDamageFeedbackStrength();
		View.bVisible = View.Alpha > 0.0f;
	}
	return View;
}

FRogue10mHudAttackCooldownView URogue10mRunHUD::GetAttackCooldownView() const
{
	FRogue10mHudAttackCooldownView View;
	const ARogue10mCharacter* Character = Cast<ARogue10mCharacter>(GetOwningPlayerPawn());
	const URogue10mAttackSkillData* Skill = Character ? Character->GetDisplayedAttackSkillForHUD() : nullptr;
	if (!Character || !Skill)
	{
		return View;
	}

	View.bVisible = true;
	View.SkillName = Skill->SkillName;
	View.IconLabel = Skill->IconLabel;
	View.IconTint = Skill->IconTint;
	View.SkillIcon = Skill->SkillIcon;
	View.CooldownRemaining = Character->GetAttackCooldownRemaining();
	View.CooldownDuration = Character->GetAttackCooldownDuration();
	View.CooldownNormalized = View.CooldownDuration > 0.0f
		? FMath::Clamp(View.CooldownRemaining / View.CooldownDuration, 0.0f, 1.0f)
		: 0.0f;
	View.bOnCooldown = View.CooldownRemaining > 0.0f;
	return View;
}

FRogue10mHudVitalView URogue10mRunHUD::MakeVitalView(
	float Current, float Max, bool bVisible, const FLinearColor& FillColor) const
{
	FRogue10mHudVitalView View;
	View.Current = Current;
	View.Max = Max;
	View.Normalized = Max > 0.0f ? FMath::Clamp(Current / Max, 0.0f, 1.0f) : 0.0f;
	View.Percent = View.Normalized * 100.0f;
	View.ValueText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Current, Max));
	View.PercentText = FText::FromString(FString::Printf(TEXT("%.0f%%"), View.Percent));
	View.FillColor = FillColor;
	View.bVisible = bVisible;
	return View;
}
