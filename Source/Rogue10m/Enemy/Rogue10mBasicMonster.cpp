// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mBasicMonster.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mAbilitySystemComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mGameMode.h"
#include "Rogue10mGameState.h"
#include "Rogue10mRunCompletionComponent.h"
#include "Engine/GameInstance.h"
#include "Rogue10mMonsterDataAsset.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "AI/Rogue10mMonsterAIController.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

ARogue10mBasicMonster::ARogue10mBasicMonster()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<URogue10mAbilitySystemComponent>(TEXT("Ability System Component"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<URogue10mAttributeSet>(TEXT("Attribute Set"));
	VitalRegenerationComponent = CreateDefaultSubobject<URogue10mVitalRegenerationComponent>(TEXT("Vital Regeneration Component"));
	AttackPatternComponent = CreateDefaultSubobject<URogue10mMonsterAttackPatternComponent>(TEXT("Attack Pattern Component"));

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ARogue10mMonsterAIController::StaticClass();
	GetCharacterMovement()->MaxWalkSpeed = 260.0f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationYaw = false;
}

UAbilitySystemComponent* ARogue10mBasicMonster::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARogue10mBasicMonster::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	float InitialStamina = 1.0f;
	float InitialMana = 1.0f;
	if (MonsterData)
	{
		MonsterDisplayName = MonsterData->DisplayName;
		MonsterLevel = MonsterData->Level;
		switch (MonsterData->MonsterRank)
		{
		case ERogue10mMonsterRank::MidBoss:
			MonsterAttributeText = NSLOCTEXT("Rogue10mMonster", "MidBossRank", "以묎컙 蹂댁뒪");
			break;
		case ERogue10mMonsterRank::FinalBoss:
			MonsterAttributeText = NSLOCTEXT("Rogue10mMonster", "FinalBossRank", "理쒖쥌 蹂댁뒪");
			break;
		default:
			MonsterAttributeText = NSLOCTEXT("Rogue10mMonster", "NormalRank", "?쇰컲");
			break;
		}
		MaxHealth = MonsterData->MaxHealth;
		InitialStamina = MonsterData->MaxStamina;
		InitialMana = MonsterData->MaxMana;
		ExperienceReward = MonsterData->ExperienceReward;
		DetectionRange = MonsterData->DetectionRange;
		StopDistance = MonsterData->StopDistance;
		BehaviorTreeAsset = MonsterData->BehaviorTreeAsset;
		LoseSightRange = MonsterData->LoseSightRange;
		ForgetTargetSeconds = MonsterData->ForgetTargetSeconds;
		PatrolRadius = MonsterData->PatrolRadius;
		PatrolWaitSeconds = MonsterData->PatrolWaitSeconds;
		MaxChaseDistance = MonsterData->MaxChaseDistance;
		AttackSkillData = MonsterData->AttackSkill;
		AttackRange = MonsterData->AttackRange;
		AttackDamage = MonsterData->AttackDamage;
		AttackInterval = MonsterData->AttackInterval;
		bDestroyOnDeath = MonsterData->bDestroyOnDeath;
		GetCharacterMovement()->MaxWalkSpeed = MonsterData->WalkSpeed;
		if (!MonsterData->SkeletalMesh.IsNull()) GetMesh()->SetSkeletalMeshAsset(MonsterData->SkeletalMesh.LoadSynchronous());
		if (MonsterData->AnimationBlueprintClass) GetMesh()->SetAnimInstanceClass(MonsterData->AnimationBlueprintClass);
		GetMesh()->SetRelativeLocation(MonsterData->MeshRelativeLocation);
		GetMesh()->SetRelativeRotation(MonsterData->MeshRelativeRotation);
		GetMesh()->SetRelativeScale3D(MonsterData->MeshRelativeScale);
		VitalRegenerationComponent->ConfigureRegeneration(
			MonsterData->HealthRegenerationPerSecond,
			MonsterData->StaminaRegenerationPerSecond,
			MonsterData->ManaRegenerationPerSecond);
	}
	if (const UGameInstance* Instance = GetGameInstance())
	{
		if (const URogue10mProgressionSubsystem* Progression = Instance->GetSubsystem<URogue10mProgressionSubsystem>())
		{
			const FRogue10mDifficultyModifiers Difficulty = Progression->GetSelectedDifficultyModifiers();
			MaxHealth *= Difficulty.HealthMultiplier;
			AttackDamage *= Difficulty.DamageMultiplier;
			DifficultyDamageMultiplier = Difficulty.DamageMultiplier;
			ExperienceReward = static_cast<int32>(FMath::Clamp(
				FMath::RoundToDouble(static_cast<double>(ExperienceReward) * Difficulty.ExperienceMultiplier),
				0.0, static_cast<double>(MAX_int32)));
		}
	}
	AttributeSet->InitializeVitals(MaxHealth, InitialStamina, InitialMana);
	if (MonsterData && MonsterData->AttackPatternProfile)
	{
		ConfigureAttackPatternProfile(MonsterData->AttackPatternProfile);
	}

	if (ARogue10mMonsterAIController* MonsterController =
		Cast<ARogue10mMonsterAIController>(GetController()))
	{
		MonsterController->InitializeMonsterAI();
	}
}

