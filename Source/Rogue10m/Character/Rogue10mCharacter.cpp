// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacter.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mCharacterDataAsset.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerFeedbackComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mSkillLoadoutDataAsset.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"

ARogue10mCharacter::ARogue10mCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

	AppearanceCameraComponent = CreateDefaultSubobject<URogue10mAppearanceCameraComponent>(TEXT("Appearance Camera Component"));
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	InventoryComponent = CreateDefaultSubobject<URogue10mInventoryComponent>(TEXT("Inventory Component"));
	CombatComponent = CreateDefaultSubobject<URogue10mCombatComponent>(TEXT("Combat Component"));
	BasicBrawlerComponent = CreateDefaultSubobject<URogue10mBasicBrawlerComponent>(TEXT("Basic Brawler Component"));
	CharacterAnimationComponent = CreateDefaultSubobject<URogue10mCharacterAnimationComponent>(
		TEXT("Character Animation Component"));

	FirstPersonPresentationComponent = CreateDefaultSubobject<URogue10mFirstPersonPresentationComponent>(TEXT("First Person Presentation Component"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName(TEXT("NoCollision")));

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(GetMesh(), FName(TEXT("head")));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(
		FVector(-2.8f, 5.89f, 0.0f),
		FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	PlayerFeedbackComponent = CreateDefaultSubobject<URogue10mPlayerFeedbackComponent>(TEXT("Player Feedback Component"));
	VitalRegenerationComponent = CreateDefaultSubobject<URogue10mVitalRegenerationComponent>(TEXT("Vital Regeneration Component"));

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
	NormalWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
}

void ARogue10mCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (AppearanceCameraComponent) { AppearanceCameraComponent->UpdateCameraFromAppearance(); }
	Super::CalcCamera(DeltaTime, OutResult);
	// Inspection only overrides the rendered POV; the head camera remains the combat origin.
	if (AppearanceCameraComponent) { AppearanceCameraComponent->ApplyInspectionView(OutResult); }
}

UAbilitySystemComponent* ARogue10mCharacter::GetAbilitySystemComponent() const
{
	const ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	return State ? State->GetAbilitySystemComponent() : nullptr;
}

void ARogue10mCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilityActorInfo();
	if (FirstPersonPresentationComponent)
	{
		FirstPersonPresentationComponent->RefreshPresentation();
	}
	if (CombatComponent)
	{
		CombatComponent->InitializeSpawnedLoadout();
	}
	if (PlayerFeedbackComponent)
	{
		PlayerFeedbackComponent->InitializeAbilitySystem();
	}
}

void ARogue10mCharacter::UnPossessed()
{
	Super::UnPossessed();
	if (AppearanceCameraComponent) { AppearanceCameraComponent->RefreshAppearanceCamera(); }
}

void ARogue10mCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilityActorInfo();
	if (FirstPersonPresentationComponent)
	{
		FirstPersonPresentationComponent->RefreshPresentation();
	}
	if (CombatComponent)
	{
		CombatComponent->InitializeSpawnedLoadout();
	}
	if (PlayerFeedbackComponent)
	{
		PlayerFeedbackComponent->InitializeAbilitySystem();
	}
}

