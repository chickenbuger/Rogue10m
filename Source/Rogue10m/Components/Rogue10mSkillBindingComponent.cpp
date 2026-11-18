#include "Rogue10mSkillBindingComponent.h"
#include "Rogue10mInputKeys.h"

#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputMappingQuery.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerInput.h"
#include "KeyState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Rogue10mAttackSkillData.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mPlayerController.h"
#include "TimerManager.h"

URogue10mSkillBindingComponent::URogue10mSkillBindingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

URogue10mCombatComponent* URogue10mSkillBindingComponent::GetCombat() const
{
	const auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	const auto* Character = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
	return Character ? Character->GetCombatComponent() : nullptr;
}

FString URogue10mSkillBindingComponent::GetSaveSlot() const
{
#if !UE_BUILD_SHIPPING
	FString TestSlot;
	if (FParse::Value(FCommandLine::Get(), TEXT("RogueSkillKeySaveSlot="), TestSlot)
		&& TestSlot.StartsWith(TEXT("Rogue10m_Fighter_QA_")) && TestSlot.Len() <= 100)
	{
		bool bValid = true;
		for (TCHAR C : TestSlot) { bValid &= FChar::IsAlnum(C) || C == TCHAR('_'); }
		if (bValid) { return TestSlot; }
	}
#endif
	const auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	const ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
	return FString::Printf(TEXT("Rogue10mSkillKeys_%d"), Player ? Player->GetControllerId() : 0);
}

void URogue10mSkillBindingComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* PC = Cast<ARogue10mPlayerController>(GetOwner()))
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
		InitializeBindings(PC->InputComponent);
	}
}

void URogue10mSkillBindingComponent::InitializeBindings(UInputComponent* Input)
{
	auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	if (!PC || !PC->IsLocalPlayerController()) { return; }
	if (!bLoaded)
	{
		Settings = Cast<URogue10mSkillBindingsSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlot(), 0));
		if (!Settings || Settings->Version != 1 || Settings->SkillKeys.Num() > 512)
		{
			Settings = NewObject<URogue10mSkillBindingsSaveGame>(this);
		}
		// Never load arbitrary asset paths from saved data. Resolve keys only against the live tree.
		for (auto It = Settings->SkillKeys.CreateIterator(); It; ++It)
		{
			if (!It.Key().StartsWith(TEXT("/Game/")) || It.Key().Len() > 512 || !IsUsableKey(It.Value())) { It.RemoveCurrent(); }
		}
		bLoaded = true;
	}
	if (BoundInput.Get() != Input)
	{
		RemoveOwnBindings();
		BoundInput = Cast<UEnhancedInputComponent>(Input);
	}
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (BoundSubsystem.Get() != Subsystem)
	{
		if (auto* Old = BoundSubsystem.Get())
		{
			Old->OnMappingContextAdded.RemoveDynamic(this, &ThisClass::HandleContextChanged);
			Old->OnMappingContextRemoved.RemoveDynamic(this, &ThisClass::HandleContextChanged);
		}
		BoundSubsystem = Subsystem;
		if (Subsystem)
		{
			Subsystem->OnMappingContextAdded.AddUniqueDynamic(this, &ThisClass::HandleContextChanged);
			Subsystem->OnMappingContextRemoved.AddUniqueDynamic(this, &ThisClass::HandleContextChanged);
		}
	}
	RefreshBindings();
}

void URogue10mSkillBindingComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(RefreshTimer); }
	if (auto* PC = Cast<ARogue10mPlayerController>(GetOwner()))
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}
	if (auto* Combat = BoundCombat.Get()) { Combat->OnSkillTreeChanged.RemoveDynamic(this, &ThisClass::ScheduleRefresh); }
	if (auto* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnMappingContextAdded.RemoveDynamic(this, &ThisClass::HandleContextChanged);
		Subsystem->OnMappingContextRemoved.RemoveDynamic(this, &ThisClass::HandleContextChanged);
	}
	RemoveOwnBindings();
	Super::EndPlay(Reason);
}

bool URogue10mSkillBindingComponent::IsUsableKey(FKey Key) const
{
	return Key == EKeys::Q || Key == EKeys::E || Key == EKeys::R;
}

bool URogue10mSkillBindingComponent::IsReservedKey(FKey Key) const
{
	return !IsUsableKey(Key);
}

bool URogue10mSkillBindingComponent::IsAllowedSkill(const URogue10mAttackSkillData* Skill) const
{
	const auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	const auto* Combat = GetCombat();
	return PC && PC->IsLocalPlayerController() && Combat
		&& Combat->CanEquipSkillInSlot(Skill, ERogue10mAttackInputSlot::QSkill);
}

FKey URogue10mSkillBindingComponent::GetSkillKey(const URogue10mAttackSkillData* Skill) const
{
	const auto* Combat = GetCombat();
	if (!Combat || !Skill) { return FKey(); }
	if (Combat->GetEquippedSkill(ERogue10mAttackInputSlot::QSkill) == Skill) { return EKeys::Q; }
	if (Combat->GetEquippedSkill(ERogue10mAttackInputSlot::ESkill) == Skill) { return EKeys::E; }
	if (Combat->GetEquippedSkill(ERogue10mAttackInputSlot::RSkill) == Skill) { return EKeys::R; }
	return FKey();
}

bool URogue10mSkillBindingComponent::StoreKeys(const TMap<FString,FKey>& Keys, FText& OutError)
{
	auto* Saved = NewObject<URogue10mSkillBindingsSaveGame>(this);
	Saved->SkillKeys = Keys;
	if (!UGameplayStatics::SaveGameToSlot(Saved, GetSaveSlot(), 0))
	{
		OutError = FText::FromString(TEXT("키 설정을 저장하지 못했습니다. 기존 설정을 유지합니다."));
		return false;
	}
	Settings = Saved;
	RefreshBindings();
	OutError = FText::GetEmpty();
	return true;
}

