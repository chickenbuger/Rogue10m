// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rogue10mCharacter.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Rogue10mMouseCombatInputComponent.h"
#include "CollisionQueryParams.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Rogue10m.h"
#include "Rogue10mInputKeys.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacterAnimationComponent.h"
#include "Rogue10mWeaponVisualComponent.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mSkillTreeCombatComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mBasicBrawlerComponent.h"
#include "Rogue10mCharacterDataAsset.h"
#include "Rogue10mFirstPersonPresentationComponent.h"
#include "Rogue10mAppearanceCameraComponent.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerFeedbackComponent.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mGameState.h"
#include "Engine/World.h"
#include "Rogue10mTownProgressionComponent.h"
#include "Rogue10mTraitRuntimeComponent.h"
#include "Engine/GameInstance.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "TimerManager.h"

ARogue10mCharacter::ARogue10mCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

	AppearanceCameraComponent = CreateDefaultSubobject<URogue10mAppearanceCameraComponent>(TEXT("Appearance Camera Component"));
	MouseCombatInputComponent = CreateDefaultSubobject<URogue10mMouseCombatInputComponent>(TEXT("Mouse Combat Input Component"));
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	InventoryComponent = CreateDefaultSubobject<URogue10mInventoryComponent>(TEXT("Inventory Component"));
	CombatComponent = CreateDefaultSubobject<URogue10mCombatComponent>(TEXT("Combat Component"));
	SkillTreeCombatComponent = CreateDefaultSubobject<URogue10mSkillTreeCombatComponent>(TEXT("Skill Tree Combat Component"));
	BasicBrawlerComponent = CreateDefaultSubobject<URogue10mBasicBrawlerComponent>(TEXT("Basic Brawler Component"));
	CharacterAnimationComponent = CreateDefaultSubobject<URogue10mCharacterAnimationComponent>(
		TEXT("Character Animation Component"));
 WeaponVisualComponent = CreateDefaultSubobject<URogue10mWeaponVisualComponent>(TEXT("Weapon Visual Component"));

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

	ThirdPersonCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Third Person Camera Boom"));
	ThirdPersonCameraBoom->SetupAttachment(GetCapsuleComponent());
	ThirdPersonCameraBoom->TargetArmLength = 320.0f;
	ThirdPersonCameraBoom->TargetOffset = FVector(0.0f, 0.0f, 58.0f);
	ThirdPersonCameraBoom->SocketOffset = FVector(0.0f, 85.0f, 10.0f);
	ThirdPersonCameraBoom->bUsePawnControlRotation = true;
	ThirdPersonCameraBoom->bDoCollisionTest = true;
	ThirdPersonCameraBoom->ProbeChannel = ECC_Camera;
	ThirdPersonCameraBoom->ProbeSize = 14.0f;
	ThirdPersonCameraBoom->bEnableCameraLag = false;
	ThirdPersonCameraBoom->bEnableCameraRotationLag = false;
	ThirdPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Third Person Camera"));
	ThirdPersonCameraComponent->SetupAttachment(ThirdPersonCameraBoom, USpringArmComponent::SocketName);
	ThirdPersonCameraComponent->bUsePawnControlRotation = false;
	ThirdPersonCameraComponent->FieldOfView = 90.0f;

	PlayerFeedbackComponent = CreateDefaultSubobject<URogue10mPlayerFeedbackComponent>(TEXT("Player Feedback Component"));
	VitalRegenerationComponent = CreateDefaultSubobject<URogue10mVitalRegenerationComponent>(TEXT("Vital Regeneration Component"));
	TraitRuntimeComponent = CreateDefaultSubobject<URogue10mTraitRuntimeComponent>(TEXT("Trait Runtime Component"));

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
	NormalWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
}

void ARogue10mCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bUseThirdPersonGameplay && ThirdPersonCameraComponent)
	{
		ThirdPersonCameraComponent->GetCameraView(DeltaTime, OutResult);
		return;
	}
	if (AppearanceCameraComponent) { AppearanceCameraComponent->UpdateCameraFromAppearance(); }
	Super::CalcCamera(DeltaTime, OutResult);
	if (AppearanceCameraComponent && AppearanceCameraComponent->IsWeightedFirstPersonActive())
	{
		OutResult.FOV = AppearanceCameraComponent->GetWeightedFirstPersonSettings().WorldFieldOfView;
	}
	// Inspection only overrides the rendered POV; the head camera remains the combat origin.
	if (AppearanceCameraComponent) { AppearanceCameraComponent->ApplyInspectionView(OutResult); }
}