float ARogue10mCharacter::TakeDamage(
	float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	URogue10mAttributeSet* Attributes = GetRogueAttributeSet();
	const float DefenseMitigatedDamage = Attributes && DamageAmount > 0.0f
		? FMath::Max(1.0f, DamageAmount - Attributes->GetDefense())
		: DamageAmount;
	const float PassiveDamageReduction = CombatComponent
		? CombatComponent->GetUnlockedPassiveDamageReduction() : 0.0f;
	const float MitigatedDamage = DefenseMitigatedDamage > 0.0f
		? DefenseMitigatedDamage * (1.0f - PassiveDamageReduction)
		: DefenseMitigatedDamage;
	const float AppliedDamage = Super::TakeDamage(
		MitigatedDamage, DamageEvent, EventInstigator, DamageCauser);
	if (IsDead() || !Attributes || AppliedDamage <= 0.0f)
	{
		return AppliedDamage;
	}

	Attributes->SetHealth(Attributes->GetHealth() - AppliedDamage);
	if (PlayerFeedbackComponent)
	{
		PlayerFeedbackComponent->NotifyDamage(AppliedDamage);
	}
	UE_LOG(
		LogRogue10m, Log, TEXT("%s 피해 %.1f, 체력 %.1f / %.1f"),
		*GetNameSafe(this), AppliedDamage, Attributes->GetHealth(), Attributes->GetMaxHealth());

	if (ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController()))
	{
		RogueController->NotifyPlayerDamaged(AppliedDamage);
		RogueController->AddCombatLogMessage(
			FString::Printf(TEXT("플레이어 피해 %.0f"), AppliedDamage),
			FLinearColor(1.0f, 0.42f, 0.36f, 1.0f));
	}

	if (Attributes->GetHealth() <= 0.0f)
	{
		Die();
	}
	return AppliedDamage;
}

void ARogue10mCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ARogue10mCharacter::DoJumpStart);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ARogue10mCharacter::DoJumpEnd);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARogue10mCharacter::MoveInput);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ARogue10mCharacter::MoveInput);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARogue10mCharacter::LookInput);
		EnhancedInput->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ARogue10mCharacter::LookInput);
	}
	else
	{
		UE_LOG(LogRogue10m, Error, TEXT("Enhanced Input Component를 찾지 못했습니다."));
	}

	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARogue10mCharacter::DoPrimaryAttackPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ARogue10mCharacter::DoPrimaryAttackReleased);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ARogue10mCharacter::DoSpecialAttackPressed);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ARogue10mCharacter::DoSpecialAttackReleased);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ARogue10mCharacter::DoSprintStart);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ARogue10mCharacter::DoSprintEnd);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ARogue10mCharacter::DoDodge);

	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ARogue10mCharacter::DoQuickSlot1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ARogue10mCharacter::DoQuickSlot2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ARogue10mCharacter::DoQuickSlot3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ARogue10mCharacter::DoQuickSlot4);
	PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ARogue10mCharacter::DoQuickSlot5);
}

void ARogue10mCharacter::MoveInput(const FInputActionValue& Value)
{
	const FVector2D Movement = Value.Get<FVector2D>();
	CachedMovementInput = Movement.GetClampedToMaxSize(1.0f);
	DoMove(Movement.X, Movement.Y);
	if (CharacterAnimationComponent)
	{
		CharacterAnimationComponent->SetMovementState(!CachedMovementInput.IsNearlyZero(), bIsSprinting);
	}
}

void ARogue10mCharacter::LookInput(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	DoAim(Look.X, Look.Y);
}

void ARogue10mCharacter::DoAim(float Yaw, float Pitch)
{
	if (IsDead() || IsBlockingWindowVisible() || !GetController())
	{
		return;
	}

	const ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController());
	const float SensitivityX = RogueController ? RogueController->GetLookSensitivityX() : 1.0f;
	const float SensitivityY = RogueController ? RogueController->GetLookSensitivityY() : 1.0f;
	AddControllerYawInput(Yaw * SensitivityX);
	AddControllerPitchInput(Pitch * SensitivityY);
}

