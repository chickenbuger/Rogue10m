// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mRunHUD.h"
#include "UObject/UObjectIterator.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Rogue10mSkillLoadoutDataAsset.h"

namespace Rogue10mWeaponSkillTreeTest
{
struct FWeaponStyle
{
	ERogue10mWeaponType WeaponType;
	const TCHAR* AssetId;
	int32 ExpectedJumpCount;
	int32 ExpectedTreeSkillCount;
	int32 ExpectedInitialUnlockCount;
	bool bUsesMartialArtsProgression;
};

static constexpr FWeaponStyle WeaponStyles[] = {
	{ERogue10mWeaponType::Dagger, TEXT("Dagger"), 2, 5, 5, false},
	{ERogue10mWeaponType::Shuriken, TEXT("Shuriken"), 2, 5, 5, false},
	{ERogue10mWeaponType::Bow, TEXT("Bow"), 2, 5, 5, false},
	{ERogue10mWeaponType::DualDaggers, TEXT("DualDaggers"), 2, 5, 5, false},
	{ERogue10mWeaponType::LongSword, TEXT("LongSword"), 1, 5, 5, false},
	{ERogue10mWeaponType::GreatSword, TEXT("GreatSword"), 1, 5, 5, false},
	{ERogue10mWeaponType::DualBlades, TEXT("DualBlades"), 1, 5, 5, false},
	{ERogue10mWeaponType::Shield, TEXT("Shield"), 1, 5, 5, false},
	{ERogue10mWeaponType::SwordBuckler, TEXT("SwordBuckler"), 1, 5, 5, false},
	{ERogue10mWeaponType::Staff, TEXT("Staff"), 2, 5, 5, false},
	{ERogue10mWeaponType::Knuckle, TEXT("Knuckle"), 2, 10, 1, true},
};

static UWorld* FindGameWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
		{
			return World;
		}
	}
	return nullptr;
}