UCameraComponent* ARogue10mCharacter::GetGameplayCameraComponent() const
{
	return bUseThirdPersonGameplay ? ThirdPersonCameraComponent.Get() : FirstPersonCameraComponent.Get();
}

FTransform ARogue10mCharacter::GetCombatViewTransform()
{
	if (!bUseThirdPersonGameplay)
	{
		if (AppearanceCameraComponent) { AppearanceCameraComponent->UpdateCameraFromAppearance(); }
		return FirstPersonCameraComponent ? FirstPersonCameraComponent->GetComponentTransform() : GetActorTransform();
	}
	const FVector Origin = GetCapsuleComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, BaseEyeHeight);
	const UCameraComponent* Camera = GetGameplayCameraComponent();
	const FVector ViewOrigin = Camera ? Camera->GetComponentLocation() : Origin;
	const FVector ViewDirection = Camera ? Camera->GetForwardVector() : GetControlRotation().Vector();
	FVector AimPoint = ViewOrigin + ViewDirection * 10000.0f;
	if (UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(Rogue10mThirdPersonAim), true, this);
		if (World->LineTraceSingleByChannel(Hit, ViewOrigin, AimPoint, ECC_Visibility, Query)
			&& FVector::DotProduct(Hit.ImpactPoint - Origin, ViewDirection) > 10.0f)
		{
			AimPoint = Hit.ImpactPoint;
		}
	}
	const FVector Direction = (AimPoint - Origin).GetSafeNormal(SMALL_NUMBER, ViewDirection);
	return FTransform(Direction.Rotation(), Origin);
}

void ARogue10mCharacter::CancelPendingMouseAttackInput()
{
	if (MouseCombatInputComponent) { MouseCombatInputComponent->CancelInput(false); }
}

bool ARogue10mCharacter::HasPendingMouseAttackInput() const
{
	return MouseCombatInputComponent && MouseCombatInputComponent->IsInputHeld();
}

void ARogue10mCharacter::ApplyInitialThirdPersonViewPitchOnce(APlayerController& PlayerController)
{
	if (!bUseThirdPersonGameplay || bThirdPersonInitialViewHandled || !HasActorBegunPlay()
		|| !PlayerController.IsLocalController() || PlayerController.GetPawn() != this) { return; }
	bThirdPersonInitialViewHandled = true;
	FRotator Rotation = PlayerController.GetControlRotation();
	if (!Rotation.ContainsNaN() && FMath::IsNearlyZero(FRotator::NormalizeAxis(Rotation.Pitch), 0.01f))
	{
		Rotation.Pitch = -12.0f;
		PlayerController.SetControlRotation(Rotation);
	}
}

void ARogue10mCharacter::RefreshThirdPersonPresentation()
{
	if (!bUseThirdPersonGameplay) { return; }
	if (AppearanceCameraComponent) { AppearanceCameraComponent->RefreshAppearanceCamera(); }
	if (FirstPersonPresentationComponent) { FirstPersonPresentationComponent->RefreshPresentation(); }
	FirstPersonCameraComponent->SetActive(false);
	ThirdPersonCameraComponent->SetActive(true);
	FirstPersonMesh->SetVisibility(false, false);
	FirstPersonMesh->SetHiddenInGame(true, false);
	FirstPersonMesh->bPauseAnims = true;
	FirstPersonMesh->SetComponentTickEnabled(false);
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetOwnerNoSee(false);
	Body->SetOnlyOwnerSee(false);
	Body->SetVisibility(true, false);
	Body->SetHiddenInGame(false, false);
	Body->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
    Body->SetReceivesDecals(false);
	Body->UnHideBoneByName(TEXT("head"));
	for (USceneComponent* Child : Body->GetAttachChildren())
	{
		if (auto* Part = Cast<USkeletalMeshComponent>(Child); Part && Part != FirstPersonMesh)
		{
			Part->SetOwnerNoSee(false);
			Part->SetOnlyOwnerSee(false);
			Part->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
            Part->SetReceivesDecals(false);
		}
	}
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

UAbilitySystemComponent* ARogue10mCharacter::GetAbilitySystemComponent() const
{
	const ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>();
	return State ? State->GetAbilitySystemComponent() : nullptr;
}

void ARogue10mCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// Initial map entry only resolves the selected Pawn class. Apply its profile
	// now that possession has supplied PlayerState, before progression attaches.
	if (UGameInstance* Instance = GetGameInstance())
	{
		const URogue10mCharacterProfileSubsystem* Profiles =
			Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		if (const FRogue10mCharacterProfile* Profile = Profiles ? Profiles->GetSelectedProfile() : nullptr)
		{
			ApplyCharacterProfile(*Profile);
		}
	}
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
	if (HasActorBegunPlay() && GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>()->AttachCharacter(*this);
	}
}

