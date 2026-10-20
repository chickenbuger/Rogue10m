#include "Rogue10mSkillBindingComponent.h"

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
	return Key.IsValid() && Key != EKeys::AnyKey && Key.IsBindableToActions() && Key.IsDigital()
		&& Key.GetMenuCategory() == EKeys::NAME_KeyboardCategory && !Key.IsModifierKey()
		&& !Key.IsGamepadKey() && !Key.IsMouseButton() && !Key.IsTouch() && !Key.IsVirtual();
}

bool URogue10mSkillBindingComponent::IsReservedKey(FKey Key) const
{
	// Reserve legacy controls even before possession has created the pawn input component.
	static const TSet<FKey> Legacy = {EKeys::W,EKeys::A,EKeys::S,EKeys::D,EKeys::SpaceBar,EKeys::E,
		EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::I,EKeys::P,EKeys::K,
		EKeys::O,EKeys::Escape,EKeys::F10,EKeys::L,EKeys::V,EKeys::Tilde};
	if (Legacy.Contains(Key)) { return true; }
	const auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	if (!PC) { return true; }
	const auto HasLegacy = [Key](const UInputComponent* Input)
	{
		if (Input) { for (const FInputKeyBinding& Binding : Input->KeyBindings) { if (Binding.Chord.Key == Key) { return true; } } }
		return false;
	};
	if (HasLegacy(PC->InputComponent) || HasLegacy(PC->GetPawn() ? PC->GetPawn()->InputComponent : nullptr)) { return true; }
	if (auto* Subsystem = BoundSubsystem.Get(); Subsystem && MappingContext && Subsystem->HasMappingContext(MappingContext))
	{
		TArray<FMappingQueryIssue> Issues;
		Subsystem->QueryMapKeyInActiveContextSet(MappingContext, GetDefault<UInputAction>(), Key,
			Issues, EMappingQueryIssue::NoIssue);
		for (const FMappingQueryIssue& Issue : Issues)
		{
			if (Issue.BlockingContext && Issue.BlockingContext != MappingContext) { return true; }
		}
	}
	if (const auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
	{
		for (const FEnhancedActionKeyMapping& Mapping : Input->GetEnhancedActionMappingsView())
		{
			// The engine rebuilds mappings at frame end. Old generations of our actions
			// can remain in this view after Actions.Reset during same-frame edits.
			if (Mapping.Key == Key && Mapping.Action && Mapping.Action->GetOuter() != this) { return true; }
		}
	}
	return false;
}

bool URogue10mSkillBindingComponent::IsAllowedSkill(const URogue10mAttackSkillData* Skill) const
{
	const auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	const auto* Combat = GetCombat();
	return PC && PC->IsLocalPlayerController() && Combat && Skill && Combat->IsSkillInActiveTree(Skill)
		&& Combat->IsAttackSkillUnlocked(Skill) && Skill->SkillTreeNodeType == ERogue10mSkillTreeNodeType::ActiveTechnique;
}

FKey URogue10mSkillBindingComponent::GetSkillKey(const URogue10mAttackSkillData* Skill) const
{
	const FKey* Key = Settings && Skill ? Settings->SkillKeys.Find(Skill->GetPathName()) : nullptr;
	return Key ? *Key : FKey();
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
	if (!IsAllowedSkill(Skill) || !Settings || !BoundInput.IsValid())
	{
		OutError = FText::FromString(TEXT("현재 트리에서 해금한 공격 기술만 지정할 수 있습니다.")); return false;
	}
	if (!IsUsableKey(Key))
	{
		OutError = FText::FromString(TEXT("보조키 조합 없이 키보드 키 하나만 지정하세요.")); return false;
	}
	if (IsReservedKey(Key))
	{
		OutError = FText::FromString(TEXT("이 키는 이동·메뉴 등 기존 조작에서 사용 중입니다.")); return false;
	}
	for (const auto* Other : GetCombat()->GetActiveSkillTreeSkills())
	{
		if (Other && Other != Skill && GetSkillKey(Other) == Key)
		{
			OutError = FText::FromString(TEXT("현재 트리의 다른 기술에 지정된 키입니다.")); return false;
		}
	}
	TMap<FString,FKey> Keys = Settings->SkillKeys;
	Keys.Add(Skill->GetPathName(), Key);
	return StoreKeys(Keys, OutError);
}

bool URogue10mSkillBindingComponent::ClearSkillKey(URogue10mAttackSkillData* Skill, FText& OutError)
{
	if (!Settings || !GetCombat() || !Skill || !GetCombat()->IsSkillInActiveTree(Skill))
	{
		OutError = FText::FromString(TEXT("현재 스킬트리의 기술을 선택하세요.")); return false;
	}
	TMap<FString,FKey> Keys = Settings->SkillKeys;
	Keys.Remove(Skill->GetPathName());
	return StoreKeys(Keys, OutError);
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
	TArray<URogue10mAttackSkillData*> DesiredSkills;
	FString Layout;
	if (Combat && Settings)
	{
		Layout = FString::FromInt(static_cast<int32>(Combat->GetActiveSkillTreeWeaponType()));
		const auto Skills = Combat->GetActiveSkillTreeSkills();
		TMap<FKey,int32> Counts;
		for (const auto* Skill : Skills) { const FKey Key = GetSkillKey(Skill); if (Key.IsValid()) { ++Counts.FindOrAdd(Key); } }
		for (auto* Skill : Skills)
		{
			const FKey Key = GetSkillKey(Skill);
			if (IsAllowedSkill(Skill) && IsUsableKey(Key) && !IsReservedKey(Key) && Counts.FindRef(Key) == 1)
			{
				DesiredSkills.Add(Skill);
				Layout += TEXT("|") + Skill->GetPathName() + TEXT("=") + Key.ToString();
			}
		}
	}
	// Progression broadcasts after every accepted attack; counters alone must not cancel a held key.
	if (Combat == BoundCombat.Get() && Layout == CurrentLayout) { return; }
	RemoveOwnBindings();
	if (BoundCombat.Get() != Combat)
	{
		if (auto* Previous = BoundCombat.Get()) { Previous->OnSkillTreeChanged.RemoveDynamic(this, &ThisClass::ScheduleRefresh); }
		BoundCombat = Combat;
		if (Combat) { Combat->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::ScheduleRefresh); }
	}
	auto* Input = BoundInput.Get();
	auto* Subsystem = BoundSubsystem.Get();
	if (Input && Subsystem && Combat && Settings)
	{
		if (!MappingContext) { MappingContext = NewObject<UInputMappingContext>(this); }
		for (auto* Skill : DesiredSkills)
		{
			const FKey Key = GetSkillKey(Skill);
			auto* Action = NewObject<UInputAction>(this);
			Action->ValueType = EInputActionValueType::Boolean;
			Action->bTriggerWhenPaused = false;
			Actions.Add(Action);
			MappingContext->MapKey(Action, Key);
			BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::SkillPressed, Skill).GetHandle());
			BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::SkillReleased, Skill).GetHandle());
			BindingHandles.Add(Input->BindAction(Action, ETriggerEvent::Canceled, this, &ThisClass::SkillCanceled, Skill).GetHandle());
		}
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = true;
		Subsystem->AddMappingContext(MappingContext, 1, Options);
		CurrentLayout = Layout;
	}
	OnSkillBindingsChanged.Broadcast();
}

void URogue10mSkillBindingComponent::SkillPressed(URogue10mAttackSkillData* Skill)
{
	auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	const FKey Key = GetSkillKey(Skill);
	if (!PC || !IsAllowedSkill(Skill) || IsReservedKey(Key)) { return; }
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
