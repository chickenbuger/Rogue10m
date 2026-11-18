// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Enemy/Rogue10mHumanoidAnimInstance.h"
#include "Enemy/Rogue10mMonsterAttackPatternProfile.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Enemy/Rogue10mBasicMonster.h"
#include "Enemy/Rogue10mMonsterAttackPatternComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mHumanoidAnimationTimingTest,
 "Rogue10m.Monster.HumanoidAnimation.ContactTiming",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mHumanoidAnimationTimingTest::RunTest(const FString& Parameters)
{
 const float Contact = URogue10mHumanoidAnimInstance::ResolveContactTime(0.6f, 1.5f);
 const float Rate = URogue10mHumanoidAnimInstance::CalculatePhaseRate(Contact, 1.1f);
 TestTrue(TEXT("Authored contact reaches the unchanged locked windup"), FMath::IsNearlyEqual(Contact / Rate, 1.1f));
 const float EnrageRate = URogue10mHumanoidAnimInstance::CalculatePhaseRate(Contact, 1.1f * 0.75f);
 const float ExpectedEnrage = 1.1f * 0.75f;
 const float ObservedEnrage = Contact / EnrageRate;
 AddInfo(FString::Printf(TEXT("Enrage float roundtrip expected=%.9f observed=%.9f absolute_error=%.9g tolerance=0.000001"), ExpectedEnrage, ObservedEnrage, FMath::Abs(ExpectedEnrage-ObservedEnrage)));
 TestTrue(TEXT("Enrage timing maps the same authored contact"), FMath::IsNearlyEqual(ObservedEnrage, ExpectedEnrage, 1.e-6f));
 const float RecoveryRate = URogue10mHumanoidAnimInstance::CalculatePhaseRate(1.5f - Contact, 0.65f);
 TestTrue(TEXT("Remainder fits authoritative recovery"), FMath::IsNearlyEqual((1.5f - Contact) / RecoveryRate, 0.65f));
 TestTrue(TEXT("Contact retains nonzero recovery tail"), URogue10mHumanoidAnimInstance::ResolveContactTime(10.f, 1.5f) < 1.5f);
 TestTrue(TEXT("Contact retains positive windup"), URogue10mHumanoidAnimInstance::ResolveContactTime(-1.f, 1.5f) > 0.f);
 TestEqual(TEXT("Invalid duration is finite"), URogue10mHumanoidAnimInstance::CalculatePhaseRate(1.f, 0.f), 1.f);
 const auto* Defaults = GetDefault<URogue10mMonsterAttackPatternProfile>();
 TestFalse(TEXT("Legacy profiles do not opt in"), Defaults->bUseHumanoidAnimInstance);
 TestTrue(TEXT("Legacy profiles need no montage asset"), Defaults->AttackMontage.IsNull());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogue10mHumanoidAnimationOptOutTest,
 "Rogue10m.Monster.HumanoidAnimation.ProfileTransitions",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRogue10mHumanoidAnimationOptOutTest::RunTest(const FString& Parameters)
{
 const auto Init=UWorld::InitializationValues().CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 if (!TestNotNull(TEXT("Synthetic world"),World)) return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 auto* Monster=World->SpawnActorDeferred<ARogue10mBasicMonster>(ARogue10mBasicMonster::StaticClass(),FTransform::Identity,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if (Monster) { Monster->AutoPossessAI=EAutoPossessAI::Disabled; Monster->FinishSpawning(FTransform::Identity); }
 auto* Mesh=Monster?Monster->GetMesh():nullptr;
 auto* Asset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Polyart/ModularStylizedChars/Meshes/SkeletalMeshes/Characters/SK_Garret_FullBody.SK_Garret_FullBody"));
 auto* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Rogue10m/Characters/Boss/AzathorHuman20261110/A_HumanBoss_Idle.A_HumanBoss_Idle"));
 if (TestNotNull(TEXT("Monster mesh"),Mesh) && TestNotNull(TEXT("Owned target mesh"),Asset) && TestNotNull(TEXT("Project idle"),Idle))
 {
  Mesh->SetSkeletalMeshAsset(Asset); Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass()); Mesh->SetReceivesDecals(true);
  const auto BaselineMode=Mesh->GetAnimationMode(); const auto BaselineClass=Mesh->GetAnimClass();
  auto* Human=NewObject<URogue10mMonsterAttackPatternProfile>(); Human->bUseHumanoidAnimInstance=true; Human->IdleAnimation=Idle; Human->MovementAnimation=Idle;
  auto* Legacy=NewObject<URogue10mMonsterAttackPatternProfile>();
  auto* Pattern=Monster->GetAttackPatternComponent(); Pattern->ConfigureProfile(Human);
  auto* Anim=Cast<URogue10mHumanoidAnimInstance>(Mesh->GetAnimInstance());
  if (TestNotNull(TEXT("Native opt-in instance"),Anim))
  {
   // This API accepts authored montages. Use the saved model rather than an unpopulated dynamic editor model.
   Anim->InitializeAnimation();
   const auto* Authored=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Rogue10m/Characters/Boss/AzathorHuman20261110/AM_HumanBoss_Attack.AM_HumanBoss_Attack"));
   UAnimMontage* Source=Authored?DuplicateObject<UAnimMontage>(Authored,GetTransientPackage()):nullptr;
   AddInfo(FString::Printf(TEXT("Authored montage length=%.6f transient source length=%.6f"),Authored?Authored->GetPlayLength():-1.f,Source?Source->GetPlayLength():-1.f));
   if (TestNotNull(TEXT("Transient source montage"),Source))
   {
    Source->RateScale=2.f;
    TestTrue(TEXT("Timed presentation accepts non-unit source rate"),Anim->PlayPresentation(Source,Idle,.5f,1.f)>0.f);
    const auto* Active=Anim->GetCurrentActiveMontage();
    if (TestNotNull(TEXT("Presentation clone active"),Active)) TestEqual(TEXT("Clone rate is normalized"),Active->RateScale,1.f);
    TestEqual(TEXT("Source rate is not mutated"),Source->RateScale,2.f);
    if (Authored) TestEqual(TEXT("Saved authored asset rate is not mutated"),Authored->RateScale,1.f);
   }
  }
  if (Anim) TestTrue(TEXT("Sequence fallback creates a playable timed montage"),Anim->PlayPresentation(nullptr,Idle,.5f,1.f)>0.f);
  Pattern->ConfigureProfile(Legacy);
  TestTrue(TEXT("Enabled legacy empty profile restores original mode"),Mesh->GetAnimationMode()==BaselineMode);
  TestTrue(TEXT("Enabled legacy empty profile restores original class"),Mesh->GetAnimClass()==BaselineClass);
  TestTrue(TEXT("Opt-out restores decal setting"),Mesh->bReceivesDecals);
  Pattern->ConfigureProfile(Human); Pattern->ConfigureProfile(nullptr);
  TestTrue(TEXT("Null opt-out restores original class"),Mesh->GetAnimClass()==BaselineClass);
  TestTrue(TEXT("Null opt-out restores original mode"),Mesh->GetAnimationMode()==BaselineMode);
  TestTrue(TEXT("Null opt-out restores decal setting"),Mesh->bReceivesDecals);
  Mesh->SetAnimInstanceClass(nullptr);
  Pattern->ConfigureProfile(Human); Pattern->ConfigureProfile(Legacy);
  TestTrue(TEXT("Empty legacy restores a null blueprint class"),Mesh->GetAnimClass()==nullptr);
  TestTrue(TEXT("Empty legacy clears a null-baseline instance"),Mesh->GetAnimInstance()==nullptr);
  Pattern->ConfigureProfile(Human); Pattern->ConfigureProfile(nullptr);
  TestTrue(TEXT("Null opt-out restores a null blueprint class"),Mesh->GetAnimClass()==nullptr);
  TestTrue(TEXT("Null opt-out clears a null-baseline instance"),Mesh->GetAnimInstance()==nullptr);
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return !HasAnyErrors();
}
#endif