float ARogue10mBasicMonster::TakeDamage(
	float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !FMath::IsFinite(DamageAmount)) { return 0.0f; }
	const ARogue10mGameState* Run = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (Run && (Run->GetRunPhase() == ERogue10mRunPhase::Victory || Run->GetRunPhase() == ERogue10mRunPhase::Defeat)) { return 0.0f; }
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (bIsDead || !AttributeSet || !FMath::IsFinite(AppliedDamage) || AppliedDamage <= 0.0f)
	{
		return AppliedDamage;
	}

	LastDamageInstigator = EventInstigator;
	ARogue10mCharacter* DirectCharacter = Cast<ARogue10mCharacter>(DamageCauser);
	LastDirectDamageCharacter = DirectCharacter && EventInstigator && EventInstigator->GetPawn() == DirectCharacter
		? DirectCharacter : nullptr;
	AActor* AggroSource = EventInstigator ? EventInstigator->GetPawn() : DamageCauser;
	if (ARogue10mMonsterAIController* MonsterController =
		Cast<ARogue10mMonsterAIController>(GetController()))
	{
		MonsterController->NotifyDamageStimulus(AggroSource, AppliedDamage);
	}
	AttributeSet->SetHealth(AttributeSet->GetHealth() - AppliedDamage);
	UE_LOG(
		LogRogue10m, Log, TEXT("%s ?쇳빐 %.1f, 泥대젰 %.1f / %.1f"),
		*GetNameSafe(this), AppliedDamage, AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());

	ARogue10mPlayerController* PlayerController = EventInstigator
		? Cast<ARogue10mPlayerController>(EventInstigator)
		: Cast<ARogue10mPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (PlayerController)
	{
		PlayerController->AddCombatLogMessage(
			FString::Printf(TEXT("紐ъ뒪?곗뿉寃??쇳빐 %.0f"), AppliedDamage),
			FLinearColor(1.0f, 0.72f, 0.22f, 1.0f));
	}

	if (AttributeSet->GetHealth() <= 0.0f)
	{
		Die();
	}
	else if (AttackPatternComponent)
	{
		AttackPatternComponent->PlayHitReaction();
	}
	return AppliedDamage;
}

bool ARogue10mBasicMonster::CanReceiveRogue10mAttack_Implementation(AActor* AttackSource) const
{
	return !bIsDead && AttackSource != this;
}

FVector ARogue10mBasicMonster::GetRogue10mDamageIndicatorLocation_Implementation() const
{
	FVector BoundsOrigin;
	FVector BoundsExtent;
	GetActorBounds(true, BoundsOrigin, BoundsExtent);
	return BoundsOrigin + FVector(0.0f, 0.0f, BoundsExtent.Z + 30.0f);
}