void ARogue10mCharacter::DoMove(float Right, float Forward)
{
	if (bIsDodging)
	{
		return;
	}
	if (IsBlockingWindowVisible())
	{
		SetSprinting(false);
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	if (!IsDead() && GetController())
	{
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ARogue10mCharacter::DoJumpStart()
{
	if (BasicBrawlerComponent && BasicBrawlerComponent->IsBasicBrawlerActive())
	{
		BasicBrawlerComponent->CancelInput();
	}
	if (IsDead() || IsBlockingWindowVisible() || bIsDodging)
	{
		return;
	}

	SetSprinting(false);
	Jump();
}

void ARogue10mCharacter::DoJumpEnd()
{
	StopJumping();
}

void ARogue10mCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	if (CharacterAnimationComponent)
	{
		CharacterAnimationComponent->NotifyJump(JumpCurrentCount);
	}
}

void ARogue10mCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (CharacterAnimationComponent)
	{
		CharacterAnimationComponent->NotifyLanded();
	}
}

void ARogue10mCharacter::DoDodge()
{
	UWorld* World = GetWorld();
	if (!World || IsDead() || IsBlockingWindowVisible() || GetCharacterMovement()->IsFalling()
		|| bIsDodging || World->GetTimeSeconds() < NextDodgeAllowedTime)
	{
		return;
	}

	SetSprinting(false);
	const URogue10mDodgeSkillDataAsset* DodgeSkill = CombatComponent
		? CombatComponent->GetActiveDodgeSkill()
		: nullptr;
	const float ActiveDistance = DodgeSkill ? DodgeSkill->Distance : DodgeDistance;
	const float ActiveDuration = DodgeSkill ? DodgeSkill->Duration : DodgeDuration;
	const float ActiveCooldown = DodgeSkill ? DodgeSkill->Cooldown : DodgeCooldown;
	const float StaminaCost = DodgeSkill ? DodgeSkill->StaminaCost : 0.0f;
	URogue10mAttributeSet* Attributes = GetRogueAttributeSet();
	if (StaminaCost > 0.0f && (!Attributes || Attributes->GetStamina() < StaminaCost))
	{
		return;
	}
	if (Attributes && StaminaCost > 0.0f)
	{
		Attributes->SetStamina(Attributes->GetStamina() - StaminaCost);
	}

	FVector Direction = GetActorForwardVector();
	if (!CachedMovementInput.IsNearlyZero())
	{
		Direction = GetActorRightVector() * CachedMovementInput.X + GetActorForwardVector() * CachedMovementInput.Y;
	}
	Direction.Z = 0.0f;
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero()) Direction = GetActorForwardVector();

	bIsDodging = true;
	NextDodgeAllowedTime = World->GetTimeSeconds() + ActiveCooldown;
	GetCharacterMovement()->Velocity = Direction * (ActiveDistance / FMath::Max(0.05f, ActiveDuration));
	if (CharacterAnimationComponent)
	{
		CharacterAnimationComponent->NotifyDodge();
	}
	World->GetTimerManager().SetTimer(DodgeTimerHandle, this, &ARogue10mCharacter::FinishDodge, ActiveDuration, false);
}

void ARogue10mCharacter::FinishDodge()
{
	bIsDodging = false;
	FVector Velocity = GetCharacterMovement()->Velocity;
	Velocity.X = 0.0f;
	Velocity.Y = 0.0f;
	GetCharacterMovement()->Velocity = Velocity;
}

void ARogue10mCharacter::DoSprintStart()
{
	if (IsDead() || IsBlockingWindowVisible())
	{
		return;
	}

	const URogue10mAttributeSet* Attributes = GetRogueAttributeSet();
	if (!Attributes || Attributes->GetStamina() <= 0.0f)
	{
		if (PlayerFeedbackComponent)
		{
			PlayerFeedbackComponent->NotifyInsufficientStamina();
		}
		return;
	}
	SetSprinting(true);
}

void ARogue10mCharacter::DoSprintEnd()
{
	SetSprinting(false);
}

void ARogue10mCharacter::SetSprinting(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;
	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintWalkSpeed : NormalWalkSpeed;
	if (PlayerFeedbackComponent)
	{
		PlayerFeedbackComponent->SetSprinting(bIsSprinting);
	}
	if (CharacterAnimationComponent)
	{
		CharacterAnimationComponent->SetMovementState(!CachedMovementInput.IsNearlyZero(), bIsSprinting);
	}

	if (UWorld* World = GetWorld())
	{
		if (bIsSprinting)
		{
			World->GetTimerManager().SetTimer(
				SprintStaminaTimerHandle, this, &ARogue10mCharacter::ConsumeSprintStamina,
				SprintStaminaDrainInterval, true, SprintStaminaDrainInterval);
		}
		else
		{
			World->GetTimerManager().ClearTimer(SprintStaminaTimerHandle);
		}
	}
}

