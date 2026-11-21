// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "Algo/AllOf.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mCombatComponent.h"
#include "Rogue10mInputKeys.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mProgressionSubsystem.h"
#include "Rogue10mSkillTreeCombatComponent.h"
#include "Rogue10mSkillTreeProgressionComponent.h"
#include "Rogue10mVitalRegenerationComponent.h"
#include "Rogue10mWeaponIdentityComponent.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/Rogue10mCombatHudAWidget.h"
#include "Widgets/Rogue10mMainHUDWidget.h"

namespace Rogue10mCombatHudAQA
{
const FIntPoint Resolutions[] = { {1672, 941}, {1280, 720} };
constexpr ERogue10mAttackInputSlot SkillSlots[] = {
	ERogue10mAttackInputSlot::QSkill,
	ERogue10mAttackInputSlot::ESkill,
	ERogue10mAttackInputSlot::RSkill,
	ERogue10mAttackInputSlot::Special,
	ERogue10mAttackInputSlot::MouseChord,
	ERogue10mAttackInputSlot::ChargedSpecial,
};

class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& GameWorld)
	{
		World = &GameWorld;
		FString Label, Profile, SkillKeys;
		FParse::Value(FCommandLine::Get(), TEXT("RogueCombatHudALabel="), Label);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), Profile);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), ProgressionPrefix);
		FParse::Value(FCommandLine::Get(), TEXT("RogueSkillKeySaveSlot="), SkillKeys);
		bool bSafeLabel = !Label.IsEmpty() && Label.Len() <= 40;
		for (const TCHAR Symbol : Label)
		{
			bSafeLabel &= (Symbol >= 'A' && Symbol <= 'Z') || (Symbol >= 'a' && Symbol <= 'z')
				|| (Symbol >= '0' && Symbol <= '9') || Symbol == '_';
		}
		if (!Check(bSafeLabel && Profile == TEXT("Rogue10m_Town_QA_HudA_") + Label
			&& SkillKeys == TEXT("Rogue10m_Fighter_QA_HudA_") + Label
			&& ProgressionPrefix == TEXT("Rogue10m_Progression_QA_HudA_") + Label,
			TEXT("isolated_HUD_A_QA_save_names_required")))
		{
			Finish();
			return;
		}
		Evidence = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("tmp/combat-hud-a-qa") / Label);
		if (!Check(IFileManager::Get().MakeDirectory(*Evidence, true) && GameWorld.GetMapName() == TEXT("StartHub"),
			TEXT("isolated_evidence_directory_and_real_StartHub")))
		{
			Finish();
			return;
		}
		Later(3.0f, [this]() { Prepare(); });
	}

