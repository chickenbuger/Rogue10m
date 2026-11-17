#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Rogue10mAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mPlayerState.h"
#include "Rogue10mWeaponVisualComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mStaffVisualTest, "Rogue10m.Weapons.StaffAttachment",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mStaffVisualTest::RunTest(const FString& Parameters)
{
 const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false)
  .CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
 if (!TestNotNull(TEXT("World"), World)) { return false; }
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 UClass* CharacterClass = LoadClass<ARogue10mCharacter>(nullptr,
  TEXT("/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C"));
 ARogue10mCharacter* Character = CharacterClass ? World->SpawnActor<ARogue10mCharacter>(CharacterClass) : nullptr;
 ARogue10mPlayerState* State = World->SpawnActor<ARogue10mPlayerState>();
 if (Character && State)
 {
  Character->SetPlayerState(State);
  // This isolated world never possesses the pawn or begins the PlayerState normally.
  // Register its AttributeSet and owner/avatar before Inventory::BeginPlay equips the starter item.
  auto* ASC = State->GetAbilitySystemComponent();
  auto* Attributes = State->GetRogueAttributeSet();
  if (!TestNotNull(TEXT("Fixture ASC"), ASC) || !TestNotNull(TEXT("Fixture AttributeSet"), Attributes))
  {
   World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
  }
  ASC->AddAttributeSetSubobject(Attributes);
  ASC->InitAbilityActorInfo(State, Character);
  if (!TestTrue(TEXT("Fixture AttributeSet registered before character BeginPlay"), ASC->GetSet<URogue10mAttributeSet>() == Attributes)
   || !TestTrue(TEXT("Fixture ASC owner/avatar initialized"), ASC->GetOwnerActor() == State && ASC->GetAvatarActor() == Character))
  {
   World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
  }
  Character->DispatchBeginPlay();
  auto* Visual = Character->FindComponentByClass<URogue10mWeaponVisualComponent>();
  if (TestNotNull(TEXT("Native weapon visual component"), Visual))
  {
   TestFalse(TEXT("No perpetual weapon tick"), Visual->PrimaryComponentTick.bCanEverTick);
   const auto* Defaults=GetDefault<URogue10mWeaponVisualComponent>();
   TestEqual(TEXT("Native staff uses selected local derivative"),Defaults->StaffMesh.ToSoftObjectPath().ToString(),FString(TEXT("/Game/Weapons/Staff20261110/SM_BattleWizardStaff01.SM_BattleWizardStaff01")));
   TestTrue(TEXT("Native staff grip and policy configured"),Defaults->StaffGrip.IsValid()&&Defaults->StaffGrip.GetScale3D().GetMin()>0.&&Defaults->bStaffUprightIdle);
   // The generic synthetic attachment contract remains static, separate from upright native review.
   Visual->bStaffUprightIdle=false;
   Visual->StaffMesh.Reset();
   AddExpectedMessagePlain(TEXT("Weapon presentation mesh is missing"), ELogVerbosity::Warning,
    EAutomationExpectedMessageFlags::Contains, 2);
   Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Staff);
   auto Visible = [](const UStaticMeshComponent* Part) { return Part && Part->IsVisible(); };
   TestFalse(TEXT("Unassigned staff does not display an unrelated mesh"), Visible(Visual->GetRightWeaponMesh()));
   // Reuse a known mesh only as a synthetic attachment fixture; this is not staff art validation.
   Visual->StaffMesh = Visual->GreatSwordMesh;
   Visual->StaffGrip = FTransform(FRotator(7, 13, 19), FVector(3, 4, 5), FVector(.75));
   Visual->RefreshWeaponVisuals();
   auto* Right = Visual->GetRightWeaponMesh();
   if (TestNotNull(TEXT("Configured staff component"), Right))
   {
    TestTrue(TEXT("Configured staff is visible"), Right->IsVisible());
    TestTrue(TEXT("Rendered body parent"), Right->GetAttachParent() == Character->GetMesh());
    TestEqual(TEXT("Default right-hand staff socket"), Right->GetAttachSocketName(), FName(TEXT("hand_r")));
    TestTrue(TEXT("Configured asset selected"), Right->GetStaticMesh() == Visual->StaffMesh.Get());
    TestTrue(TEXT("Configured grip applied"), Right->GetRelativeTransform().Equals(Visual->StaffGrip));
    TestFalse(TEXT("No offhand staff or residual knuckle"), Visible(Visual->GetLeftWeaponMesh()));
    TestEqual(TEXT("Visual never owns collision"), Right->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    AddExpectedMessagePlain(TEXT("Staff presentation socket is missing"), ELogVerbosity::Warning,
     EAutomationExpectedMessageFlags::Contains, 2);
    Visual->StaffAttachSocket = TEXT("MissingStaffSocketForTest");
    Visual->RefreshWeaponVisuals();
    TestFalse(TEXT("Missing socket hides staff instead of root fallback"), Right->IsVisible());
    Visual->StaffAttachSocket = NAME_None;
    Visual->RefreshWeaponVisuals();
    TestFalse(TEXT("Empty socket hides staff"), Right->IsVisible());
    Visual->StaffAttachSocket = TEXT("hand_r");
    Visual->RefreshWeaponVisuals();
    TestTrue(TEXT("Valid socket recovers same component"), Right->IsVisible() && Right == Visual->GetRightWeaponMesh());
    const FTransform ValidGrip = Visual->StaffGrip;
    AddExpectedMessagePlain(TEXT("Staff presentation grip is invalid"), ELogVerbosity::Warning,
     EAutomationExpectedMessageFlags::Contains, 2);
    Visual->StaffGrip.SetScale3D(FVector::ZeroVector);
    Visual->RefreshWeaponVisuals();
    TestFalse(TEXT("Zero scale is rejected"), Right->IsVisible());
    Visual->StaffGrip.SetScale3D(FVector(-1, 1, 1));
    Visual->RefreshWeaponVisuals();
    TestFalse(TEXT("Negative scale is rejected"), Right->IsVisible());
    Visual->StaffGrip = ValidGrip;
    Visual->RefreshWeaponVisuals();
    TestTrue(TEXT("Valid grip recovers visibility"), Right->IsVisible());
    Visual->StaffMesh.Reset();
    Visual->RefreshWeaponVisuals();
    TestFalse(TEXT("Missing asset hides old visual"), Right->IsVisible());
    Visual->StaffMesh = Visual->GreatSwordMesh;
    Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
    TestTrue(TEXT("Knuckle still shows both hands"), Visible(Right) && Visible(Visual->GetLeftWeaponMesh()));
    UClass* PreviousBodyAnimClass = Character->GetMesh()->GetAnimClass();
    Character->RestoreEquippedWeaponType(ERogue10mWeaponType::GreatSword);
    TestTrue(TEXT("Sword selection preserved"), Right->GetStaticMesh() == Visual->GreatSwordMesh.Get());
    Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Staff);
    TestTrue(TEXT("Staff reuses right visual component"), Right == Visual->GetRightWeaponMesh() && Right->IsVisible());
    TestTrue(TEXT("Staff restores configured static grip after sword"), Right->GetRelativeTransform().Equals(Visual->StaffGrip));
    TestTrue(TEXT("Sword-only body animation override restores on staff"), Character->GetMesh()->GetAnimClass() == PreviousBodyAnimClass);
    TestFalse(TEXT("Staff hides residual offhand knuckle"), Visible(Visual->GetLeftWeaponMesh()));
    Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Unarmed);
    TestFalse(TEXT("Unarmed hides both hands"), Visible(Right) || Visible(Visual->GetLeftWeaponMesh()));
   }
  }
 }
 else { AddError(TEXT("Character or PlayerState spawn failed")); }
 World->DestroyWorld(false);
 GEngine->DestroyWorldContext(World);
 return !HasAnyErrors();
}
#endif