void ARogue10mCharacter::ConsumeSprintStamina()
{
	if (!bIsSprinting || IsDead() || IsBlockingWindowVisible())
	{
		SetSprinting(false);
		return;
	}

	URogue10mAttributeSet* Attributes = GetRogueAttributeSet();
	const float StaminaCost = SprintStaminaCostPerSecond * SprintStaminaDrainInterval;
	if (!Attributes || !Attributes->ConsumeStamina(StaminaCost))
	{
		SetSprinting(false);
		if (PlayerFeedbackComponent)
		{
			PlayerFeedbackComponent->NotifyInsufficientStamina();
		}
	}
}

void ARogue10mCharacter::DoPrimaryAttackPressed()
{
	if (CombatComponent)
	{
		CombatComponent->HandleAttackPressed(true);
	}
}

void ARogue10mCharacter::DoPrimaryAttackReleased()
{
	if (CombatComponent)
	{
		CombatComponent->HandleAttackReleased(true);
	}
}

void ARogue10mCharacter::DoSpecialAttackPressed()
{
	if (CombatComponent)
	{
		CombatComponent->HandleAttackPressed(false);
	}
}

void ARogue10mCharacter::DoSpecialAttackReleased()
{
	if (CombatComponent)
	{
		CombatComponent->HandleAttackReleased(false);
	}
}

void ARogue10mCharacter::DoUnarmedAttack()
{
	if (CombatComponent)
	{
		CombatComponent->ActivateQuickSlot(1);
	}
}

void ARogue10mCharacter::DoToggleInventory()
{
	if (!IsDead())
	{
		if (ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController()))
		{
			RogueController->ToggleInventory();
		}
	}
}

void ARogue10mCharacter::DoToggleItemWindow()
{
	if (!IsDead())
	{
		if (ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController()))
		{
			RogueController->ToggleItemWindow();
		}
	}
}

void ARogue10mCharacter::DoToggleSkillTree()
{
	if (!IsDead())
	{
		if (ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController()))
		{
			RogueController->ToggleSkillTree();
		}
	}
}

void ARogue10mCharacter::DoToggleSettings()
{
	if (!IsDead())
	{
		if (ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController()))
		{
			RogueController->ToggleSettings();
		}
	}
}

void ARogue10mCharacter::DoToggleCombatLog()
{
	if (ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController()))
	{
		const bool bVisible = RogueController->ToggleCombatLogVisible();
		if (bVisible)
		{
			RogueController->AddCombatLogMessage(TEXT("전투 로그 표시 켜짐"), FLinearColor(0.58f, 0.85f, 1.0f, 1.0f));
		}
	}
}

void ARogue10mCharacter::DoQuickSlot1() { ActivateQuickSlot(1); }
void ARogue10mCharacter::DoQuickSlot2() { ActivateQuickSlot(2); }
void ARogue10mCharacter::DoQuickSlot3() { ActivateQuickSlot(3); }
void ARogue10mCharacter::DoQuickSlot4() { ActivateQuickSlot(4); }
void ARogue10mCharacter::DoQuickSlot5() { ActivateQuickSlot(5); }

void ARogue10mCharacter::Die()
{
	if (IsDead())
	{
		return;
	}

	SetSprinting(false);
	if (CharacterAnimationComponent)
	{
		CharacterAnimationComponent->StopMotionEffects();
	}
	if (CombatComponent)
	{
		CombatComponent->CancelCombatVisuals();
	}

	if (ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>())
	{
		State->SetCharacterDead(true);
	}
	if (ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController()))
	{
		RogueController->CloseAllBlockingPanels();
		DisableInput(RogueController);
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	BP_OnRunDeath();
	UE_LOG(LogRogue10m, Log, TEXT("%s 사망"), *GetNameSafe(this));
}