private:
	void Later(float Delay, TFunction<void()> Callback)
	{
		FTimerHandle Timer;
		World->GetTimerManager().SetTimer(Timer,
			FTimerDelegate::CreateLambda([Self = AsShared(), Callback = MoveTemp(Callback)]()
			{
				if (!Self->bFinished && Self->World.IsValid()) { Callback(); }
			}), Delay, false);
	}

	bool Check(bool bPassed, const FString& Detail)
	{
		++Checks;
		Failures += bPassed ? 0 : 1;
		Report += FString::Printf(TEXT("%s case=%d %s\n"), bPassed ? TEXT("PASS") : TEXT("FAIL"), CaseIndex, *Detail);
		if (!bPassed)
		{
			UE_LOG(LogRogue10m, Error, TEXT("HUD_A QA case=%d %s"), CaseIndex, *Detail);
		}
		return bPassed;
	}

	void Prepare()
	{
		UE_LOG(LogRogue10m, Display, TEXT("HUD_A QA START real_StartHub"));
		ARogue10mPlayerController* Controller = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		UGameInstance* Game = World->GetGameInstance();
		auto* Profiles = Game ? Game->GetSubsystem<URogue10mCharacterProfileSubsystem>() : nullptr;
		auto* Progression = Game ? Game->GetSubsystem<URogue10mProgressionSubsystem>() : nullptr;
		FRogue10mCharacterProfile Profile;
		if (!Check(Controller && Profiles && Progression && Profiles->GetProfileCount() == 0
			&& Profiles->CreateProfile(TEXT("CombatHudAQa"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("disposable_profile_created_through_game_subsystem")))
		{
			Finish();
			return;
		}

		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
		State = Character.IsValid() ? Character->GetPlayerState<ARogue10mPlayerState>() : nullptr;
		if (!Check(Character.IsValid() && State.IsValid() && Character->GetCombatComponent() && Character->GetRogueAttributeSet()
			&& Character->IsThirdPersonGameplayActive() && Progression->IsAttachedTo(*Character)
			&& Progression->GetAttachedProfileId() == Profile.ProfileId
			&& Progression->GetSaveSlotForProfile(Profile.ProfileId).StartsWith(ProgressionPrefix + TEXT("_")),
			TEXT("real_third_person_pawn_and_production_save_attachment")))
		{
			Finish();
			return;
		}

		Progression->DetachCharacter(*Character, false);
		if (!Check(!Progression->IsAttachedTo(*Character) && !Progression->CanRetrySave(), TEXT("QA_changes_detached_from_progression_save")))
		{
			Finish();
			return;
		}
		Later(.75f, [this]() { CaptureUnarmed(); });
	}

	void CaptureUnarmed()
	{
		const URogue10mMainHUDWidget* Main = FindHud();
		const auto* View = Main ? Main->GetCombatHudA() : nullptr;
		const UGameViewportClient* ViewportClient = GEngine->GameViewport;
		const FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
		const USpringArmComponent* CameraBoom = Character->GetThirdPersonCameraBoom();
		if (!Check(View && View->IsVisible() && CameraBoom && Viewport && Viewport->GetSizeXY() == Resolutions[0],
			TEXT("default_camera_unarmed_capture_uses_real_HUD_at_reference_resolution")))
		{
			Finish();
			return;
		}
		const auto& Identity = View->GetDisplayedIdentity();
		Check(Identity.WeaponType == ERogue10mWeaponType::Unarmed && !Identity.bHasIdentityResource
			&& !View->ShowsIdentityPercent() && !View->ShowsIdentityKey(), TEXT("unarmed_hides_placeholder_percentage_and_Ctrl"));
		Report += FString::Printf(TEXT("unarmed_default_camera_arm=%.1f target=(%.1f,%.1f,%.1f) socket=(%.1f,%.1f,%.1f)\n"),
			CameraBoom->TargetArmLength, CameraBoom->TargetOffset.X, CameraBoom->TargetOffset.Y, CameraBoom->TargetOffset.Z,
			CameraBoom->SocketOffset.X, CameraBoom->SocketOffset.Y, CameraBoom->SocketOffset.Z);
		const FString Screenshot = Evidence / TEXT("CombatHudA_Unarmed_1672x941.png");
		FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
		Later(1.0f, [this, Screenshot]()
		{
			Check(IFileManager::Get().FileSize(*Screenshot) > 24, TEXT("unarmed_untouched_default_camera_capture_saved"));
			PrepareGreatsword();
		});
	}

	void PrepareGreatsword()
	{
		AController* Controller = Character->GetController();
		if (USpringArmComponent* CameraBoom = Character->GetThirdPersonCameraBoom())
		{
			// Frame the detached sample character for the two HUD comparison captures.
			CameraBoom->TargetArmLength = 370.f;
			CameraBoom->TargetOffset = FVector(0.0, 0.0, -42.0);
			CameraBoom->SocketOffset = FVector::ZeroVector;
			if (Controller)
			{
				FRotator Rotation = Controller->GetControlRotation();
				Rotation.Pitch = -20.0;
				Controller->SetControlRotation(Rotation);
			}
		}
		Character->SetCanBeDamaged(false);
		if (auto* Regeneration = Character->FindComponentByClass<URogue10mVitalRegenerationComponent>())
		{
			Regeneration->ConfigureRegeneration(0.f, 0.f, 0.f);
		}
		Character->SetEquippedWeaponType(ERogue10mWeaponType::GreatSword);
		State->SetIdentityType(ERogue10mIdentityType::Rage);
		State->SetManaEnabled(false);
		URogue10mAttributeSet* Attributes = Character->GetRogueAttributeSet();
		Attributes->SetPlayerLevel(8.f);
		Attributes->SetMaxHealth(135.f);
		Attributes->SetMaxStamina(100.f);
		PrepareGreatswordIdentity();
		EquipSampleSkills(*Character->GetCombatComponent());
		SetCase();
	}

	void PrepareGreatswordIdentity()
	{
		auto* Tree = State->GetSkillTreeProgressionComponent();
		auto* IdentityCombat = Character->GetSkillTreeCombatComponent();
		if (!Check(Tree && IdentityCombat && Tree->GetActiveTrack() == ERogue10mSkillTreeTrack::GreatSword,
			TEXT("greatsword_uses_real_skill_tree_identity_track")))
		{
			return;
		}
		FRogue10mSkillTreeSnapshot Snapshot = Tree->Capture();
		FRogue10mSkillTreeInvestment& Entry = Snapshot.Investments.AddDefaulted_GetRef();
		Entry.NodeId = FName(TEXT("I.FightingSpirit.Entry"));
		Entry.Route = ERogue10mSkillTreeUnlockRoute::Points;
		Entry.SpentPoints = 1;
		FRogue10mSkillTreeInvestment& Advanced = Snapshot.Investments.AddDefaulted_GetRef();
		Advanced.NodeId = FName(TEXT("I.FightingSpirit.Advanced"));
		Advanced.Route = ERogue10mSkillTreeUnlockRoute::Points;
		Advanced.SpentPoints = 2;
		Snapshot.ActiveIdentity = ERogue10mSkillTreeIdentity::FightingSpirit;
		if (!Check(Tree->Restore(Snapshot) && Tree->GetActiveIdentity() == Snapshot.ActiveIdentity,
			TEXT("disposable_greatsword_profile_restores_valid_FightingSpirit_investment")))
		{
			return;
		}
		for (int32 Hit = 0; Hit < 3; ++Hit)
		{
			IdentityCombat->NotifyConfirmedAttack(ERogue10mSkillTreeTrack::GreatSword);
		}
	}

	void EquipSampleSkills(URogue10mCombatComponent& Combat)
	{
		Combat.UnlockAllActiveTreeForTesting();
		for (const ERogue10mAttackInputSlot Slot : SkillSlots)
		{
			Combat.UnassignSkillFromInputSlot(Slot);
		}
		const TArray<URogue10mAttackSkillData*> Candidates = Combat.GetSelectableActiveTechniques();
		// Give the restricted inputs first choice of assets usable by several slots.
		TSet<URogue10mAttackSkillData*> Assigned;
		for (int32 Index = UE_ARRAY_COUNT(SkillSlots) - 1; Index >= 0; --Index)
		{
			const ERogue10mAttackInputSlot Slot = SkillSlots[Index];
			for (URogue10mAttackSkillData* Skill : Candidates)
			{
				if (Skill && !Assigned.Contains(Skill) && Combat.CanEquipSkillInSlot(Skill, Slot)
					&& Combat.AssignSkillToInputSlot(Skill, Slot))
				{
					Assigned.Add(Skill);
					break;
				}
			}
		}
		Check(!Assigned.IsEmpty(), TEXT("at_least_one_live_greatsword_skill_equipped_from_real_content"));
		Report += FString::Printf(TEXT("production_greatsword_skills_equipped=%d/%d\n"), Assigned.Num(), UE_ARRAY_COUNT(SkillSlots));
	}

	void SetCase()
	{
		if (CaseIndex == UE_ARRAY_COUNT(Resolutions))
		{
			ProbeDodgeAndEmptySkill();
			return;
		}
		const FIntPoint Size = Resolutions[CaseIndex];
		URogue10mAttributeSet* Attributes = Character->GetRogueAttributeSet();
		Attributes->SetHealth(CaseIndex == 0 ? 104.f : 72.f);
		Attributes->SetStamina(Attributes->GetMaxStamina() * (CaseIndex == 0 ? .64f : .83f));
		Attributes->SetExperience(Attributes->GetExperienceToNextLevel() * (CaseIndex == 0 ? .42f : .68f));
		GEngine->Exec(World.Get(), *FString::Printf(TEXT("r.SetRes %dx%dw"), Size.X, Size.Y));
		Later(2.0f, [this]() { Inspect(); });
	}

	URogue10mMainHUDWidget* FindHud() const
	{
		for (TObjectIterator<URogue10mMainHUDWidget> It; It; ++It)
		{
			if (It->GetWorld() == World.Get() && It->IsInViewport()) { return *It; }
		}
		return nullptr;
	}

	void Inspect()
	{
		URogue10mMainHUDWidget* Main = FindHud();
		const auto* View = Main ? Main->GetCombatHudA() : nullptr;
		const UGameViewportClient* ViewportClient = GEngine->GameViewport;
		FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
		if (!Check(Main && View && View->IsVisible() && View->HasAtlas() && Viewport,
			TEXT("production_MainHUD_has_visible_A_surface_and_atlas")))
		{
			Finish();
			return;
		}
		const UWidget* Legacy = Main->GetWidgetFromName(TEXT("BottomHUDWidget"));
		Check(!Legacy || !Legacy->IsVisible(), TEXT("only_one_combat_HUD_visible"));
		const FIntPoint Pixels = Viewport->GetSizeXY();
		Check(Pixels == Resolutions[CaseIndex], TEXT("standalone_viewport_matches_requested_capture_resolution"));
		const FVector2D Local = View->GetCachedGeometry().GetLocalSize();
		const FVector2D MainSize = Main->GetCachedGeometry().GetLocalSize();
		const FVector2D Origin = Main->GetCachedGeometry().AbsoluteToLocal(
			View->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector));
		if (!Check(Local.X > 0 && Local.Y > 0 && Local.Equals(MainSize, 1.0) && Origin.IsNearlyZero(1.0),
			TEXT("paint_surface_covers_MainHUD_viewport")))
		{
			Finish();
			return;
		}
		InspectGeometry(Local, Pixels);
		InspectData(*Main, *View);
		const FString Filename = FString::Printf(TEXT("CombatHudA_%dx%d.png"), Pixels.X, Pixels.Y);
		const FString Screenshot = Evidence / Filename;
		FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
		Later(1.0f, [this, Screenshot]()
		{
			Check(IFileManager::Get().FileSize(*Screenshot) > 24, TEXT("real_game_and_UI_capture_saved"));
			++CaseIndex;
			SetCase();
		});
	}

	void ProbeDodgeAndEmptySkill()
	{
		auto* Controller = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		if (!Check(Controller != nullptr, TEXT("live_controller_can_receive_simulated_Alt")))
		{
			Finish();
			return;
		}
		Character->GetCombatComponent()->UnassignSkillFromInputSlot(ERogue10mAttackInputSlot::QSkill);
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Rogue10mInput::DodgeKey(), IE_Pressed, 1.f));
		Later(.10f, [this, WeakController = TWeakObjectPtr<ARogue10mPlayerController>(Controller)]()
		{
			if (WeakController.IsValid())
			{
				WeakController->InputKey(FInputKeyEventArgs::CreateSimulated(Rogue10mInput::DodgeKey(), IE_Released, 0.f));
			}
			URogue10mMainHUDWidget* Main = FindHud();
			if (Main) { Main->RefreshBoundWidgetData(); }
			const auto* View = Main ? Main->GetCombatHudA() : nullptr;
			Check(View && Character->GetDodgeCooldownRemaining() > 0.f && View->GetDodgeCooldownRemaining() > 0.f
				&& FMath::IsNearlyEqual(View->GetDodgeCooldownRemaining(), Character->GetDodgeCooldownRemaining(), .15f),
				TEXT("simulated_Alt_reaches_real_HUD_positive_dodge_cooldown"));
			Check(View && !View->GetDisplayedSkills().IsEmpty() && !View->GetDisplayedSkills()[0].bUnlocked
				&& !View->GetDisplayedSkills()[0].SkillData, TEXT("cleared_Q_skill_render_state_is_locked"));
			Later(.50f, [this]() { ProbeMana(); });
		});
	}

	void ProbeMana()
	{
		Character->SetEquippedWeaponType(ERogue10mWeaponType::Staff);
		State->SetIdentityType(ERogue10mIdentityType::Mana);
		State->SetManaEnabled(true);
		URogue10mAttributeSet* Attributes = Character->GetRogueAttributeSet();
		Attributes->SetMaxMana(100.f);
		Attributes->SetMana(43.f);
		Later(.40f, [this]()
		{
			const URogue10mMainHUDWidget* Main = FindHud();
			const auto* View = Main ? Main->GetCombatHudA() : nullptr;
			Check(View && View->IsUsingMana() && View->GetDisplayedIdentity().WeaponType == ERogue10mWeaponType::Staff
				&& FMath::IsNearlyEqual(View->GetDisplayedResource().Current, Character->GetRogueAttributeSet()->GetMana(), .01f)
				&& FMath::IsNearlyEqual(View->GetDisplayedResource().Normalized, Main->GetManaView().Normalized, .001f),
				TEXT("equipped_staff_uses_live_mana_in_the_same_vital_row"));
			ProbeKnuckle();
		});
	}

	void ProbeKnuckle()
	{
		Character->SetEquippedWeaponType(ERogue10mWeaponType::Knuckle);
		State->SetManaEnabled(false);
		Later(.40f, [this]()
		{
			const URogue10mMainHUDWidget* Main = FindHud();
			const auto* View = Main ? Main->GetCombatHudA() : nullptr;
			auto* Controller = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
			if (!Check(View && Controller && View->GetDisplayedIdentity().WeaponType == ERogue10mWeaponType::Knuckle
				&& View->ShowsIdentityKey(), TEXT("equipped_knuckle_shows_supported_Ctrl_input")))
			{
				Finish();
				return;
			}
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftControl, IE_Pressed, 1.f));
			Later(.15f, [this, WeakController = TWeakObjectPtr<ARogue10mPlayerController>(Controller)]()
			{
				if (WeakController.IsValid())
				{
					WeakController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftControl, IE_Released, 0.f));
				}
				const auto* Burst = Character->FindComponentByClass<URogue10mWeaponIdentityComponent>();
				URogue10mMainHUDWidget* RefreshedMain = FindHud();
				if (RefreshedMain) { RefreshedMain->RefreshBoundWidgetData(); }
				const auto* RefreshedView = RefreshedMain ? RefreshedMain->GetCombatHudA() : nullptr;
				Check(Burst && Burst->IsIdentityActive() && RefreshedView && RefreshedView->ShowsIdentityKey()
					&& RefreshedView->ShowsIdentityPercent() && RefreshedView->GetDisplayedIdentity().Normalized > 0.0f,
					TEXT("simulated_Ctrl_activates_real_knuckle_identity_and_updates_HUD"));
				Finish();
			});
		});
	}

	void InspectGeometry(const FVector2D& Local, const FIntPoint& Pixels)
	{
		const FVector2D ToPixels(Pixels.X / Local.X, Pixels.Y / Local.Y);
		const FBox2D LocalVitals = URogue10mCombatHudAWidget::GetVitalsBounds(Local);
		const FBox2D LocalCombat = URogue10mCombatHudAWidget::GetCombatBounds(Local);
		const FBox2D Vitals(LocalVitals.Min * ToPixels, LocalVitals.Max * ToPixels);
		const FBox2D Battle(LocalCombat.Min * ToPixels, LocalCombat.Max * ToPixels);
		const double Scale = FMath::Min(Pixels.X / 1672.0, Pixels.Y / 941.0);
		const FVector2D GroupOffset((Pixels.X - 1672.0 * Scale) * .5, Pixels.Y - 941.0 * Scale);
		Check(Vitals.Min.Equals(FVector2D(26, 22) * Scale, 2.0) && Vitals.Max.Equals(FVector2D(317, 154) * Scale, 2.0),
			TEXT("vitals_reference_scale_and_top_left_anchor"));
		Check(Battle.Min.Equals(GroupOffset + FVector2D(413, 732) * Scale, 2.0)
			&& Battle.Max.Equals(GroupOffset + FVector2D(1329, 912) * Scale, 2.0),
			TEXT("combat_cluster_matches_approved_bottom_placement"));
		Check(Battle.Min.X >= 0.0 && Battle.Min.Y > Pixels.Y * .70 && Battle.Max.X <= Pixels.X && Battle.Max.Y <= Pixels.Y,
			TEXT("combat_reference_scale_fits_without_covering_main_playfield"));
		CheckFeetClearance(Pixels, Battle.Min.Y);
		const FString Geometry = FString::Printf(
			TEXT("case=%d viewport=%dx%d scale=%.4f vitals=(%.1f,%.1f)-(%.1f,%.1f) combat=(%.1f,%.1f)-(%.1f,%.1f)\n"),
			CaseIndex, Pixels.X, Pixels.Y, Scale, Vitals.Min.X, Vitals.Min.Y, Vitals.Max.X, Vitals.Max.Y,
			Battle.Min.X, Battle.Min.Y, Battle.Max.X, Battle.Max.Y);
		Report += Geometry;
		UE_LOG(LogRogue10m, Display, TEXT("HUD_A %s"), *Geometry.TrimEnd());
	}

	void CheckFeetClearance(const FIntPoint& Pixels, double TopOfCombat)
	{
		const APlayerController* Controller = World->GetFirstPlayerController();
		const USkeletalMeshComponent* Mesh = Character->GetMesh();
		bool bClear = Controller && Mesh;
		double BottomY = 0.0;
		if (bClear)
		{
			for (const FName Bone : { FName(TEXT("foot_l")), FName(TEXT("foot_r")), FName(TEXT("ball_l")), FName(TEXT("ball_r")) })
			{
				FVector2D Screen;
				if (Mesh->GetBoneIndex(Bone) == INDEX_NONE || !Controller->ProjectWorldLocationToScreen(Mesh->GetBoneLocation(Bone), Screen))
				{
					bClear = false;
					break;
				}
				bClear &= Screen.X >= 0.0 && Screen.X <= Pixels.X && Screen.Y >= 0.0;
				BottomY = FMath::Max(BottomY, Screen.Y);
			}
		}
		Check(bClear && TopOfCombat - BottomY >= 20.0,
			FString::Printf(TEXT("detached_capture_feet_clear_HUD gap_px=%.1f"), TopOfCombat - BottomY));
	}

	void InspectData(const URogue10mMainHUDWidget& Main, const URogue10mCombatHudAWidget& View)
	{
		const URogue10mAttributeSet* Attributes = Character->GetRogueAttributeSet();
		const FRogue10mHudVitalView& Health = View.GetDisplayedHealth();
		const FRogue10mHudVitalView& Resource = View.GetDisplayedResource();
		const FRogue10mHudProgressionView& Growth = View.GetDisplayedProgression();
		const FRogue10mHudIdentityView& Identity = View.GetDisplayedIdentity();
		const FRogue10mHudIdentityView ExpectedIdentity = Main.GetIdentityView();
		Check(Health.bVisible && Health.Current > 0.f && FMath::IsNearlyEqual(Health.Current, Attributes->GetHealth(), .01f)
			&& FMath::IsNearlyEqual(Health.Max, Attributes->GetMaxHealth(), .01f)
			&& FMath::IsNearlyEqual(Health.Normalized, Health.Current / Health.Max, .001f), TEXT("paint_health_from_live_GAS"));
		Check(Resource.bVisible && FMath::IsNearlyEqual(Resource.Current, Attributes->GetStamina(), .01f)
			&& FMath::IsNearlyEqual(Resource.Normalized, Main.GetStaminaView().Normalized, .001f), TEXT("paint_stamina_from_live_GAS"));
		Check(Growth.Level == State->GetPlayerLevel() && FMath::IsNearlyEqual(Growth.CurrentExperience, State->GetCurrentExperience(), .01f)
			&& FMath::IsNearlyEqual(Growth.ExperienceNormalized, Main.GetProgressionView().ExperienceNormalized, .001f),
			TEXT("paint_level_and_experience_from_live_progression"));
		Check(Identity.WeaponType == ERogue10mWeaponType::GreatSword && Identity.IdentityType == ERogue10mIdentityType::Rage
			&& Identity.IdentityType == ExpectedIdentity.IdentityType && Identity.bHasIdentityResource && Identity.Label.EqualTo(ExpectedIdentity.Label)
			&& Identity.Normalized > 0.f && Identity.Normalized < 1.f
			&& FMath::IsNearlyEqual(Identity.Normalized, ExpectedIdentity.Normalized, .001f), TEXT("paint_live_greatsword_identity"));
		Check(View.ShowsIdentityPercent() && !View.ShowsIdentityKey(), TEXT("greatsword_paints_gauge_without_unsupported_Ctrl"));
		Report += FString::Printf(TEXT("rendered_health=%.1f/%.1f stamina=%.1f/%.1f level=%d identity=%.3f\n"),
			Health.Current, Health.Max, Resource.Current, Resource.Max, Growth.Level, Identity.Normalized);
		Check(Rogue10mInput::DodgeKey() == EKeys::LeftAlt && Rogue10mInput::DodgeKeyLabel().ToString() == TEXT("Alt")
			&& View.IsDodging() == Character->IsDodging()
			&& FMath::IsNearlyEqual(View.GetDodgeCooldownRemaining(), Character->GetDodgeCooldownRemaining(), .15f),
			TEXT("separate_Alt_dodge_and_live_cooldown_state"));

		const auto& Skills = View.GetDisplayedSkills();
		const TArray<FRogue10mHudQuickSlotView> ExpectedSkills = Main.GetSkillQuickSlotViews();
		Check(Skills.Num() == UE_ARRAY_COUNT(SkillSlots) && ExpectedSkills.Num() == Skills.Num(), TEXT("six_live_combat_action_views"));
		for (int32 Index = 0; Index < FMath::Min3(Skills.Num(), ExpectedSkills.Num(), static_cast<int32>(UE_ARRAY_COUNT(SkillSlots))); ++Index)
		{
			const FRogue10mHudQuickSlotView& Actual = Skills[Index];
			const FRogue10mHudQuickSlotView& Expected = ExpectedSkills[Index];
			Check(Actual.InputSlot == SkillSlots[Index] && !Actual.bDodgeSlot && Actual.InputText.EqualTo(Expected.InputText)
				&& Actual.SkillData == Expected.SkillData && Actual.SkillIcon == Expected.SkillIcon && Actual.bUnlocked == Expected.bUnlocked,
				FString::Printf(TEXT("slot_%d_actual_input_loadout_icon_and_unlock_state"), Index));
		}
		const auto& Items = View.GetDisplayedItems();
		Check(Items.Num() == 4 && Algo::AllOf(Items, [](const FRogue10mHudQuickSlotView& Item) { return !Item.bUnlocked; }),
			TEXT("four_live_locked_item_views"));
	}

	void Finish()
	{
		bFinished = true;
		const bool bPassed = Failures == 0 && CaseIndex == UE_ARRAY_COUNT(Resolutions);
		const FString Result = FString::Printf(TEXT("RESULT=COMBAT_HUD_A_%s cases=%d checks=%d failures=%d"),
			bPassed ? TEXT("PASSED") : TEXT("FAILED"), CaseIndex, Checks, Failures);
		const bool bSaved = !Evidence.IsEmpty()
			&& FFileHelper::SaveStringToFile(Report + Result + TEXT("\n"), *(Evidence / TEXT("runtime-contract.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogRogue10m, Display, TEXT("%s report=%d"), *Result, bSaved);
		FPlatformMisc::RequestExitWithStatus(false, bPassed && bSaved ? 0 : 1);
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mCharacter> Character;
	TWeakObjectPtr<ARogue10mPlayerState> State;
	FString Evidence;
	FString ProgressionPrefix;
	FString Report;
	int32 CaseIndex = 0;
	int32 Checks = 0;
	int32 Failures = 0;
	bool bFinished = false;
};

static TSharedPtr<FRun> ActiveRun;

static void Run()
{
	if (GIsEditor || !GEngine || ActiveRun.IsValid())
	{
		UE_LOG(LogRogue10m, Error, TEXT("Combat HUD A verification requires a fresh standalone game process."));
		return;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && World->WorldType == EWorldType::Game && World->GetNetMode() == NM_Standalone)
		{
			ActiveRun = MakeShared<FRun>();
			ActiveRun->Start(*World);
			return;
		}
	}
	UE_LOG(LogRogue10m, Error, TEXT("Combat HUD A verification could not find the standalone game world."));
}

static FAutoConsoleCommand Command(TEXT("Rogue10m.TestCombatHudA"),
	TEXT("Checks live HUD A data and layout in two isolated StartHub viewports, captures PNGs, and exits."),
	FConsoleCommandDelegate::CreateStatic(&Run));
}

#endif