static void RunWeaponSkillTreeSwitchTest()
{
	UWorld* World = FindGameWorld();
	ARogue10mCharacter* Character = World
		? Cast<ARogue10mCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
	URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	if (!World || !Character || !Combat)
	{
		UE_LOG(LogRogue10m, Error,
			TEXT("RESULT=WEAPON_SKILL_TREE_FAILED reason=no_game_world_character_or_combat"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}

	int32 FailureCount = 0;
	auto Check = [&FailureCount](bool bCondition, const FWeaponStyle& Style, const TCHAR* Reason)
	{
		if (!bCondition)
		{
			++FailureCount;
			UE_LOG(LogRogue10m, Error,
				TEXT("WEAPON_SKILL_TREE FAIL weapon=%s reason=%s"), Style.AssetId, Reason);
		}
	};

	URogue10mAttackSkillData* PreviousPrimary =
		Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
	for (const FWeaponStyle& Style : WeaponStyles)
	{
		const int32 FailureCountBeforeStyle = FailureCount;
		Combat->HandleAttackPressed(true);
		Character->SetEquippedWeaponType(Style.WeaponType);
		Combat->HandleAttackReleased(true);

		URogue10mAttackSkillData* Primary =
			Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
		URogue10mAttackSkillData* Special =
			Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Special);
		URogue10mAttackSkillData* Charged =
			Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedPrimary);
		const TArray<URogue10mAttackSkillData*> ActiveSkills = Combat->GetActiveSkillTreeSkills();

		Check(Character->GetEquippedWeaponType() == Style.WeaponType, Style,
			TEXT("장착 무기 타입 불일치"));
		Check(Combat->GetActiveSkillTreeWeaponType() == Style.WeaponType, Style,
			TEXT("활성 스킬트리 무기 타입 불일치"));
		Check(Character->JumpMaxCount == Style.ExpectedJumpCount, Style,
			TEXT("프로필 점프 횟수 불일치"));
		Check(ActiveSkills.Num() == Style.ExpectedTreeSkillCount, Style,
			TEXT("활성 트리 스킬 수 불일치"));
		Check(Combat->GetUnlockedWeaponSkills().Num() == Style.ExpectedInitialUnlockCount, Style,
			TEXT("활성 무기 초기 해금 스킬 수 불일치"));
		Check(Primary != nullptr, Style, TEXT("기본 공격 슬롯 누락"));
		Check(Style.bUsesMartialArtsProgression ? (!Special && !Charged) : (Special && Charged),
			Style, TEXT("초기 입력 슬롯 구성 불일치"));
		Check(Combat->GetActiveDodgeSkill() != nullptr, Style, TEXT("활성 회피 스킬 누락"));
		Check(Combat->GetEquippedSkill(ERogue10mAttackInputSlot::JumpPrimary) == nullptr,
			Style, TEXT("현재 프로필 밖 점프 스킬 fallback 노출"));
		Check(Combat->GetDisplayedAttackSkill() == Primary, Style,
			TEXT("무기 교체 후 이전 콤보/공격 표시 상태 잔존"));

		for (const URogue10mAttackSkillData* Skill : ActiveSkills)
		{
			const bool bMatchesStyle = Skill && (Skill->GetPathName().Contains(Style.AssetId)
				|| (Style.bUsesMartialArtsProgression && (Skill->GetPathName().Contains(TEXT("MartialArts"))
					|| Skill->GetPathName().Contains(TEXT("StoneFist")))));
			Check(bMatchesStyle, Style, TEXT("다른 무기 스킬이 활성 트리에 포함됨"));
			Check(Combat->IsSkillInActiveTree(Skill), Style,
				TEXT("활성 트리 소속 판정 실패"));
			const bool bShouldStartUnlocked = !Style.bUsesMartialArtsProgression || Skill == Primary;
			Check(Combat->IsAttackSkillUnlocked(Skill) == bShouldStartUnlocked, Style,
				TEXT("초기 해금 상태 불일치"));
		}

		if (Style.bUsesMartialArtsProgression)
		{
			ARogue10mPlayerState* Progression = Character->GetPlayerState<ARogue10mPlayerState>();
			Check(Progression != nullptr, Style, TEXT("권사 진행도 PlayerState 누락"));
			if (Progression)
			{
				for (const URogue10mAttackSkillData* Skill : ActiveSkills)
				{
					for (const FRogue10mSkillUnlockCondition& Condition : Skill->UnlockConditions)
					{
						if (Condition.ConditionType == ERogue10mSkillUnlockConditionType::SkillUseCount)
						{
							Progression->RecordSkillUse(
								Condition.RequiredSkill ? Condition.RequiredSkill->GetFName() : NAME_None,
								Condition.RequiredCount);
						}
						else
						{
							Progression->RecordMonsterDefeat(
								Condition.RequiredMonsterId, Condition.RequiredCount);
						}
					}
				}
				Combat->EvaluateActiveSkillUnlocks();
			}

			Check(Combat->GetUnlockedWeaponSkills().Num() == ActiveSkills.Num(), Style,
				TEXT("권사 수련 조건 충족 후 전체 해금 실패"));
			Check(FMath::IsNearlyEqual(Combat->GetUnlockedPassiveDamageReduction(), 0.15f), Style,
				TEXT("금강불괴 피해 감소 적용 실패"));
			for (URogue10mAttackSkillData* Skill : ActiveSkills)
			{
				Check(Combat->IsAttackSkillUnlocked(Skill), Style,
					TEXT("조건 충족 후 잠긴 권사 무공 잔존"));
				if (Skill && Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::PassiveTechnique)
				{
					Check(!Combat->AssignSkillToInputSlot(Skill, ERogue10mAttackInputSlot::Special), Style,
						TEXT("패시브 무공의 입력 슬롯 장착 허용"));
				}
			}
		}

		if (PreviousPrimary && PreviousPrimary != Primary)
		{
			Check(!Combat->AssignSkillToInputSlot(
				PreviousPrimary, ERogue10mAttackInputSlot::Special), Style,
				TEXT("이전 무기 스킬의 현재 슬롯 지정 허용"));
			Check(Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Special) == Special,
				Style, TEXT("거부된 교차 무기 지정이 현재 슬롯을 변경함"));
		}

		UE_LOG(LogRogue10m, Log,
			TEXT("WEAPON_SKILL_TREE %s weapon=%s skills=%d primary=%s"),
			FailureCount == FailureCountBeforeStyle ? TEXT("PASS") : TEXT("FAIL"),
			Style.AssetId, ActiveSkills.Num(), *GetNameSafe(Primary));
		PreviousPrimary = Primary;
	}

	UE_LOG(LogRogue10m, Display,
		TEXT("RESULT=WEAPON_SKILL_TREE_%s weapons=%d failures=%d"),
		FailureCount == 0 ? TEXT("PASSED") : TEXT("FAILED"),
		UE_ARRAY_COUNT(WeaponStyles), FailureCount);
	FPlatformMisc::RequestExitWithStatus(false, FailureCount == 0 ? 0 : 1);
}