bool ARogue10mCharacter::IsDead() const
{
	const ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	return State && State->IsCharacterDead();
}

FText ARogue10mCharacter::GetCharacterDisplayName() const
{
	const ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	return State ? State->GetCharacterDisplayName() : FText::FromString(TEXT("로그"));
}

FText ARogue10mCharacter::GetCharacterJobName() const
{
	const ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	return State ? State->GetCharacterJobName() : NSLOCTEXT("Rogue10mCharacter", "DefaultAdventurerJob", "모험가");
}

ERogue10mWeaponType ARogue10mCharacter::GetEquippedWeaponType() const
{
	const ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	return State ? State->GetEquippedWeaponType() : ERogue10mWeaponType::Unarmed;
}

void ARogue10mCharacter::SetEquippedWeaponType(ERogue10mWeaponType NewWeaponType)
{
	if (ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>())
	{
		State->SetEquippedWeaponType(NewWeaponType);
		if (CombatComponent)
		{
			CombatComponent->HandleEquippedWeaponChanged();
		}
	}
}

URogue10mAttributeSet* ARogue10mCharacter::GetRogueAttributeSet() const
{
	const ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	return State ? State->GetRogueAttributeSet() : nullptr;
}

FRogue10mCharacterStatSnapshot ARogue10mCharacter::GetCharacterStatSnapshot() const
{
	FRogue10mCharacterStatSnapshot Snapshot;
	if (const URogue10mCharacterDataAsset* CharacterData =
		CombatComponent ? CombatComponent->GetCharacterData() : nullptr)
	{
		Snapshot.Base = CharacterData->GetBaseStats();
	}
	if (InventoryComponent)
	{
		Snapshot.Equipment = InventoryComponent->GetEquipmentStatModifiers();
	}
	Snapshot.Recalculate();
	return Snapshot;
}

bool ARogue10mCharacter::ApplyCharacterProfile(const FRogue10mCharacterProfile& Profile)
{
	if (!Profile.IsValid())
	{
		UE_LOG(LogRogue10m, Error, TEXT("유효하지 않은 캐릭터 프로필은 적용할 수 없습니다."));
		return false;
	}

	CharacterAppearance = Profile.Appearance;
	if (FirstPersonPresentationComponent) { FirstPersonPresentationComponent->RefreshPresentation(); }
	if (ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>())
	{
		FText JobName = NSLOCTEXT("Rogue10mCharacter", "DefaultAdventurerJob", "모험가");
		if (const URogue10mCharacterDataAsset* CharacterData =
			CombatComponent ? CombatComponent->GetCharacterData() : nullptr)
		{
			if (!CharacterData->JobName.IsEmpty())
			{
				JobName = CharacterData->JobName;
			}
		}
		State->SetCharacterProfileIdentity(FText::FromString(Profile.CharacterName), JobName);
	}
	return true;
}

