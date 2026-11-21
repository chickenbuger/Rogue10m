// Project-owned weapon presentation. Gameplay damage remains in CombatComponent.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "Rogue10mWeaponVisualComponent.generated.h"
class UStaticMesh;
class UAnimInstance;
class USkeletalMesh;
class USkeletalMeshComponent;
class USkeleton;
class UStaticMeshComponent;
class URogue10mWeaponRetargetAnimInstance;
/** Event-driven weapon meshes attached to the rendered character, never the hidden animation source. */
UCLASS(ClassGroup=(Rogue10m), meta=(BlueprintSpawnableComponent))
class ROGUE10M_API URogue10mWeaponVisualComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 URogue10mWeaponVisualComponent();
 // Pure presentation math, in any single consistent coordinate space. Output is unchanged on failure.
 static bool BuildStaffUprightGrip(const FTransform& HandWorld, const FTransform& AuthoredGrip,
  const FVector& FacingForward, float ActionWeight, FTransform& OutGrip);
 static bool BuildSwordPalmGrip(const FVector& RightPalm, const FVector& LeftPalm, const FVector& FingerAxis, FTransform& OutGrip);
 UFUNCTION(BlueprintCallable, Category="Rogue10m|Weapon Visual")
 void RefreshWeaponVisuals();
 UFUNCTION(BlueprintPure, Category="Rogue10m|Weapon Visual")
 UStaticMeshComponent* GetRightWeaponMesh() const { return RightWeaponMesh; }
 UFUNCTION(BlueprintPure, Category="Rogue10m|Weapon Visual")
 UStaticMeshComponent* GetLeftWeaponMesh() const { return LeftWeaponMesh; }
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 TSoftObjectPtr<UStaticMesh> GreatSwordMesh;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 TSoftObjectPtr<UStaticMesh> KnuckleMesh;
 // Assign only a verified staff asset; an empty slot intentionally remains hidden.
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 TSoftObjectPtr<UStaticMesh> StaffMesh;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 FName StaffAttachSocket = TEXT("hand_r");
 // Identity is an untuned fallback, not a visually validated staff grip.
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 FTransform StaffGrip = FTransform::Identity;
 // Opt-in positive local Y jewel-up rest pose. Authored DefaultSlot orientation wins during actions.
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 bool bStaffUprightIdle = false;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 FTransform GreatSwordGrip;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 FTransform RightKnuckleGrip;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rogue10m|Weapon Visual")
 FTransform LeftKnuckleGrip;
 // Only the HumanMale rendered body uses this weapon-specific retarget graph.
 UPROPERTY(EditDefaultsOnly, Category="Rogue10m|Weapon Visual")
 TSoftClassPtr<UAnimInstance> GreatSwordBodyAnimClass;
 UPROPERTY(EditDefaultsOnly, Category="Rogue10m|Weapon Visual")
 TSoftObjectPtr<USkeleton> GreatSwordBodySkeleton;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
 virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
private:
 friend class URogue10mWeaponRetargetAnimInstance;
 bool ConsumeEquipmentRetargetPose(UAnimInstance& Instance, TArray<FTransform>& OutPose, TBitArray<>& OutBones);
 void SwitchWeaponRetarget(USkeletalMeshComponent& Body, UClass* NewClass, bool bThirdPerson);
 TWeakObjectPtr<USkeletalMeshComponent> PendingRetargetBody;
 TWeakObjectPtr<UClass> PendingRetargetClass;
 TArray<FTransform> PendingRetargetPose;
 TBitArray<> PendingRetargetBones;
 void UnbindSwordGrip();
 void UpdateSwordGrip();
 void UnbindStaffGrip();
 void UpdateStaffGrip();
 TWeakObjectPtr<class USkeletalMeshComponent> StaffGripBody;
 FDelegateHandle StaffGripDelegate;
 FTransform LastStaffGrip;
 TWeakObjectPtr<UStaticMesh> StaffGripAsset;
 bool bOwnStaffGrip = false;
 TWeakObjectPtr<class USkeletalMeshComponent> GripBody;
 FDelegateHandle GripDelegate;
 FTransform LastGrip;
 bool bOwnGrip = false;
 void RefreshBodyRetarget(class USkeletalMeshComponent* Body, bool bGreatSword);
 UPROPERTY(Transient) TObjectPtr<UClass> PreviousBodyAnimClass;
 UPROPERTY(Transient) TObjectPtr<USkeletalMesh> PreviousBodyMesh;
 UPROPERTY(Transient) TObjectPtr<UClass> AppliedSwordBodyAnimClass;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RightWeaponMesh;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LeftWeaponMesh;
};