static void CaptureMartialArtistSkillTreePreview()
{
	UWorld* World = FindGameWorld();
	ARogue10mCharacter* Character = World
		? Cast<ARogue10mCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
	ARogue10mPlayerController* Controller = Character
		? Cast<ARogue10mPlayerController>(Character->GetController()) : nullptr;
	URogue10mCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	ARogue10mPlayerState* Progression = Character
		? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
	if (!World || !Character || !Controller || !Combat || !Progression)
	{
		UE_LOG(LogRogue10m, Error,
			TEXT("RESULT=MARTIAL_ARTIST_SKILL_TREE_CAPTURE_FAILED reason=missing_runtime_state"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}

	Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
	for (TObjectIterator<URogue10mRunHUD> It; It; ++It)
	{
		if (It->GetWorld() == World)
		{
			It->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	const TArray<URogue10mAttackSkillData*> Skills = Combat->GetActiveSkillTreeSkills();
	for (const URogue10mAttackSkillData* Skill : Skills)
	{
		if (!Skill || Skill->SkillTreeTier != 2 || Skill->UnlockConditions.IsEmpty())
		{
			continue;
		}
		const FRogue10mSkillUnlockCondition& Condition = Skill->UnlockConditions[0];
		const int32 PreviewCount = FMath::Max(1, Condition.RequiredCount / 2);
		if (Condition.ConditionType == ERogue10mSkillUnlockConditionType::SkillUseCount
			&& Condition.RequiredSkill)
		{
			Progression->RecordSkillUse(Condition.RequiredSkill->GetFName(), PreviewCount);
		}
		else if (Condition.ConditionType == ERogue10mSkillUnlockConditionType::MonsterDefeatCount)
		{
			Progression->RecordMonsterDefeat(Condition.RequiredMonsterId, PreviewCount);
		}
	}

	Controller->ToggleSkillTree();
	static FTimerHandle CaptureHandle;
	static FTimerHandle ExitHandle;
	World->GetTimerManager().SetTimer(
		CaptureHandle,
		FTimerDelegate::CreateLambda([]
		{
			FScreenshotRequest::RequestScreenshot(TEXT("MartialArtistSkillTree.png"), true, false);
		}),
		0.8f, false);
	World->GetTimerManager().SetTimer(
		ExitHandle,
		FTimerDelegate::CreateLambda([]
		{
			UE_LOG(LogRogue10m, Display, TEXT("RESULT=MARTIAL_ARTIST_SKILL_TREE_CAPTURED"));
			FPlatformMisc::RequestExitWithStatus(false, 0);
		}),
		2.0f, false);
}

static FAutoConsoleCommand RunWeaponSkillTreeSwitchTestCommand(
	TEXT("Rogue10m.TestWeaponSkillTreeSwitching"),
	TEXT("Verifies that equipped weapons activate only their matching skill trees and bindings."),
	FConsoleCommandDelegate::CreateStatic(&RunWeaponSkillTreeSwitchTest));

static FAutoConsoleCommand CaptureMartialArtistSkillTreePreviewCommand(
	TEXT("Rogue10m.CaptureMartialArtistSkillTree"),
	TEXT("Opens the live martial artist skill tree, captures a preview, and exits."),
	FConsoleCommandDelegate::CreateStatic(&CaptureMartialArtistSkillTreePreview));
}

#endif