void ARogue10mCharacter::RefreshCharacterStats(bool bRestoreVitals)
{
	const FRogue10mCharacterStatSnapshot Snapshot = GetCharacterStatSnapshot();
	URogue10mAttributeSet* Attributes = GetRogueAttributeSet();
	if (Attributes && HasAuthority())
	{
		const float PreviousMaxHealth = FMath::Max(1.0f, Attributes->GetMaxHealth());
		const float HealthRatio = FMath::Clamp(Attributes->GetHealth() / PreviousMaxHealth, 0.0f, 1.0f);
		const float PreviousMaxStamina = FMath::Max(1.0f, Attributes->GetMaxStamina());
		const float StaminaRatio = FMath::Clamp(Attributes->GetStamina() / PreviousMaxStamina, 0.0f, 1.0f);
		const float PreviousMaxMana = FMath::Max(1.0f, Attributes->GetMaxMana());
		const float ManaRatio = FMath::Clamp(Attributes->GetMana() / PreviousMaxMana, 0.0f, 1.0f);

		Attributes->SetMaxHealth(Snapshot.MaxHealth);
		Attributes->SetMaxStamina(Snapshot.Base.MaxStamina);
		Attributes->SetMaxMana(Snapshot.Base.MaxMana);
		Attributes->SetAttackPower(Snapshot.AttackPower);
		Attributes->SetDefense(Snapshot.Defense);
		Attributes->SetCriticalChance(Snapshot.CriticalChance);
		Attributes->SetCriticalDamageMultiplier(Snapshot.CriticalDamageMultiplier);
		Attributes->SetAttackSpeedMultiplier(Snapshot.AttackSpeedMultiplier);
		Attributes->SetMoveSpeed(Snapshot.MoveSpeed);

		if (bRestoreVitals)
		{
			Attributes->RestoreVitals();
		}
		else
		{
			Attributes->SetHealth(Snapshot.MaxHealth * HealthRatio);
			Attributes->SetStamina(Snapshot.Base.MaxStamina * StaminaRatio);
			Attributes->SetMana(Snapshot.Base.MaxMana * ManaRatio);
		}
	}

	NormalWalkSpeed = Snapshot.MoveSpeed;
	SprintWalkSpeed = Snapshot.SprintSpeed;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = bIsSprinting ? SprintWalkSpeed : NormalWalkSpeed;
	}
}

const URogue10mAttackSkillData* ARogue10mCharacter::GetDisplayedAttackSkillForHUD() const
{
	return CombatComponent ? CombatComponent->GetDisplayedAttackSkill() : nullptr;
}

float ARogue10mCharacter::GetAttackCooldownRemaining() const
{
	return CombatComponent ? CombatComponent->GetAttackCooldownRemaining() : 0.0f;
}

float ARogue10mCharacter::GetAttackCooldownDuration() const
{
	return CombatComponent ? CombatComponent->GetAttackCooldownDuration() : 0.0f;
}

bool ARogue10mCharacter::ExecutePendingAttackSkillFromAbility()
{
	return CombatComponent && CombatComponent->ExecutePendingAttackSkillFromAbility();
}

USkeletalMeshComponent* ARogue10mCharacter::GetAnimationPlaybackMesh() const
{
	if (GetMesh() && GetMesh()->GetAnimInstance())
	{
		return GetMesh();
	}
	return FirstPersonMesh;
}

bool ARogue10mCharacter::PlayCommonMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		return false;
	}

	bool bPlayed = false;
	auto TryPlay = [&bPlayed, Montage, PlayRate](USkeletalMeshComponent* MeshComponent)
	{
		if (UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr)
		{
			bPlayed |= AnimInstance->Montage_Play(Montage, FMath::Max(0.01f, PlayRate)) > 0.0f;
		}
	};

	USkeletalMeshComponent* PlaybackMesh = GetAnimationPlaybackMesh();
	TryPlay(PlaybackMesh);
	if (FirstPersonMesh != PlaybackMesh && !(AppearanceCameraComponent && AppearanceCameraComponent->IsAppearanceCameraActive()))
	{
		TryPlay(FirstPersonMesh);
	}
	if (!bPlayed)
	{
		UE_LOG(
			LogRogue10m, Warning, TEXT("%s 공통 Montage 재생 실패: %s"),
			*GetNameSafe(this), *GetNameSafe(Montage));
	}
	return bPlayed;
}

bool ARogue10mCharacter::IsBlockingWindowVisible() const
{
	const ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController());
	return RogueController && RogueController->IsAnyBlockingWindowVisible();
}

void ARogue10mCharacter::ActivateQuickSlot(int32 SlotNumber)
{
	if (ARogue10mPlayerController* RogueController = Cast<ARogue10mPlayerController>(GetController()))
	{
		RogueController->ActivateQuickSlot(SlotNumber);
	}
}

void ARogue10mCharacter::InitializeAbilityActorInfo()
{
	ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	if (UAbilitySystemComponent* AbilitySystem = State ? State->GetAbilitySystemComponent() : nullptr)
	{
		AbilitySystem->InitAbilityActorInfo(State, this);
	}
	RefreshCharacterStats(true);
}
