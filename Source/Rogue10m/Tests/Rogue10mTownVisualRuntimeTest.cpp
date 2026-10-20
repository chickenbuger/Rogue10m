// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rogue10m.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCharacterProfileSubsystem.h"
#include "Rogue10mInventoryComponent.h"
#include "Rogue10mPlayerController.h"
#include "Rogue10mTutorialNPC.h"
#include "Rogue10mTutorialWidget.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Rogue10mTownVisualTest
{
class FRun : public TSharedFromThis<FRun>
{
public:
	void Start(UWorld& InWorld)
	{
		World = &InWorld;
		FString ProfileSlot, SavePrefix;
		FParse::Value(FCommandLine::Get(), TEXT("RogueProfileSaveSlot="), ProfileSlot);
		FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSavePrefix="), SavePrefix);
		auto Safe = [](const FString& Value, const TCHAR* Prefix)
		{
			if (!Value.StartsWith(Prefix) || Value.Len() > 80) { return false; }
			for (TCHAR C : Value) { if (!FChar::IsAlnum(C) && C != TEXT('_')) { return false; } }
			return true;
		};
		if (!Safe(ProfileSlot, TEXT("Rogue10m_Town_QA_")) || !Safe(SavePrefix, TEXT("Rogue10m_Progression_QA_")))
		{
			Check(false, TEXT("isolated_save_flags_required")); Finish(); return;
		}
		Output = FPaths::Combine(FPaths::ProjectDir(), TEXT("Feature/doc/evidence/town-services-persistence-20261011"), ProfileSlot);
		IFileManager::Get().MakeDirectory(*Output, true);
		Later(4.0f, [this] { Prepare(); });
	}
private:
	void Check(bool bOK, const TCHAR* Reason)
	{
		Results += FString::Printf(TEXT("%s %s\n"), bOK ? TEXT("PASS") : TEXT("FAIL"), Reason);
		if (!bOK) { ++Failures; }
	}
	void Later(float Delay, TFunction<void()> Callback)
	{
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda(
			[Self = AsShared(), Callback = MoveTemp(Callback)] { Callback(); }), Delay, false);
	}
	void Key(FKey InKey)
	{
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, IE_Pressed, 1.0f));
		Later(0.15f, [this, InKey] { Controller->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, IE_Released, 0.0f)); });
	}
	void Prepare()
	{
		auto* Profiles = World->GetGameInstance()->GetSubsystem<URogue10mCharacterProfileSubsystem>();
		Controller = Cast<ARogue10mPlayerController>(World->GetFirstPlayerController());
		Check(Profiles && Controller.IsValid() && Profiles->GetProfileCount() == 0, TEXT("fresh_isolated_visual_profile"));
		if (Failures) { Finish(); return; }
		FRogue10mCharacterProfile Profile;
		Check(Profiles->CreateProfile(TEXT("TownVisual"), FRogue10mCharacterAppearance(), Profile)
			&& Profiles->SelectProfile(Profile.ProfileId), TEXT("visual_profile_created"));
		Controller->EnterSelectedCharacter();
		Controller->CloseAllBlockingPanels();
		auto* Character = Cast<ARogue10mCharacter>(Controller->GetPawn());
		for (TActorIterator<ARogue10mTutorialNPC> It(World.Get()); It; ++It) { NPC = *It; break; }
		Check(Character && NPC.IsValid(), TEXT("actual_NPC_and_selected_pawn_exist"));
		if (Failures) { Finish(); return; }
		Character->GetInventoryComponent()->AddGold(500);
		Character->SetActorLocation(NPC->GetActorLocation() + FVector(100, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
		Check(NPC->BeginDialogue(Character), TEXT("dialogue_open"));
		Later(1.0f, [this] { PurchaseViaKey(); });
	}
	void PurchaseViaKey()
	{
		Key(EKeys::Enter);
		// Separate timer from key release, so both events reach the controller input stack.
		FTimerHandle Next;
		World->GetTimerManager().SetTimer(Next, FTimerDelegate::CreateLambda([Self = AsShared()]
		{
			auto* Character = Cast<ARogue10mCharacter>(Self->Controller->GetPawn());
			Self->Check(Character && Character->GetInventoryComponent()->GetGold() < 500, TEXT("Enter_executes_selected_service_through_input_stack"));
			Self->Key(EKeys::Down);
			FTimerHandle Capture;
			Self->World->GetTimerManager().SetTimer(Capture, FTimerDelegate::CreateLambda([Self] { Self->Capture(); }), 1.0f, false);
		}), 1.0f, false);
	}
	void Capture()
	{
		auto* Widget = NPC->GetTutorialWidget();
		Check(Widget && Widget->GetDisplayedServices().ToString().Contains(TEXT("▶ 장착 무기")), TEXT("Down_changes_visible_selected_service"));
		if (Widget)
		{
			const auto* Action = Widget->GetWidgetFromName(TEXT("TutorialAction"));
			const auto* Services = Widget->GetWidgetFromName(TEXT("TownServices"));
			const FGeometry& Root = Widget->GetCachedGeometry();
			auto Inside = [&Root](const UWidget* Child)
			{
				if (!Child) { return false; }
				const FGeometry& G = Child->GetCachedGeometry();
				const FVector2D A = Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
				const FVector2D B = Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));
				return G.GetLocalSize().X > 0 && G.GetLocalSize().Y > 0 && A.X >= 0 && A.Y >= 0
					&& B.X <= Root.GetLocalSize().X + 1 && B.Y <= Root.GetLocalSize().Y + 1;
			};
			Check(Inside(Action) && Inside(Services), TEXT("services_and_final_action_fully_inside_viewport"));
		}
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Output, TEXT("town-services.png")), true, false);
		Later(2.0f, [this]
		{
			Check(IFileManager::Get().FileSize(*FPaths::Combine(Output, TEXT("town-services.png"))) > 24, TEXT("rendered_screenshot_written"));
			Key(EKeys::Escape);
			FTimerHandle Next;
			World->GetTimerManager().SetTimer(Next, FTimerDelegate::CreateLambda([Self = AsShared()]
			{
				Self->Check(!Self->NPC->IsDialogueOpen(), TEXT("Esc_closes_dialogue_through_input_stack"));
				Self->Finish();
			}), 0.6f, false);
		});
	}
	void Finish()
	{
		if (!Output.IsEmpty()) { FFileHelper::SaveStringToFile(Results, *FPaths::Combine(Output, TEXT("visual-results.txt"))); }
		UE_LOG(LogRogue10m, Display, TEXT("RESULT=TOWN_VISUAL_%s failures=%d"), Failures ? TEXT("FAILED") : TEXT("PASSED"), Failures);
		FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
	}
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARogue10mPlayerController> Controller;
	TWeakObjectPtr<ARogue10mTutorialNPC> NPC;
	FTimerHandle Timer;
	FString Output, Results;
	int32 Failures = 0;
};
static TSharedPtr<FRun> Active;
static FAutoConsoleCommand Command(TEXT("Rogue10m.TestTownVisual"), TEXT("Captures actual town NPC services and verifies keyboard input in an isolated QA profile."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (!GEngine || GIsEditor) { return; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* W = Context.World(); W && W->WorldType == EWorldType::Game)
			{ Active = MakeShared<FRun>(); Active->Start(*W); return; }
		}
	}));
}
#endif