void ARogue10mCharacter::BeginPlay()
{
	Super::BeginPlay();
	RefreshCommonMobility();
	RefreshThirdPersonPresentation();
 if (WeaponVisualComponent) { WeaponVisualComponent->RefreshWeaponVisuals(); }
	if (GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>()->AttachCharacter(*this);
	}
}

void ARogue10mCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>()->DetachCharacter(*this);
	}
	if (MouseCombatInputComponent) { MouseCombatInputComponent->CancelInput(); }
	Super::EndPlay(EndPlayReason);
}

void ARogue10mCharacter::UnPossessed()
{
	ClearCommonMobilityActions();
	if (MouseCombatInputComponent) { MouseCombatInputComponent->CancelInput(); }
	if (GetGameInstance())
	{
		GetGameInstance()->GetSubsystem<URogue10mProgressionSubsystem>()->DetachCharacter(*this);
	}
	if (CharacterAnimationComponent) { CharacterAnimationComponent->ResetFighterCombatStance(); }
	Super::UnPossessed();
	RefreshCommonMobility();
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
	if (!HasAuthority() || !FMath::IsFinite(DamageAmount)) { return 0.0f; }
	const ARogue10mGameState* Run = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr;
	if (Run && Run->IsRunTerminal()) { return 0.0f; }
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
	if (IsDead() || !Attributes || !FMath::IsFinite(AppliedDamage) || AppliedDamage <= 0.0f)
	{
		return AppliedDamage;
	}
	// A confirmed hit interrupts owned attacks; health costs/healing and rejected damage do not.
	if (CombatComponent) { CombatComponent->InterruptAttacksOnHit(); }

	if (!TraitRuntimeComponent || !TraitRuntimeComponent->TryPreventLethalDamage(AppliedDamage, DamageCauser))
	{
		const float HealthBefore = Attributes->GetHealth();
		Attributes->SetHealth(HealthBefore - AppliedDamage);
		const float HealthAfter = Attributes->GetHealth();
		if (CharacterAnimationComponent && HealthAfter > 0.0f && HealthAfter < HealthBefore)
		{
			FVector ShotDirection = FVector::ZeroVector;
			if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
			{
				ShotDirection = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection;
			}
			else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
			{
				ShotDirection = GetActorLocation() - static_cast<const FRadialDamageEvent&>(DamageEvent).Origin;
			}
			if (ShotDirection.ContainsNaN() || ShotDirection.IsNearlyZero())
			{
				const AActor* Origin = DamageCauser != this ? DamageCauser : nullptr;
				if (!Origin && EventInstigator) { Origin = EventInstigator->GetPawn(); }
				if (Origin) { ShotDirection = GetActorLocation() - Origin->GetActorLocation(); }
			}
			CharacterAnimationComponent->NotifyNonlethalHit(ShotDirection);
		}
	}
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
	// Fixed skill slots are dispatched here, never through saved arbitrary key mappings.
	for (const FKey Key : {EKeys::Q, EKeys::E, EKeys::R})
	{
		PlayerInputComponent->BindKey(Key, IE_Pressed, this, &ARogue10mCharacter::DoEquippedSkillPressed);
		PlayerInputComponent->BindKey(Key, IE_Released, this, &ARogue10mCharacter::DoEquippedSkillReleased);
	}
	PlayerInputComponent->BindKey(Rogue10mInput::IdentityKey(), IE_Pressed, this, &ARogue10mCharacter::DoWeaponIdentity);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ARogue10mCharacter::DoSprintStart);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ARogue10mCharacter::DoSprintEnd);
	PlayerInputComponent->BindKey(Rogue10mInput::DodgeKey(), IE_Pressed, this, &ARogue10mCharacter::DoDodge);

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
	BeginLandingStability();
	if (CharacterAnimationComponent)
	{
		CharacterAnimationComponent->NotifyLanded();
	}
}

