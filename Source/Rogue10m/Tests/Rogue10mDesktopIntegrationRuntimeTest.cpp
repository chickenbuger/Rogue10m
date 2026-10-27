// Rendered, isolated-profile QA only. Does not change shipping gameplay or authored maps.
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mItemDataAsset.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSaveGame.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "AI/Rogue10mMonsterAIController.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace Rogue10mDesktopIntegration
{
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld* InWorld)
	{
		World = InWorld;
		FString Label, ProfileSlot, Prefix, KeySlot;
		const TCHAR* Args = FCommandLine::Get();
		FParse::Value(Args,TEXT("RogueDesktopQANativeHold="),NativeHoldSeconds);
		const bool bArgs = FParse::Value(Args,TEXT("RogueDesktopQALabel="),Label)
			&& FParse::Value(Args,TEXT("RogueProfileSaveSlot="),ProfileSlot)
			&& FParse::Value(Args,TEXT("RogueProgressionSavePrefix="),Prefix)
			&& FParse::Value(Args,TEXT("RogueSkillKeySaveSlot="),KeySlot);
		bool bLabel = !Label.IsEmpty() && Label.Len() <= 24;
		for (TCHAR Ch : Label) { bLabel &= FChar::IsAlnum(Ch) || Ch == TCHAR('_'); }
		if (!bArgs || !bLabel || NativeHoldSeconds<0 || NativeHoldSeconds>120 || ProfileSlot != TEXT("Rogue10m_Town_QA_Desktop_") + Label
			|| Prefix != TEXT("Rogue10m_Progression_QA_Desktop_") + Label
			|| KeySlot != TEXT("Rogue10m_Fighter_QA_Desktop_") + Label || FParse::Param(Args,TEXT("nullrhi")))
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=DESKTOP_INTEGRATION_FAILED reason=exact_isolated_slots_and_rendering_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(),TEXT("Feature/doc/evidence/desktop-render-20261027"),Label);
		Images = FPaths::Combine(FPaths::ProjectDir(),TEXT("DevLog"),TEXT("20261027_DesktopIntegration_") + Label);
		if (IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().DirectoryExists(*Images))
		{
			UE_LOG(LogRogue10m,Error,TEXT("RESULT=DESKTOP_INTEGRATION_FAILED reason=fresh_evidence_label_required"));
			FPlatformMisc::RequestExitWithStatus(false,1); return;
		}
		IFileManager::Get().MakeDirectory(*Output,true); IFileManager::Get().MakeDirectory(*Images,true);
		Deadline = FPlatformTime::Seconds() + 240.0;
		Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self=AsShared()](float)
		{
			if (Self->bFinished) { return false; }
			if (FPlatformTime::Seconds() > Self->Deadline) { Self->Check(false,TEXT("bounded_watchdog")); Self->Finish(); return false; }
			if (Self->bHolding && Self->Character.IsValid())
			{
				Self->NativeMove = FMath::Max(Self->NativeMove,FVector::Dist2D(Self->NativeOrigin,Self->Character->GetActorLocation()));
				if (FPlatformTime::Seconds() > Self->HoldUntil) { Self->Finish(); return false; }
			}
			if (Self->Next && FPlatformTime::Seconds() >= Self->NextAt) { auto Work = MoveTemp(Self->Next); Work(); }
			return !Self->bFinished;
		}),0.02f);
		Later(3.0,[this]{Prepare();});
	}
	bool Finished() const { return bFinished; }