bool URogue10mSkillBindingComponent::SetSkillKey(URogue10mAttackSkillData* Skill, FKey Key, FText& OutError)
{
	auto* Combat = GetCombat();
	const auto Slot = Key == EKeys::Q ? ERogue10mAttackInputSlot::QSkill
		: Key == EKeys::E ? ERogue10mAttackInputSlot::ESkill : ERogue10mAttackInputSlot::RSkill;
	if (!Combat || !IsUsableKey(Key) || !IsAllowedSkill(Skill) || !Combat->AssignSkillToInputSlot(Skill, Slot))
	{
		OutError = FText::FromString(TEXT("공용 기술은 Q/E/R 또는 우클릭 슬롯에만 장착합니다. 동시/차징 전용은 해당 HUD 슬롯으로 드래그하세요."));
		return false;
	}
	OnSkillBindingsChanged.Broadcast(); OutError = FText::GetEmpty(); return true;
}
bool URogue10mSkillBindingComponent::ClearSkillKey(URogue10mAttackSkillData* Skill, FText& OutError)
{
	auto* Combat = GetCombat();
	if (!Combat || !Skill) { OutError = FText::FromString(TEXT("현재 무기의 기술을 선택하세요.")); return false; }
	bool bChanged = false;
	for (auto Slot : {ERogue10mAttackInputSlot::QSkill, ERogue10mAttackInputSlot::ESkill, ERogue10mAttackInputSlot::RSkill})
		{ if (Combat->GetEquippedSkill(Slot) == Skill) { bChanged |= Combat->UnassignSkillFromInputSlot(Slot); } }
	OutError = FText::GetEmpty(); if (bChanged) { OnSkillBindingsChanged.Broadcast(); } return bChanged;
}

void URogue10mSkillBindingComponent::CancelHeldInput()
{
	if (auto* Combat = BoundCombat.Get()) { Combat->CancelBoundSkillInput(); }
}

void URogue10mSkillBindingComponent::RemoveOwnBindings()
{
	CancelHeldInput();
	if (auto* Input = BoundInput.Get()) { for (uint32 Handle : BindingHandles) { Input->RemoveBindingByHandle(Handle); } }
	BindingHandles.Reset();
	if (auto* Subsystem = BoundSubsystem.Get(); Subsystem && MappingContext) { Subsystem->RemoveMappingContext(MappingContext); }
	if (MappingContext) { MappingContext->UnmapAll(); }
	Actions.Reset();
	CurrentLayout.Reset();
}

void URogue10mSkillBindingComponent::RefreshBindings()
{
	if (bRefreshing) { return; }
	TGuardValue<bool> Guard(bRefreshing, true);
	auto* Combat = GetCombat();
	if (BoundCombat.Get() != Combat)
	{
		RemoveOwnBindings();
		if (auto* Previous = BoundCombat.Get()) { Previous->OnSkillTreeChanged.RemoveDynamic(this, &ThisClass::ScheduleRefresh); }
		BoundCombat = Combat;
		if (Combat) { Combat->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::ScheduleRefresh); }
	}
	// Physical fixed slots are owned by Character. Never rebuild arbitrary saved key mappings.
	OnSkillBindingsChanged.Broadcast();
}

void URogue10mSkillBindingComponent::SkillPressed(URogue10mAttackSkillData* Skill)
{
	auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	const FKey Key = GetSkillKey(Skill);
	if (!PC || !IsAllowedSkill(Skill) || IsReservedKey(Key)) { return; }
	// A higher-priority legacy interaction (for example NPC dialogue) may have
	// consumed this key before Enhanced Input dispatches the skill callback.
	// Keep release/cancel delivery intact so an already-held skill can clean up.
	const FKeyState* KeyState = PC->PlayerInput ? PC->PlayerInput->GetKeyState(Key) : nullptr;
	if (KeyState && KeyState->bConsumed) { return; }
	// Modifier chords are intentionally unsupported even if the bare key was saved.
	for (FKey Modifier : {EKeys::LeftShift,EKeys::RightShift,EKeys::LeftControl,EKeys::RightControl,EKeys::LeftAlt,EKeys::RightAlt,EKeys::LeftCommand,EKeys::RightCommand})
	{
		if (PC->IsInputKeyDown(Modifier)) { return; }
	}
	GetCombat()->HandleBoundSkillPressed(Skill);
}

void URogue10mSkillBindingComponent::SkillReleased(URogue10mAttackSkillData* Skill)
{
	const FKey Key = GetSkillKey(Skill);
	if (IsAllowedSkill(Skill) && !IsReservedKey(Key)) { GetCombat()->HandleBoundSkillReleased(Skill); }
	else if (auto* Combat = GetCombat()) { Combat->CancelBoundSkillInput(); }
}

void URogue10mSkillBindingComponent::SkillCanceled(URogue10mAttackSkillData* Skill)
{
	if (auto* Combat = GetCombat()) { Combat->CancelBoundSkillInput(); }
}

void URogue10mSkillBindingComponent::HandlePawnChanged(APawn* Previous, APawn* Current)
{
	CancelHeldInput();
	ScheduleRefresh();
}

void URogue10mSkillBindingComponent::ScheduleRefresh()
{
	if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(RefreshTimer))
	{
		RefreshTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshBindings);
	}
}

void URogue10mSkillBindingComponent::HandleContextChanged(const UInputMappingContext* Context)
{
	if (Context != MappingContext && !bRefreshing) { CancelHeldInput(); ScheduleRefresh(); }
}