void ARogue10mCharacter::DoSprintStart()
{
	if (IsDead() || IsBlockingWindowVisible() || IsRolling())
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
	RefreshMovementSpeed();
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
	if (MouseCombatInputComponent) { MouseCombatInputComponent->Press(true); }
}

void ARogue10mCharacter::DoPrimaryAttackReleased()
{
	if (MouseCombatInputComponent) { MouseCombatInputComponent->Release(true); }
}

void ARogue10mCharacter::DoSpecialAttackPressed()
{
	if (MouseCombatInputComponent) { MouseCombatInputComponent->Press(false); }
}

void ARogue10mCharacter::DoSpecialAttackReleased()
{
	if (MouseCombatInputComponent) { MouseCombatInputComponent->Release(false); }
}

void ARogue10mCharacter::DoEquippedSkillPressed(FKey Key)
{
	if (!CombatComponent || (Key != EKeys::Q && Key != EKeys::E && Key != EKeys::R)) { return; }
	const auto* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController) { return; }
	// Preserve the legacy skill-binding contract: modifier chords cannot start skills.
	for (const FKey Modifier : {EKeys::LeftShift, EKeys::RightShift, EKeys::LeftControl, EKeys::RightControl,
		EKeys::LeftAlt, EKeys::RightAlt, EKeys::LeftCommand, EKeys::RightCommand})
	{
		if (PlayerController->IsInputKeyDown(Modifier)) { return; }
	}
	const auto Slot = Key == EKeys::Q ? ERogue10mAttackInputSlot::QSkill
		: Key == EKeys::E ? ERogue10mAttackInputSlot::ESkill : ERogue10mAttackInputSlot::RSkill;
	CombatComponent->HandleEquippedSkillPressed(Slot);
}

void ARogue10mCharacter::DoEquippedSkillReleased(FKey Key)
{
	if (!CombatComponent || (Key != EKeys::Q && Key != EKeys::E && Key != EKeys::R)) { return; }
	// Always deliver release, including when a modifier was pressed after the skill key.
	const auto Slot = Key == EKeys::Q ? ERogue10mAttackInputSlot::QSkill
		: Key == EKeys::E ? ERogue10mAttackInputSlot::ESkill : ERogue10mAttackInputSlot::RSkill;
	CombatComponent->HandleEquippedSkillReleased(Slot);
}

void ARogue10mCharacter::DoWeaponIdentity()
{
	if (CombatComponent && !IsDead() && !IsBlockingWindowVisible()) { CombatComponent->TryActivateWeaponIdentity(); }
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
	ClearCommonMobilityActions();
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
	if (ARogue10mGameState* Run = GetWorld() ? GetWorld()->GetGameState<ARogue10mGameState>() : nullptr)
	{
		Run->FinishRun(false);
	}
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
	ApplyEquippedWeaponType(NewWeaponType, true);
}

void ARogue10mCharacter::RestoreEquippedWeaponType(ERogue10mWeaponType NewWeaponType)
{
	ApplyEquippedWeaponType(NewWeaponType, false);
}

bool ARogue10mCharacter::CanChangeWeapon() const
{
	const URogue10mAttributeSet* Attributes = GetRogueAttributeSet();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (IsDead() || !Attributes || Attributes->GetHealth() <= 0.0f || bIsDodging || !Movement
		|| (Movement->MovementMode != MOVE_Walking && Movement->MovementMode != MOVE_NavWalking)
		|| (CombatComponent && CombatComponent->IsWeaponChangeBlocked())) { return false; }
	// Existing non-locomotion montages (including hit/cast presentation) are a
	// conservative busy signal; combat stance duration is deliberately not one.
	for (const USkeletalMeshComponent* PlaybackComponent : {GetAnimationPlaybackMesh(), GetFirstPersonMesh()})
	{
		// Inactive first-person presentation must not block third-person gameplay.
		if (bUseThirdPersonGameplay && PlaybackComponent == FirstPersonMesh) { continue; }
		if (const UAnimInstance* Anim = PlaybackComponent ? PlaybackComponent->GetAnimInstance() : nullptr;
			Anim && Anim->IsAnyMontagePlaying()) { return false; }
	}
	return true;
}

void ARogue10mCharacter::NotifyWeaponEquipmentChanged(ERogue10mWeaponType NewWeaponType)
{
	ApplyEquippedWeaponType(NewWeaponType, true, true);
}