UBehaviorTree* ARogue10mBasicMonster::ResolveBehaviorTreeAsset() const
{
	return BehaviorTreeAsset.LoadSynchronous();
}

float ARogue10mBasicMonster::GetEffectiveAttackRange() const
{
	if (AttackPatternComponent && AttackPatternComponent->IsEnabled())
	{
		return AttackPatternComponent->GetEngagementRange();
	}
	return AttackSkillData ? AttackSkillData->AttackRange : AttackRange;
}

void ARogue10mBasicMonster::ConfigureAttackPatternProfile(URogue10mMonsterAttackPatternProfile* Profile)
{
	if (AttackPatternComponent) AttackPatternComponent->ConfigureProfile(Profile);
}

bool ARogue10mBasicMonster::IsPatternAttackActive() const
{
	return AttackPatternComponent && AttackPatternComponent->IsEnabled() && AttackPatternComponent->IsAttackActive();
}

void ARogue10mBasicMonster::SetAITarget(ARogue10mCharacter* NewTarget)
{
	TargetCharacter = NewTarget;
}

void ARogue10mBasicMonster::ClearAITarget()
{
	if (AttackPatternComponent && AttackPatternComponent->IsAttackActive()) AttackPatternComponent->CancelAttack();
	TargetCharacter.Reset();
	LockedAttackTarget.Reset();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AttackSequenceTimer);
	}
}

void ARogue10mBasicMonster::ExecuteAICombat(ARogue10mCharacter* Target)
{
	if (!Target || Target->IsDead() || bIsDead)
	{
		return;
	}
	TargetCharacter = Target;
	TryAttackTarget(FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()));
}
void ARogue10mBasicMonster::TryAttackTarget(float DistanceToTarget)
{
	ARogue10mCharacter* Target = TargetCharacter.Get();
	if (AttackPatternComponent && AttackPatternComponent->IsEnabled())
	{
		AttackPatternComponent->TryStartAttack(Target);
		return;
	}
	const float Range = AttackSkillData ? AttackSkillData->AttackRange : AttackRange;
	const float Cooldown = AttackSkillData ? AttackSkillData->AttackCooldown : AttackInterval;
	if (!Target || !GetWorld() || DistanceToTarget > Range || GetWorld()->GetTimerManager().IsTimerActive(AttackSequenceTimer)) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < Cooldown) return;
	LastAttackTime = Now;
	StartMonsterAttackSequence();
}

void ARogue10mBasicMonster::StartMonsterAttackSequence()
{
	CompletedAttackPulses = 0;
	LockedAttackTarget.Reset();
	ExecuteMonsterAttackPulse();
	const int32 Count = !AttackSkillData || AttackSkillData->HitMode == ERogue10mAttackHitMode::Single ? 1 : FMath::Clamp(AttackSkillData->HitCount, 1, 64);
	if (Count > 1 && GetWorld()) GetWorld()->GetTimerManager().SetTimer(AttackSequenceTimer, this, &ARogue10mBasicMonster::ExecuteMonsterAttackPulse, FMath::Max(0.01f, AttackSkillData->HitInterval), true);
}

