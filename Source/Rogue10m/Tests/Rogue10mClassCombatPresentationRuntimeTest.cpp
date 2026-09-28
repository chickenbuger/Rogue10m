// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Animation/AnimInstance.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Rogue10m.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mClassCombatPresentationTest
{
struct FStyle
{
	ERogue10mWeaponType WeaponType;
	const TCHAR* AssetId;
	int32 ExpectedJumpCount;
	int32 ExpectedTreeSkillCount;
	int32 ExpectedInitialUnlockCount;
	bool bUsesMartialArtsProgression;
};

static constexpr FStyle Styles[] = {
	{ERogue10mWeaponType::Dagger, TEXT("Dagger"), 2, 5, 5, false},
	{ERogue10mWeaponType::Shuriken, TEXT("Shuriken"), 2, 5, 5, false},
	{ERogue10mWeaponType::DualDaggers, TEXT("DualDaggers"), 2, 5, 5, false},
	{ERogue10mWeaponType::LongSword, TEXT("LongSword"), 1, 5, 5, false},
	{ERogue10mWeaponType::GreatSword, TEXT("GreatSword"), 1, 5, 5, false},
	{ERogue10mWeaponType::DualBlades, TEXT("DualBlades"), 1, 5, 5, false},
	{ERogue10mWeaponType::Shield, TEXT("Shield"), 1, 5, 5, false},
	{ERogue10mWeaponType::SwordBuckler, TEXT("SwordBuckler"), 1, 5, 5, false},
	{ERogue10mWeaponType::Staff, TEXT("Staff"), 2, 5, 5, false},
	{ERogue10mWeaponType::Knuckle, TEXT("Knuckle"), 2, 10, 1, true},
};

class FRuntimeState : public TSharedFromThis<FRuntimeState>
{
public:
	void Start(UWorld& InWorld, ARogue10mCharacter& InCharacter)
	{
		World = &InWorld;
		Character = &InCharacter;
		Combat = InCharacter.GetCombatComponent();
		Check(Combat.IsValid(), TEXT("Combat Component가 없습니다."));
		RunNextStyle();
	}

private:
	void Check(bool bCondition, const FString& Message)
	{
		if (bCondition)
		{
			return;
		}
		++FailureCount;
		++CurrentStyleFailureCount;
		UE_LOG(LogRogue10m, Error, TEXT("CLASS_COMBAT_RUNTIME FAIL style=%s reason=%s"),
			CurrentStyleIndex < UE_ARRAY_COUNT(Styles) ? Styles[CurrentStyleIndex].AssetId : TEXT("Setup"),
			*Message);
	}

	void Schedule(float DelaySeconds, TFunction<void()> Callback)
	{
		if (!World.IsValid())
		{
			Check(false, TEXT("테스트 World가 종료되었습니다."));
			Finish();
			return;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateLambda([Self = AsShared(), Callback = MoveTemp(Callback)]() mutable
			{
				if (Self->World.IsValid())
				{
					Callback();
				}
			}),
			FMath::Max(0.01f, DelaySeconds), false);
	}

	int32 CountActiveFirstPersonEffects() const
	{
		if (!Character.IsValid() || !Character->GetFirstPersonMesh())
		{
			return 0;
		}
		TArray<USceneComponent*> AttachedComponents;
		Character->GetFirstPersonMesh()->GetChildrenComponents(true, AttachedComponents);
		int32 ActiveCount = 0;
		for (USceneComponent* Component : AttachedComponents)
		{
			const UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Component);
			if (Niagara && Niagara->IsActive())
			{
				++ActiveCount;
			}
		}
		return ActiveCount;
	}

	bool IsMontagePlaying(const URogue10mAttackSkillData* Skill) const
	{
		USkeletalMeshComponent* Mesh = Character.IsValid() ? Character->GetAnimationPlaybackMesh() : nullptr;
		UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		return Skill && Skill->AttackMontage && AnimInstance
			&& AnimInstance->Montage_IsPlaying(Skill->AttackMontage);
	}

	void ValidateFirstPersonBudget(const URogue10mAttackSkillData& Skill)
	{
		Check(Skill.bEnableAttackEffects, TEXT("공격 파티클이 꺼져 있습니다."));
		Check(Skill.bUseFirstPersonEffectOverrides, TEXT("1인칭 파티클 Override가 꺼져 있습니다."));
		Check(Skill.CastEffect != nullptr && Skill.ImpactEffect != nullptr, TEXT("Cast/Impact Niagara가 없습니다."));
		Check(Skill.CastEffectScale * Skill.FirstPersonCastScaleMultiplier <= 0.0601f,
			TEXT("1인칭 Cast 크기 상한을 초과했습니다."));
		Check(Skill.ChargeEffectScale * Skill.FirstPersonChargeScaleMultiplier <= 0.0261f,
			TEXT("1인칭 Charge 크기 상한을 초과했습니다."));
		Check(Skill.ImpactEffectScale * Skill.FirstPersonImpactScaleMultiplier <= 0.1801f,
			TEXT("1인칭 Impact 크기 상한을 초과했습니다."));
		Check(Skill.CastEffectEmissionDuration * Skill.FirstPersonEmissionDurationMultiplier <= 0.0601f,
			TEXT("1인칭 Cast 지속시간 상한을 초과했습니다."));
		Check(Skill.ImpactEffectEmissionDuration * Skill.FirstPersonEmissionDurationMultiplier <= 0.0701f,
			TEXT("1인칭 Impact 지속시간 상한을 초과했습니다."));
		Check(Skill.FirstPersonOffHandEffectScaleMultiplier <= 0.70f,
			TEXT("1인칭 보조 손 파티클 배율이 너무 큽니다."));
		Check(FMath::Abs(Skill.FirstPersonEffectOffset.Y) >= 14.0f && Skill.FirstPersonEffectOffset.Z <= -11.0f,
			TEXT("파티클이 조준점 아래/측면으로 이동되지 않았습니다."));
	}

	void RunNextStyle()
	{
		if (!Character.IsValid() || !Combat.IsValid() || CurrentStyleIndex >= UE_ARRAY_COUNT(Styles))
		{
			Finish();
			return;
		}

		CurrentStyleFailureCount = 0;
		const FStyle& Style = Styles[CurrentStyleIndex];
		if (URogue10mAttributeSet* Attributes = Character->GetRogueAttributeSet())
		{
			Attributes->RestoreVitals();
		}
		const TWeakObjectPtr<URogue10mAttackSkillData> PreviousPrimary =
			Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
		Combat->HandleAttackPressed(true);
		Character->SetEquippedWeaponType(Style.WeaponType);
		Check(CountActiveFirstPersonEffects() == 0, TEXT("무기 교체 후 이전 차징 Niagara가 남아 있습니다."));

		Primary01 = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
		Primary02 = Primary01.IsValid() ? Primary01->NextComboSkill.Get() : nullptr;
		Primary03 = Primary02.IsValid() ? Primary02->NextComboSkill.Get() : nullptr;
		Charged = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ChargedPrimary);
		const URogue10mAttackSkillData* Special = Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Special);
		bLastMontagePlaybackObserved = false;
		if (Primary01.IsValid() && Primary02.IsValid())
		{
			// A live editor game compiles a Niagara system on first use. Keep that editor-only hitch
			// from invalidating the input-flow test; saved combo timings are validated separately.
			Primary01->ComboWindowCloseSeconds = FMath::Max(Primary01->ComboWindowCloseSeconds, 2.0f);
			Primary02->ComboWindowCloseSeconds = FMath::Max(Primary02->ComboWindowCloseSeconds, 2.0f);
		}

		Check(Character->GetEquippedWeaponType() == Style.WeaponType, TEXT("무기 유형 전환에 실패했습니다."));
		Check(Combat->GetActiveSkillTreeWeaponType() == Style.WeaponType, TEXT("활성 스킬트리가 장착 무기와 다릅니다."));
		Check(Character->JumpMaxCount == Style.ExpectedJumpCount, TEXT("클래스별 점프 횟수가 다릅니다."));
		const TArray<URogue10mAttackSkillData*> ActiveTreeSkills = Combat->GetActiveSkillTreeSkills();
		Check(ActiveTreeSkills.Num() == Style.ExpectedTreeSkillCount,
			TEXT("활성 스킬트리의 스킬 수가 다릅니다."));
		Check(Combat->GetUnlockedWeaponSkills().Num() == Style.ExpectedInitialUnlockCount,
			TEXT("활성 무기의 초기 해금 스킬 수가 다릅니다."));
		for (const URogue10mAttackSkillData* ActiveTreeSkill : ActiveTreeSkills)
		{
			const bool bMatchesStyle = ActiveTreeSkill && (ActiveTreeSkill->GetPathName().Contains(Style.AssetId)
				|| (Style.bUsesMartialArtsProgression && (ActiveTreeSkill->GetPathName().Contains(TEXT("MartialArts"))
					|| ActiveTreeSkill->GetPathName().Contains(TEXT("StoneFist")))));
			Check(bMatchesStyle, TEXT("활성 스킬트리에 다른 무기 스킬이 포함되었습니다."));
		}
		if (PreviousPrimary.IsValid() && PreviousPrimary.Get() != Primary01.Get())
		{
			Check(!Combat->AssignSkillToInputSlot(
				PreviousPrimary.Get(), ERogue10mAttackInputSlot::Special),
				TEXT("이전 무기 스킬이 현재 무기 슬롯에 지정되었습니다."));
		}
		Check(Combat->GetEquippedSkill(ERogue10mAttackInputSlot::JumpPrimary) == nullptr,
			TEXT("현재 프로필에 없는 점프 스킬이 fallback으로 노출되었습니다."));
		Check(Primary01.IsValid() && Primary02.IsValid() && Primary03.IsValid(), TEXT("3연계 공격 연결이 없습니다."));
		Check(Style.bUsesMartialArtsProgression ? (!Charged.IsValid() && Special == nullptr)
			: (Charged.IsValid() && Special != nullptr), TEXT("초기 차징/특수 공격 구성 불일치"));
		if (Primary01.IsValid())
		{
			Check(Primary01->GetPathName().Contains(Style.AssetId), TEXT("다른 스타일의 Primary가 연결되었습니다."));
			ValidateFirstPersonBudget(*Primary01);
		}
		if (Charged.IsValid())
		{
			Check(Charged->ChargeEffect != nullptr, TEXT("Charge Niagara가 없습니다."));
			ValidateFirstPersonBudget(*Charged);
		}

		UE_LOG(LogRogue10m, Log, TEXT("CLASS_COMBAT_RUNTIME BEGIN style=%s jump=%d"),
			Style.AssetId, Character->JumpMaxCount);
		if (Style.bUsesMartialArtsProgression)
		{
			Combat->HandleAttackPressed(true);
			Schedule(0.04f, [Self = AsShared()] { Self->WarmupMartialPrimary(); });
			return;
		}
		Combat->HandleAttackPressed(true);
		Check(CountActiveFirstPersonEffects() >= 1, TEXT("짧은 입력 시작 시 1인칭 Charge Niagara가 생성되지 않았습니다."));
		Schedule(0.04f, [Self = AsShared()] { Self->ReleasePrimary01(); });
	}

	void WarmupMartialPrimary()
	{
		Combat->HandleAttackReleased(true);
		const float WaitSeconds = Primary01.IsValid() ? Primary01->ComboWindowCloseSeconds + 0.35f : 2.2f;
		Schedule(WaitSeconds, [Self = AsShared()] { Self->BeginMartialPrimary(); });
	}

	void BeginMartialPrimary()
	{
		Combat->HandleAttackPressed(true);
		Schedule(0.04f, [Self = AsShared()] { Self->ReleaseMartialPrimary(); });
	}

	void ReleaseMartialPrimary()
	{
		Combat->HandleAttackReleased(true);
		bLastMontagePlaybackObserved = IsMontagePlaying(Primary01.Get());
		Check(CountActiveFirstPersonEffects() >= 1,
			TEXT("권사 기본 공격 시 1인칭 Cast Niagara가 생성되지 않았습니다."));
		FScreenshotRequest::RequestScreenshot(
			FString::Printf(TEXT("ClassCombat_%s.png"), Styles[CurrentStyleIndex].AssetId),
			false, false);
		Schedule(0.06f, [Self = AsShared()] { Self->InspectMartialPrimary(); });
	}

	void InspectMartialPrimary()
	{
		Check(bLastMontagePlaybackObserved, TEXT("권사 기본 공격 Montage가 재생되지 않았습니다."));
		UE_LOG(LogRogue10m, Log, TEXT("CLASS_COMBAT_RUNTIME %s style=%s progression_tree=1 active_fp_fx=%d"),
			CurrentStyleFailureCount == 0 ? TEXT("PASS") : TEXT("FAIL"),
			Styles[CurrentStyleIndex].AssetId,
			CountActiveFirstPersonEffects());
		++CurrentStyleIndex;
		Schedule(Combat->GetAttackCooldownRemaining() + 0.18f,
			[Self = AsShared()] { Self->RunNextStyle(); });
	}

	void ReleasePrimary01()
	{
		Combat->HandleAttackReleased(true);
		bLastMontagePlaybackObserved = IsMontagePlaying(Primary01.Get());
		Schedule(0.06f, [Self = AsShared()] { Self->InspectPrimary01(); });
	}

	void InspectPrimary01()
	{
		Check(bLastMontagePlaybackObserved, TEXT("Primary01 Montage가 재생되지 않았습니다."));
		Check(Combat->GetDisplayedAttackSkill() == Primary02.Get(), TEXT("Primary02 콤보 대기가 열리지 않았습니다."));
		const float Delay = Primary01.IsValid() ? Primary01->ComboWindowOpenSeconds + 0.04f : 0.18f;
		Schedule(Delay, [Self = AsShared()] { Self->PressPrimary02(); });
	}

	void PressPrimary02()
	{
		Combat->HandleAttackPressed(true);
		Schedule(0.03f, [Self = AsShared()] { Self->ReleasePrimary02(); });
	}

	void ReleasePrimary02()
	{
		Combat->HandleAttackReleased(true);
		bLastMontagePlaybackObserved = IsMontagePlaying(Primary02.Get());
		Schedule(0.06f, [Self = AsShared()] { Self->InspectPrimary02(); });
	}

	void InspectPrimary02()
	{
		Check(bLastMontagePlaybackObserved, TEXT("Primary02 Montage가 재생되지 않았습니다."));
		Check(Combat->GetDisplayedAttackSkill() == Primary03.Get(), TEXT("Primary03 콤보 대기가 열리지 않았습니다."));
		const float Delay = Primary02.IsValid() ? Primary02->ComboWindowOpenSeconds + 0.04f : 0.18f;
		Schedule(Delay, [Self = AsShared()] { Self->PressPrimary03(); });
	}

	void PressPrimary03()
	{
		Combat->HandleAttackPressed(true);
		Schedule(0.03f, [Self = AsShared()] { Self->ReleasePrimary03(); });
	}

	void ReleasePrimary03()
	{
		Combat->HandleAttackReleased(true);
		bLastMontagePlaybackObserved = IsMontagePlaying(Primary03.Get());
		Schedule(0.06f, [Self = AsShared()] { Self->InspectPrimary03(); });
	}

	void InspectPrimary03()
	{
		Check(bLastMontagePlaybackObserved, TEXT("Primary03 Montage가 재생되지 않았습니다."));
		Schedule(Combat->GetAttackCooldownRemaining() + 0.08f,
			[Self = AsShared()] { Self->BeginChargedAttack(); });
	}

	void BeginChargedAttack()
	{
		Combat->HandleAttackPressed(true);
		Check(CountActiveFirstPersonEffects() >= 1, TEXT("차징 중 1인칭 Niagara가 생성되지 않았습니다."));
		FScreenshotRequest::RequestScreenshot(
			FString::Printf(TEXT("ClassCombat_%s.png"), Styles[CurrentStyleIndex].AssetId),
			false, false);
		const float HoldSeconds = Charged.IsValid() ? Charged->ChargeSeconds + 0.06f : 0.78f;
		Schedule(HoldSeconds, [Self = AsShared()] { Self->ReleaseChargedAttack(); });
	}

	void ReleaseChargedAttack()
	{
		Combat->HandleAttackReleased(true);
		bLastMontagePlaybackObserved = IsMontagePlaying(Charged.Get());
		Schedule(0.07f, [Self = AsShared()] { Self->InspectChargedAttack(); });
	}

	void InspectChargedAttack()
	{
		Check(bLastMontagePlaybackObserved, TEXT("Charged Montage가 재생되지 않았습니다."));
		Check(Combat->GetDisplayedAttackSkill() == Charged.Get(), TEXT("Charged 공격이 실행 상태로 기록되지 않았습니다."));
		UE_LOG(LogRogue10m, Log, TEXT("CLASS_COMBAT_RUNTIME %s style=%s active_fp_fx=%d"),
			CurrentStyleFailureCount == 0 ? TEXT("PASS") : TEXT("FAIL"),
			Styles[CurrentStyleIndex].AssetId,
			CountActiveFirstPersonEffects());
		++CurrentStyleIndex;
		Schedule(Combat->GetAttackCooldownRemaining() + 0.18f, [Self = AsShared()] { Self->RunNextStyle(); });
	}

	void Finish()
	{
		if (bFinished)
		{
			return;
		}
		bFinished = true;
		if (Combat.IsValid())
		{
			Combat->CancelCombatVisuals();
		}
		UE_LOG(LogRogue10m, Display,
			TEXT("RESULT=CLASS_COMBAT_RUNTIME_%s styles=%d combos=%d charged=%d screenshots=%d failures=%d"),
			FailureCount == 0 ? TEXT("PASSED") : TEXT("FAILED"),
			UE_ARRAY_COUNT(Styles),
			(UE_ARRAY_COUNT(Styles) - 1) * 3,
			UE_ARRAY_COUNT(Styles) - 1,
			UE_ARRAY_COUNT(Styles), FailureCount);
		FPlatformMisc::RequestExitWithStatus(false, FailureCount == 0 ? 0 : 1);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<URogue10mCombatComponent> Combat;
	TWeakObjectPtr<URogue10mAttackSkillData> Primary01;
	TWeakObjectPtr<URogue10mAttackSkillData> Primary02;
	TWeakObjectPtr<URogue10mAttackSkillData> Primary03;
	TWeakObjectPtr<URogue10mAttackSkillData> Charged;
	int32 CurrentStyleIndex = 0;
	int32 CurrentStyleFailureCount = 0;
	int32 FailureCount = 0;
	bool bFinished = false;
	bool bLastMontagePlaybackObserved = false;
};

static TSharedPtr<FRuntimeState> ActiveRuntimeTest;

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

static void RunRuntimeTest()
{
	UWorld* World = FindGameWorld();
	ARogue10mCharacter* Character = World
		? Cast<ARogue10mCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
	if (!World || !Character)
	{
		UE_LOG(LogRogue10m, Error, TEXT("RESULT=CLASS_COMBAT_RUNTIME_FAILED reason=no_game_world_or_player"));
		FPlatformMisc::RequestExitWithStatus(false, 1);
		return;
	}
	ActiveRuntimeTest = MakeShared<FRuntimeState>();
	ActiveRuntimeTest->Start(*World, *Character);
}

static FAutoConsoleCommand RunRuntimeTestCommand(
	TEXT("Rogue10m.TestClassCombatPresentation"),
	TEXT("Runs all class combat animations and first-person Niagara in a live game world, then exits."),
	FConsoleCommandDelegate::CreateStatic(&RunRuntimeTest));
}

#endif