void ARogue10mCharacter::ApplyEquippedWeaponType(ERogue10mWeaponType NewWeaponType, bool bApplySwapCooldown, bool bItemChanged)
{
	if (ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>())
	{
		const bool bWeaponTypeChanged = State->GetEquippedWeaponType() != NewWeaponType;
		State->SetEquippedWeaponType(NewWeaponType);
  if (WeaponVisualComponent) { WeaponVisualComponent->RefreshWeaponVisuals(); }
		if (CombatComponent)
		{
			CombatComponent->HandleEquippedWeaponChanged(bApplySwapCooldown && (bWeaponTypeChanged || bItemChanged));
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
	if (const ARogue10mPlayerState* State = GetPlayerState<ARogue10mPlayerState>())
	{
		if (const URogue10mSkillTreeProgressionComponent* Tree = State->GetSkillTreeProgressionComponent())
		{
			Snapshot.Equipment.Accumulate(Tree->GetMasteryStatModifiers(State->GetEquippedWeaponType()));
			Snapshot.Equipment.MoveSpeedMultiplierBonus += Tree->GetCommonSkillValue(ERogue10mCommonSkill::MoveSpeed)
				+ Tree->GetCommonSkillValue(ERogue10mCommonSkill::MoveSpeedUpgrade);
		}
		if (const URogue10mTownProgressionComponent* Town = State->FindComponentByClass<URogue10mTownProgressionComponent>())
		{
			Snapshot.Equipment.Accumulate(Town->GetStatModifiers());
			Snapshot.Equipment.DefenseBonus += Town->GetTraitBonus(ERogue10mTownTrait::Defense);
			Snapshot.Equipment.CriticalChanceBonus += Town->GetTraitBonus(ERogue10mTownTrait::CriticalChance);
			Snapshot.Equipment.CriticalDamageMultiplierBonus += Town->GetTraitBonus(ERogue10mTownTrait::CriticalDamage);
			Snapshot.Equipment.MaxStaminaBonus += Town->GetTraitBonus(ERogue10mTownTrait::MaxStamina);
			Snapshot.Equipment.MoveSpeedMultiplierBonus += Town->GetTraitBonus(ERogue10mTownTrait::MoveSpeed);
		}
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
 if (WeaponVisualComponent) { WeaponVisualComponent->RefreshWeaponVisuals(); }
	if (FirstPersonPresentationComponent) { FirstPersonPresentationComponent->RefreshPresentation(); }
	RefreshThirdPersonPresentation();
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
		const auto* State = GetPlayerState<ARogue10mPlayerState>();
		const auto* Tree = State ? State->GetSkillTreeProgressionComponent() : nullptr;
		const float MaxMana = Snapshot.Base.MaxMana + (Tree ? Tree->GetManaCapacityBonus() : 0.0f);
		const float PreviousHealth = Attributes->GetHealth();
		const float PreviousStamina = Attributes->GetStamina();
		const float PreviousMana = Attributes->GetMana();

		Attributes->SetMaxHealth(Snapshot.MaxHealth);
		Attributes->SetMaxStamina(Snapshot.MaxStamina);
		Attributes->SetMaxMana(MaxMana);
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
			// Reallocation, reinvestment and equipment changes never refill current resources.
			Attributes->SetHealth(FMath::Clamp(PreviousHealth, 0.0f, Snapshot.MaxHealth));
			Attributes->SetStamina(FMath::Clamp(PreviousStamina, 0.0f, Snapshot.MaxStamina));
			Attributes->SetMana(FMath::Clamp(PreviousMana, 0.0f, MaxMana));
		}
	}

	NormalWalkSpeed = Snapshot.MoveSpeed;
	SprintWalkSpeed = Snapshot.SprintSpeed;
	RefreshCommonMobility();
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
	if (!URogue10mCharacterAnimationComponent::IsHeadViewSafeAttackMontage(Montage))
	{
		UE_LOG(LogRogue10m, Warning, TEXT("%s 회전/구르기 모션 재생 차단: %s"), *GetNameSafe(this), *GetNameSafe(Montage));
		return false;
	}

	bool bPlayed = false;
	auto TryPlay = [this, &bPlayed, Montage, PlayRate](USkeletalMeshComponent* MeshComponent)
	{
		// This mesh is paused with ticking disabled in third-person gameplay.
		if (bUseThirdPersonGameplay && MeshComponent == FirstPersonMesh) { return; }
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