void ARogue10mBasicMonster::ExecuteMonsterAttackPulse()
{
	ARogue10mCharacter* Target = LockedAttackTarget.IsValid() ? LockedAttackTarget.Get() : TargetCharacter.Get();
	if (!Target || Target->IsDead() || !GetWorld()) { if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AttackSequenceTimer); return; }
	++CompletedAttackPulses;
	const int32 Count = !AttackSkillData || AttackSkillData->HitMode == ERogue10mAttackHitMode::Single ? 1 : FMath::Clamp(AttackSkillData->HitCount, 1, 64);
	const bool bLockedMultiHit = AttackSkillData && AttackSkillData->HitMode == ERogue10mAttackHitMode::MultiHit && LockedAttackTarget.IsValid();
	const bool bCanHit = !AttackSkillData || bLockedMultiHit || IsTargetInsideAttackShape(*Target, *AttackSkillData);
	if (bCanHit)
	{
		if (AttackSkillData && AttackSkillData->HitMode == ERogue10mAttackHitMode::MultiHit && !LockedAttackTarget.IsValid()) LockedAttackTarget = Target;
		float Damage = 0.0f;
		bool bCriticalHit = false;
		if (AttackSkillData)
		{
			Damage = AttackSkillData->RollDamage(AttributeSet, bCriticalHit) * DifficultyDamageMultiplier;
		}
		else
		{
			const float RatioA = AttributeSet ? AttributeSet->GetMinDamageRatio() : 0.9f;
			const float RatioB = AttributeSet ? AttributeSet->GetMaxDamageRatio() : 1.1f;
			Damage = AttackDamage * FMath::FRandRange(FMath::Min(RatioA, RatioB), FMath::Max(RatioA, RatioB));
			const float CriticalChance = AttributeSet ? AttributeSet->GetCriticalChance() : 0.0f;
			bCriticalHit = FMath::FRand() < FMath::Clamp(CriticalChance, 0.0f, 1.0f);
			if (bCriticalHit)
			{
				Damage *= AttributeSet ? FMath::Max(0.0f, AttributeSet->GetCriticalDamageMultiplier()) : 1.5f;
			}
		}
		UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, UDamageType::StaticClass());
		UE_LOG(LogRogue10m, Log, TEXT("%s 怨듦꺽 %d/%d: %s?먭쾶 %.1f ?쇳빐%s"), *GetNameSafe(this), CompletedAttackPulses, Count, *GetNameSafe(Target), Damage, bCriticalHit ? TEXT(" (移섎챸?)") : TEXT(""));
	}
	if (CompletedAttackPulses >= Count || (AttackSkillData && AttackSkillData->HitMode == ERogue10mAttackHitMode::MultiHit && !LockedAttackTarget.IsValid())) GetWorld()->GetTimerManager().ClearTimer(AttackSequenceTimer);
}

