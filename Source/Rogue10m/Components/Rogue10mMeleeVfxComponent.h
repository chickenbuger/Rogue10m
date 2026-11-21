#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rogue10mMeleeVfxComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UMaterialInterface;
class UProceduralMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/** Short, world-space presentation of melee motion, query range and confirmed impact. */
UCLASS(ClassGroup=(Rogue10m))
class ROGUE10M_API URogue10mMeleeVfxComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	URogue10mMeleeVfxComponent();

	void BeginTrail(uint32 ExecutionId, UAnimInstance* AnimInstance, UAnimMontage* Montage, int32 MontageInstanceId,
		USkeletalMeshComponent* BodyMesh, UStaticMeshComponent* BladeMesh, FName PrimarySocket, FName SecondarySocket,
		int32 Tier, bool bSword, float SecondsToFirstContact);
	/** Stop sampling the execution (or all executions when ID is zero), retaining the short fade. */
	void EndTrail(uint32 ExecutionId);
	void ShowRange(const TArray<FVector>& WorldPolyline, int32 Tier, bool bSword);
	void ShowImpact(const FVector& WorldPoint, const FVector& SurfaceNormal, int32 Tier, bool bSword);

#if !UE_BUILD_SHIPPING
	int32 GetActiveImpactCount() const;
	int32 GetActiveRangeCount() const;
	int32 GetTrailSampleCount() const;
	TOptional<FVector> GetLastImpactPoint() const;
	const TArray<FVector>* GetLastRangeOutline() const;
	int32 GetLastPresentationTier() const;
#endif

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FTrailSample
	{
		FVector Base = FVector::ZeroVector;
		FVector Tip = FVector::ZeroVector;
		double Time = 0.0;
		bool bConnect = false;
	};

	struct FTrailTrack
	{
		TArray<FTrailSample> Samples;
		TOptional<FTrailSample> Previous;
	};

	struct FTrail
	{
		TWeakObjectPtr<UAnimInstance> Animation;
		TWeakObjectPtr<UAnimMontage> Montage;
		TWeakObjectPtr<USkeletalMeshComponent> Body;
		TWeakObjectPtr<UStaticMeshComponent> Blade;
		FName PrimarySocket;
		FName SecondarySocket;
		FTrailTrack Primary;
		FTrailTrack Secondary;
		double CaptureAfter = 0.0;
		uint32 ExecutionId = 0;
		int32 MontageInstanceId = INDEX_NONE;
		int32 Tier = 0;
		bool bSword = false;
		bool bTracking = true;
	};

	struct FRangeCue
	{
		TArray<FVector> Outline;
		double Time = 0.0;
		int32 Tier = 0;
		bool bSword = false;
	};

	struct FImpactCue
	{
		FVector Point = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		double Time = 0.0;
		int32 Tier = 0;
		bool bSword = false;
	};

	bool EnsureMesh();
	void SampleTrails(double Now);
	static void AddTrailSample(FTrailTrack& Track, FTrailSample Sample, double MinimumSpeed);
	void DrawCues(double Now);

	UPROPERTY(EditDefaultsOnly, Category="Rogue10m|Visual")
	TObjectPtr<UMaterialInterface> CueMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CueMesh;

	TArray<FTrail> Trails;
	TArray<FRangeCue> Ranges;
	TArray<FImpactCue> Impacts;
};
