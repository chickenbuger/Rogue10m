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
#include <limits>
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mSwordPalmGripTest, "Rogue10m.Weapons.SwordPalmGrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mSwordPalmGripTest::RunTest(const FString& Parameters)
{
 FTransform Grip;
 const FVector Right(12,7,30),Left(12,7,10);
 TestTrue(TEXT("Valid palm basis"),URogue10mWeaponVisualComponent::BuildSwordPalmGrip(Right,Left,FVector(2,0,1),Grip));
 TestTrue(TEXT("Origin is rendered right palm"),Grip.GetLocation().Equals(Right));
 TestTrue(TEXT("Blade Z points from left to right palm"),Grip.GetUnitAxis(EAxis::Z).Equals(FVector::UpVector));
 TestTrue(TEXT("Finger projection preserves authored roll Y"),Grip.GetUnitAxis(EAxis::Y).Equals(FVector::ForwardVector));
 TestTrue(TEXT("X is right-handed cross Y Z"),Grip.GetUnitAxis(EAxis::X).Equals(FVector(0,-1,0)));
 TestTrue(TEXT("Orthonormal right-handed basis"),FVector::CrossProduct(Grip.GetUnitAxis(EAxis::X),Grip.GetUnitAxis(EAxis::Y)).Equals(Grip.GetUnitAxis(EAxis::Z)));
 const FTransform Before=Grip;
 TestFalse(TEXT("Short baseline rejected"),URogue10mWeaponVisualComponent::BuildSwordPalmGrip(Right,Right+FVector(0,0,.5),FVector::ForwardVector,Grip));
 TestFalse(TEXT("Parallel finger roll axis rejected"),URogue10mWeaponVisualComponent::BuildSwordPalmGrip(Right,Left,FVector::UpVector,Grip));
 TestFalse(TEXT("NaN palm rejected"),URogue10mWeaponVisualComponent::BuildSwordPalmGrip(FVector(std::numeric_limits<double>::quiet_NaN(),0,0),Left,FVector::ForwardVector,Grip));
 TestFalse(TEXT("Infinite axis rejected"),URogue10mWeaponVisualComponent::BuildSwordPalmGrip(Right,Left,FVector(std::numeric_limits<double>::infinity(),0,0),Grip));
 TestTrue(TEXT("Failure preserves caller fallback"),Grip.Equals(Before));
 const FQuat Turn(FVector::UpVector,.7);
 FTransform Rotated;
 TestTrue(TEXT("Rotated basis valid"),URogue10mWeaponVisualComponent::BuildSwordPalmGrip(Turn.RotateVector(Right),Turn.RotateVector(Left),Turn.RotateVector(FVector(2,0,1)),Rotated));
 TestTrue(TEXT("Finger Y follows body rotation"),Rotated.GetUnitAxis(EAxis::Y).Equals(Turn.RotateVector(FVector::ForwardVector)));
 TestTrue(TEXT("Blade-face X follows body rotation"),Rotated.GetUnitAxis(EAxis::X).Equals(Turn.RotateVector(FVector(0,-1,0))));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mWeaponVisualTest, "Rogue10m.Weapons.VisualAttachment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mWeaponVisualTest::RunTest(const FString& Parameters)
{
 const auto Init = UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
 if (!TestNotNull(TEXT("World"), World)) { return false; }
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 UClass* CharacterClass = LoadClass<ARogue10mCharacter>(nullptr, TEXT("/Game/Character/Customization/Characters/BP_Rogue10m_HumanMaleCharacter.BP_Rogue10m_HumanMaleCharacter_C"));
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
  Character->GetMesh()->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
  Character->DispatchBeginPlay();
  auto* Visual = Character->FindComponentByClass<URogue10mWeaponVisualComponent>();
  if (TestNotNull(TEXT("Native component"), Visual))
  {
   TestFalse(TEXT("No perpetual weapon tick"), Visual->PrimaryComponentTick.bCanEverTick);
   Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
   auto* Right = Visual->GetRightWeaponMesh(); auto* Left = Visual->GetLeftWeaponMesh();
   if (TestNotNull(TEXT("Right knuckle"), Right) && TestNotNull(TEXT("Left knuckle"), Left))
   {
    TestTrue(TEXT("Both visible"), Right->IsVisible() && Left->IsVisible());
    TestTrue(TEXT("Rendered body attachment"), Right->GetAttachParent() == Character->GetMesh());
    TestEqual(TEXT("Right hand"), Right->GetAttachSocketName(), FName(TEXT("hand_r")));
    TestEqual(TEXT("Left hand"), Left->GetAttachSocketName(), FName(TEXT("hand_l")));
    TestEqual(TEXT("No visual collision"), Right->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    TestNotNull(TEXT("Knuckle asset"), Right->GetStaticMesh().Get());
    Character->RestoreEquippedWeaponType(ERogue10mWeaponType::GreatSword);
    TestTrue(TEXT("Sword visible"), Right->IsVisible());
    TestFalse(TEXT("Offhand knuckle hidden for sword"), Left->IsVisible());
    TestTrue(TEXT("Sword mesh selected"), Right->GetStaticMesh() == Visual->GreatSwordMesh.Get());
    Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Unarmed);
    TestFalse(TEXT("Unarmed hides both"), Right->IsVisible() || Left->IsVisible());
    Character->RestoreEquippedWeaponType(ERogue10mWeaponType::Knuckle);
    TestTrue(TEXT("Switch back uses existing components"), Right == Visual->GetRightWeaponMesh() && Left == Visual->GetLeftWeaponMesh());
    TestTrue(TEXT("Switch back shows both"), Right->IsVisible() && Left->IsVisible());
   }
  }
 }
 else { AddError(TEXT("Character or state spawn failed")); }
 World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
 return !HasAnyErrors();
}
#endif