bool ARogue10mBasicMonster::IsTargetInsideAttackShape(const ARogue10mCharacter& Target, const URogue10mAttackSkillData& SkillData) const
{
	const FVector Offset = Target.GetActorLocation() - GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const float ForwardDistance = FVector::DotProduct(Forward, Offset);
	switch (SkillData.AttackShape)
	{
	case ERogue10mAttackShape::LinearBox:
		return ForwardDistance >= 0.0f && ForwardDistance <= SkillData.AttackRange && FMath::Abs(FVector::DotProduct(GetActorRightVector(), Offset)) <= SkillData.BoxHalfWidth && FMath::Abs(Offset.Z) <= SkillData.BoxHalfHeight;
	case ERogue10mAttackShape::Projectile:
		return ForwardDistance >= 0.0f && ForwardDistance <= SkillData.AttackRange && FVector::CrossProduct(Offset, Forward).Size() <= SkillData.AttackTraceRadius;
	case ERogue10mAttackShape::Arc:
		return Offset.Size2D() <= SkillData.AttackRange && FVector::DotProduct(Forward, Offset.GetSafeNormal()) >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(SkillData.ArcAngleDegrees, 1.0f, 180.0f) * 0.5f));
	case ERogue10mAttackShape::Circle:
		return FVector::Dist2D(GetActorLocation() + Forward * SkillData.CircleForwardOffset, Target.GetActorLocation()) <= SkillData.AttackRange;
	default: return false;
	}
}
void ARogue10mBasicMonster::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	if (AttackPatternComponent) AttackPatternComponent->CancelAttack();
	int32 AwardedExperience = 0;
	int32 AwardedGold = 0;
	const ARogue10mGameState* Run = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	const bool bRunEnded = Run && (Run->GetRunPhase() == ERogue10mRunPhase::Victory || Run->GetRunPhase() == ERogue10mRunPhase::Defeat);
	if (HasAuthority() && !bCombatTestInstance && !bRunEnded)
	{
		if (ARogue10mPlayerController* RewardController =
			Cast<ARogue10mPlayerController>(LastDamageInstigator.Get()))
		{
			if (ARogue10mPlayerState* RewardState =
				RewardController->GetPlayerState<ARogue10mPlayerState>())
			{
				if (MonsterData && !MonsterData->MonsterId.IsNone())
				{
					RewardState->RecordMonsterDefeat(MonsterData->MonsterId);
				}
				ARogue10mCharacter* RewardCharacter = Cast<ARogue10mCharacter>(RewardController->GetPawn());
				URogue10mTraitRuntimeComponent* Traits = RewardCharacter ? RewardCharacter->GetTraitRuntimeComponent() : nullptr;
				if (Traits && RewardCharacter == LastDirectDamageCharacter.Get()) { Traits->NotifyDirectKill(); }
				if (ExperienceReward > 0)
				{
					if (URogue10mInventoryComponent* Inventory = RewardCharacter ? RewardCharacter->GetInventoryComponent() : nullptr)
					{
						// Preserve the existing difficulty-adjusted base reward; GoldFind only changes gold, never XP.
						const int32 BaseGoldReward = FMath::Max(1, ExperienceReward / 10);
						const int32 GoldReward = Traits ? Traits->CalculateMonsterGoldReward(BaseGoldReward) : BaseGoldReward;
						const int32 PreviousGold = Inventory->GetGold();
						Inventory->AddGold(GoldReward);
						AwardedGold = Inventory->GetGold() - PreviousGold;
						RewardController->AddCombatLogMessage(FString::Printf(TEXT("골드 +%d"), GoldReward), FLinearColor(1.0f, 0.8f, 0.3f));
					}
				}
				if (ExperienceReward > 0)
				{
					RewardState->AddExperience(ExperienceReward);
					AwardedExperience = ExperienceReward;
					RewardController->AddCombatLogMessage(
						FString::Printf(TEXT("%s 처치: 경험치 +%d"),
							*MonsterDisplayName.ToString(), ExperienceReward),
						FLinearColor(0.42f, 0.9f, 0.58f, 1.0f));
					UE_LOG(LogRogue10m, Log, TEXT("%s 처치 보상: 경험치 +%d"),
						*MonsterDisplayName.ToString(), ExperienceReward);
				}
			}
		}
	}
	if (ARogue10mMonsterAIController* MonsterController =
		Cast<ARogue10mMonsterAIController>(GetController()))
	{
		MonsterController->StopMonsterAI();
	}
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AttackSequenceTimer);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	const float DeathPresentationSeconds = AttackPatternComponent ? AttackPatternComponent->PlayDeathPresentation() : 0.0f;
	if (ARogue10mGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogue10mGameMode>() : nullptr)
	{
		if (URogue10mRunCompletionComponent* Completion = Mode->GetRunCompletionComponent())
		{
			Completion->NotifyFinalBossDefeated(*this, LastDamageInstigator.Get(), AwardedExperience, AwardedGold);
		}
	}
	if (HasAuthority()) OnMonsterDied.Broadcast(this);
	UE_LOG(LogRogue10m, Log, TEXT("%s ?щ쭩"), *GetNameSafe(this));

	if (bDestroyOnDeath)
	{
		if (DeathPresentationSeconds > 0.0f) SetLifeSpan(DeathPresentationSeconds);
		else Destroy();
	}
}
void ARogue10mBasicMonster::MarkCombatTestInstance()
{
#if !UE_BUILD_SHIPPING
	if (HasAuthority() && GetWorld() && GetWorld()->GetNetMode() == NM_Standalone) bCombatTestInstance = true;
#endif
}