private:
	void Check(bool bOK,const TCHAR* Why)
	{
		++Checks; Results += FString::Printf(TEXT("%s %s\n"),bOK?TEXT("PASS"):TEXT("FAIL"),Why);
		if (!bOK) { ++Failures; UE_LOG(LogRogue10m,Error,TEXT("DESKTOP_INTEGRATION FAIL %s"),Why); }
	}
	void Later(double Delay,TFunction<void()> Work) { NextAt = FPlatformTime::Seconds()+Delay; Next = MoveTemp(Work); }
	void Key(FKey Value,bool bDown) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Value,bDown?IE_Pressed:IE_Released,bDown?1.f:0.f)); }
	void Photo(const TCHAR* Name)
	{
		// Core-ticker commands can precede this frame's HUD refresh. Capture a
		// later real viewport frame, without modifying or compositing its pixels.
		const FString Path=FPaths::Combine(Images,Name);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Path](float)
		{ FScreenshotRequest::RequestScreenshot(Path,true,false);return false; }),0.2f);
	}
	int32 ContainerFor(FGuid Id) const
	{
		const auto& Containers = Inventory->GetInventoryContainers();
		for (int32 I=0;I<Containers.Num();++I) { for (const auto& E:Containers[I].Entries) { if (E.InstanceId==Id) { return I; } } }
		return INDEX_NONE;
	}
	bool Equip(FGuid Id)
	{
		const int32 I=ContainerFor(Id); const bool bOK=I!=INDEX_NONE && Inventory->TryEquipGridItem(I,Id);
		if (bOK) { Check(Character->GetEquippedWeaponType()==(Id==KnuckleId?ERogue10mWeaponType::Knuckle:ERogue10mWeaponType::GreatSword),TEXT("equipped_type_matches_owned_instance")); }
		return bOK;
	}
	FGuid Equipped() const
	{
		for (const auto& S:Inventory->GetLeftEquipmentSlots()) { if (S.SlotType==ERogue10mInventorySlotType::MainWeapon && S.bHasEquippedItem) { return S.EquippedItem.InstanceId; } }
		return {};
	}
	void Prepare()
	{
		PC = World.IsValid()?Cast<ARogue10mPlayerController>(World->GetFirstPlayerController()):nullptr;
		auto* Instance = World.IsValid()?World->GetGameInstance():nullptr;
		auto* Profiles = Instance?Instance->GetSubsystem<URogue10mCharacterProfileSubsystem>():nullptr;
		auto* Saves = Instance?Instance->GetSubsystem<URogue10mProgressionSubsystem>():nullptr;
		Check(PC.IsValid() && Profiles && Saves,TEXT("actual_game_controller_and_profile_subsystems"));
		if (Failures) { Finish(); return; }
		Check(Profiles->GetProfileCount()==0,TEXT("fresh_QA_profile_slot"));
		FRogue10mCharacterProfile Profile;
		Check(!Failures && Profiles->CreateProfile(TEXT("DesktopQa"),FRogue10mCharacterAppearance(),Profile)
			&& Profiles->SelectProfile(Profile.ProfileId),TEXT("new_QA_profile_selected"));
		if (Failures) { Finish(); return; }
		PC->EnterSelectedCharacter(); PC->CloseAllBlockingPanels();
		Character = Cast<ARogue10mCharacter>(PC->GetPawn());
		State = Character.IsValid()?Character->GetPlayerState<ARogue10mPlayerState>():nullptr;
		Check(Character.IsValid() && State.IsValid() && Saves->IsAttachedTo(*Character),TEXT("selected_character_attached"));
		if (Failures) { Finish(); return; }
		Inventory = Character->GetInventoryComponent(); Combat = Character->GetCombatComponent();
		InitialWeapon = UEnum::GetValueAsString(Character->GetEquippedWeaponType());
		Saves->DetachCharacter(*Character,false);
		Check(!Saves->IsAttachedTo(*Character),TEXT("QA_fixture_detached_from_autosave_before_transient_items"));
		PC->SetControlRotation(FRotator(0,Character->GetActorRotation().Yaw,0));
		GroundDeadline = FPlatformTime::Seconds()+15.0; Later(0.5,[this]{WaitGround();});
	}
	void WaitGround()
	{
		if (!Character->CanChangeWeapon() && FPlatformTime::Seconds()<GroundDeadline) { Later(0.2,[this]{WaitGround();}); return; }
		Check(Character->CanChangeWeapon(),TEXT("actual_grounded_idle_state"));
		if (Failures) { Finish(); return; }
		KnuckleData.Reset(NewObject<URogue10mItemDataAsset>(GetTransientPackage(),NAME_None,RF_Transient));
		SwordData.Reset(NewObject<URogue10mItemDataAsset>(GetTransientPackage(),NAME_None,RF_Transient));
		for (auto* D:{KnuckleData.Get(),SwordData.Get()}) { D->Category=ERogue10mItemCategory::Equipment; D->EquipSlotType=ERogue10mInventorySlotType::MainWeapon; }
		KnuckleData->WeaponType=ERogue10mWeaponType::Knuckle; KnuckleData->DisplayName=FText::FromString(TEXT("QA Knuckle"));
		SwordData->WeaponType=ERogue10mWeaponType::GreatSword; SwordData->DisplayName=FText::FromString(TEXT("QA GreatSword"));
		int32 Index=INDEX_NONE;
		Check(Inventory->TryAddGridItem(KnuckleData.Get(),1,Index,KnuckleId) && Inventory->TryAddGridItem(SwordData.Get(),1,Index,SwordId),TEXT("QA_owned_items_added_through_inventory"));
		Check(Equip(KnuckleId),TEXT("actual_inventory_knuckle_equip"));
		Check(Character->CanBeDamaged(),TEXT("equip_grants_no_invulnerability"));
		Photo(TEXT("01_knuckle_ready.png"));
		Later(0.7,[this]{StartMovement();});
	}
	void StartMovement()
	{
		MoveOrigin=Character->GetActorLocation(); Key(EKeys::W,true);
		Later(0.35,[this]
		{
			WalkDistance=FVector::Dist2D(MoveOrigin,Character->GetActorLocation());
			Check(WalkDistance>5.0,TEXT("W_InputKey_moves_real_character")); Key(EKeys::LeftShift,true);
			Later(0.3,[this]
			{
				Check(Character->IsSprinting() && Character->CanChangeWeapon(),TEXT("real_shift_sprint_allows_switch"));
				Check(Equip(SwordId),TEXT("inventory_switch_to_greatsword_while_running"));
				const float Remaining=Combat->GetAttackCooldownRemaining();
				Check(Remaining>=0.48f && Remaining<=0.52f,TEXT("fresh_swap_half_second_floor"));
				Photo(TEXT("02_running_sword_swap.png")); Key(EKeys::W,false); Key(EKeys::LeftShift,false);
				Later(0.7,[this]{PrepareAttack();});
			});
		});
	}
	void PrepareAttack()
	{
		Check(Equip(KnuckleId),TEXT("return_to_owned_knuckle"));
		AttackSkill=Combat->GetEquippedSkill(ERogue10mAttackInputSlot::Primary);
		Check(AttackSkill.IsValid(),TEXT("real_knuckle_primary_asset"));
		if (Failures) { Finish(); return; }
		OldCooldown=AttackSkill->AttackCooldown; AttackSkill->AttackCooldown=5.0f; // QA-only in-memory timing fixture, restored on exit.
		UClass* Class=LoadClass<ARogue10mBasicMonster>(nullptr,TEXT("/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_ThornWarden.BP_OW_ThornWarden_C"));
		FTransform Transform(Character->GetActorRotation(),Character->GetActorLocation()+Character->GetActorForwardVector()*135.0f);
		Target = Class?World->SpawnActorDeferred<ARogue10mBasicMonster>(Class,Transform,PC.Get(),Character.Get(),ESpawnActorCollisionHandlingMethod::AlwaysSpawn):nullptr;
		Check(Target.IsValid(),TEXT("authored_test_target_spawned"));
		if (Failures) { Finish(); return; }
		Target->MarkCombatTestInstance(); UGameplayStatics::FinishSpawningActor(Target.Get(),Transform);
		if (auto* AI=Cast<ARogue10mMonsterAIController>(Target->GetController())) { AI->StopMonsterAI(); }
		Target->GetCharacterMovement()->DisableMovement(); Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Target->GetRogueAttributeSet()->SetMaxHealth(10000.f); Target->GetRogueAttributeSet()->SetHealth(10000.f);
		if (auto* Regen=Target->FindComponentByClass<URogue10mVitalRegenerationComponent>()) { Regen->ConfigureRegeneration(0,0,0); }
		Later(0.7,[this]
		{
			UsesBefore=State->GetSkillUseCount(AttackSkill->GetFName()); TargetBefore=Target->GetRogueAttributeSet()->GetHealth();
			Key(EKeys::LeftMouseButton,true);
			Later(0.1,[this]
			{
				const auto Before=Inventory->CaptureInventory(); const FGuid BeforeId=Equipped();
				Check(!Equip(SwordId),TEXT("held_attack_blocks_inventory_switch"));
				const auto After=Inventory->CaptureInventory();
				Check(Equipped()==BeforeId && FRogue10mInventorySnapshot::StaticStruct()->CompareScriptStruct(&Before,&After,0),TEXT("held_rejection_preserves_inventory"));
				Key(EKeys::LeftMouseButton,false);
				Later(0.06,[this]{CheckAttacking();});
			});
		});
	}
	void CheckAttacking()
	{
		Check(State->GetSkillUseCount(AttackSkill->GetFName())==UsesBefore+1,TEXT("LMB_InputKey_accepts_exactly_one_actual_attack"));
		Check(Combat->IsWeaponChangeBlocked(),TEXT("real_attack_action_busy"));
		const FGuid Before=Equipped(); Check(!Equip(SwordId) && Equipped()==Before,TEXT("active_attack_rejects_weapon_change"));
		Photo(TEXT("03_attack_switch_rejected.png"));
		GroundDeadline=FPlatformTime::Seconds()+12.0; Later(0.1,[this]{WaitAttackEnd();});
	}
	void WaitAttackEnd()
	{
		if (!Character->CanChangeWeapon() && FPlatformTime::Seconds()<GroundDeadline) { Later(0.1,[this]{WaitAttackEnd();}); return; }
		Check(Character->CanChangeWeapon(),TEXT("attack_ends_before_long_reuse_cooldown"));
		Damage=TargetBefore-Target->GetRogueAttributeSet()->GetHealth(); Check(Damage>0,TEXT("actual_attack_hit_reduces_authored_target_health"));
		const double DeadlineBefore=World->GetTimeSeconds()+Combat->GetAttackCooldownRemaining();
		Check(Combat->GetAttackCooldownRemaining()>1.0f,TEXT("long_reuse_time_still_remaining"));
		Check(Equip(SwordId),TEXT("switch_allowed_after_action_during_reuse"));
		const double DeadlineAfter=World->GetTimeSeconds()+Combat->GetAttackCooldownRemaining();
		Check(FMath::Abs(DeadlineAfter-DeadlineBefore)<0.01,TEXT("switch_preserves_long_absolute_deadline"));
		Check(Character->CanBeDamaged(),TEXT("post_attack_switch_grants_no_invulnerability"));
		const FGuid Before=Equipped(); State->SetCharacterDead(true);
		Check(!Equip(KnuckleId) && Equipped()==Before,TEXT("death_flag_rejects_actual_inventory_change")); State->SetCharacterDead(false);
		Photo(TEXT("04_long_cooldown_preserved.png"));
		AttackSkill->AttackCooldown=OldCooldown;
		Later(Combat->GetAttackCooldownRemaining()+0.2,[this]{PrepareNativeInput();});
	}
	void PrepareNativeInput()
	{
		Check(Equip(KnuckleId),TEXT("native_input_stage_returns_to_knuckle"));
		Later(0.7,[this]
		{
			if (Failures) { Finish(); return; }
			NativeOrigin=Character->GetActorLocation(); NativeUses=State->GetSkillUseCount(AttackSkill->GetFName());
			bHolding=NativeHoldSeconds>0; HoldUntil=FPlatformTime::Seconds()+NativeHoldSeconds;
			PC->AddCombatLogMessage(TEXT("QA automatic checks passed. Controlled fixture; not a full run."),FLinearColor::Green,15.f);
			UE_LOG(LogRogue10m,Display,TEXT("DESKTOP_INTEGRATION AUTO_COMPLETE checks=%d failures=%d native_hold_seconds=%d"),Checks,Failures,NativeHoldSeconds);
			Photo(TEXT("05_knuckle_return.png"));
			if (!bHolding) { Later(0.6,[this]{Finish();}); }
		});
	}
	void Finish()
	{
		if (bFinished) { return; } bFinished=true;
		if (AttackSkill.IsValid()) { AttackSkill->AttackCooldown=OldCooldown; }
		const int32 NativeDelta=bHolding && State.IsValid() && AttackSkill.IsValid()?State->GetSkillUseCount(AttackSkill->GetFName())-NativeUses:0;
		Results+=FString::Printf(TEXT("checks=%d\nfailures=%d\nwalk_cm=%.3f\nactual_target_damage=%.3f\ninitial_weapon=%s\nnative_max_move_cm=%.3f\nnative_attack_uses=%d\nNOTE=QA-owned transient items, AI-disabled marked target, temporary five-second cooldown; not full-run or balance validation. Native stage has no simulated input.\n"),Checks,Failures,WalkDistance,Damage,*InitialWeapon,NativeMove,NativeDelta);
		FFileHelper::SaveStringToFile(Results,*FPaths::Combine(Output,TEXT("results.txt")));
		if (World.IsValid()) { Photo(TEXT("06_final_view.png")); }
		UE_LOG(LogRogue10m,Display,TEXT("RESULT=DESKTOP_INTEGRATION_%s checks=%d failures=%d native_move_cm=%.3f native_uses=%d"),Failures?TEXT("FAILED"):TEXT("PASSED"),Checks,Failures,NativeMove,NativeDelta);
		// Allow the last real viewport frame to be written before a clean exit.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Code=Failures?1:0](float){FPlatformMisc::RequestExitWithStatus(false,Code);return false;}),1.0f);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> PC;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mPlayerState> State;
	TWeakObjectPtr<URogue10mInventoryComponent> Inventory;
	TWeakObjectPtr<URogue10mCombatComponent> Combat;
	TWeakObjectPtr<ARogue10mBasicMonster> Target;
	TWeakObjectPtr<URogue10mAttackSkillData> AttackSkill;
	TStrongObjectPtr<URogue10mItemDataAsset> KnuckleData,SwordData;
	FTSTicker::FDelegateHandle Ticker;
	FGuid KnuckleId,SwordId;
	FString Output,Images,Results,InitialWeapon;
	TFunction<void()> Next;
	double NextAt=0,Deadline=0,GroundDeadline=0,HoldUntil=0;
	FVector MoveOrigin=FVector::ZeroVector,NativeOrigin=FVector::ZeroVector;
	float OldCooldown=0,TargetBefore=0,WalkDistance=0,Damage=0;
	double NativeMove=0;
	int32 Checks=0,Failures=0,UsesBefore=0,NativeUses=0,NativeHoldSeconds=90;
	bool bFinished=false,bHolding=false;
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestDesktopWeaponIntegration"),TEXT("Rendered isolated-profile desktop movement, actual attacks and inventory swap QA; requires exact Desktop QA slots."),FConsoleCommandDelegate::CreateLambda([]
{
	if (!GEngine || GIsEditor || (Active.IsValid() && !Active->Finished())) { return; }
	for (const auto& Context:GEngine->GetWorldContexts()) { if (auto* W=Context.World();W && W->WorldType==EWorldType::Game) { Active=MakeShared<FRun>();Active->Start(W);return; } }
}));
}
#endif
