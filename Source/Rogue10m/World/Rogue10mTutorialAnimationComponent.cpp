// Copyright Epic Games, Inc. All Rights Reserved.
#include "Rogue10mTutorialAnimationComponent.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Rogue10m.h"
#include "Rogue10mTutorialDataAsset.h"
#include "Rogue10mTutorialNPC.h"
#include "TimerManager.h"

namespace Rogue10mTutorialAnimation
{
	constexpr float MovementSampleSeconds = 0.1f;
	constexpr float TeleportResetDistance = 300.0f;
}

URogue10mTutorialAnimationComponent::URogue10mTutorialAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URogue10mTutorialAnimationComponent::BeginPlay()
{
	Super::BeginPlay();
	bStopping = false;
	ResetMovementSample();
	if (UWorld* World = GetWorld())
	{
		FTimerManagerTimerParameters TimerParameters;
		TimerParameters.bLoop = true;
		TimerParameters.bMaxOncePerFrame = true;
		World->GetTimerManager().SetTimer(MovementTimer, this, &ThisClass::SampleMovement,
			Rogue10mTutorialAnimation::MovementSampleSeconds, TimerParameters);
	}
}

void URogue10mTutorialAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bStopping = true;
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(MovementTimer); }
	CurrentAnimation = nullptr;
	GuideMesh.Reset();
	TutorialData.Reset();
	FallbackIdleAnimation.Reset();
	Super::EndPlay(EndPlayReason);
}

UAnimSequence* URogue10mTutorialAnimationComponent::GetCompatibleAnimation(UAnimSequence* Candidate) const
{
	const USkeletalMeshComponent* Mesh = GuideMesh.Get();
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	return Candidate && Asset && Candidate->GetSkeleton() == Asset->GetSkeleton()
		&& Candidate->GetPlayLength() > 0.0f ? Candidate : nullptr;
}

void URogue10mTutorialAnimationComponent::Configure(
	USkeletalMeshComponent* InMesh, URogue10mTutorialDataAsset* InData)
{
	const bool bChanged = GuideMesh.Get() != InMesh || TutorialData.Get() != InData;
	GuideMesh = InMesh;
	TutorialData = InData;
	UAnimSequence* Idle = InData ? GetCompatibleAnimation(InData->GuideIdleAnimation) : nullptr;
	if (!Idle && InMesh && AnimationState == ERogue10mTutorialAnimationState::Idle)
	{
		Idle = GetCompatibleAnimation(Cast<UAnimSequence>(InMesh->AnimationData.AnimToPlay));
	}
	if (Idle) { FallbackIdleAnimation = Idle; }
	if (InData)
	{
		for (UAnimSequence* Candidate : {InData->GuideWalkAnimation.Get(), InData->GuideTalkAnimation.Get()})
		{
			if (Candidate && !GetCompatibleAnimation(Candidate))
			{
				UE_LOG(LogRogue10m, Warning, TEXT("Tutorial guide %s cannot play incompatible animation %s; using idle."),
					*GetNameSafe(GetOwner()), *GetNameSafe(Candidate));
			}
		}
	}
	if (HasBegunPlay())
	{
		if (bChanged) { ResetMovementSample(); }
		RefreshAnimation();
	}
}

void URogue10mTutorialAnimationComponent::SetDialogueOpen(bool bOpen)
{
	bDialogueOpen = bOpen;
	if (HasBegunPlay() && !bStopping) { RefreshAnimation(); }
}

void URogue10mTutorialAnimationComponent::ResetMovementSample()
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	bHasMovementSample = Owner && World;
	LastLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	LastSampleTime = World ? World->GetTimeSeconds() : 0.0;
	MeasuredSpeed = 0.0f;
	bMoving = false;
}

void URogue10mTutorialAnimationComponent::SampleMovement()
{
	if (bStopping) { return; }
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World) { return; }
	// 데이터 자산 교체도 실제 NPC 설정을 따릅니다. 에디터의 정지 외형은 바꾸지 않습니다.
	if (const ARogue10mTutorialNPC* NPC = Cast<ARogue10mTutorialNPC>(Owner))
	{
		if (TutorialData.Get() != NPC->TutorialData.Get() || GuideMesh.Get() != NPC->GuideMesh.Get())
		{
			Configure(NPC->GuideMesh, NPC->TutorialData);
			return;
		}
	}
	const FVector Location = Owner->GetActorLocation();
	const double Now = World->GetTimeSeconds();
	const double Elapsed = Now - LastSampleTime;
	const float Distance = FVector::Dist2D(Location, LastLocation);
	// 같은 게임 프레임의 중복 호출은 이동 상태와 클립 재생 시간을 초기화하지 않습니다.
	if (bHasMovementSample && Elapsed >= 0.0 && Elapsed <= UE_SMALL_NUMBER) { return; }
	if (!bHasMovementSample || Elapsed < 0.0 || Distance > Rogue10mTutorialAnimation::TeleportResetDistance)
	{
		ResetMovementSample();
	}
	else
	{
		MeasuredSpeed = static_cast<float>(Distance / Elapsed);
		const URogue10mTutorialDataAsset* Data = TutorialData.Get();
		const float StartSpeed = FMath::Max(1.0f, Data ? Data->GuideMinimumWalkSpeed : 10.0f);
		bMoving = MeasuredSpeed > (bMoving ? StartSpeed * 0.5f : StartSpeed);
		LastLocation = Location;
		LastSampleTime = Now;
	}
	RefreshAnimation();
}

void URogue10mTutorialAnimationComponent::RefreshAnimation()
{
	USkeletalMeshComponent* Mesh = GuideMesh.Get();
	const URogue10mTutorialDataAsset* Data = TutorialData.Get();
	if (!Mesh || bStopping) { return; }
	ERogue10mTutorialAnimationState DesiredState = bDialogueOpen
		? ERogue10mTutorialAnimationState::Talk
		: bMoving ? ERogue10mTutorialAnimationState::Walk : ERogue10mTutorialAnimationState::Idle;
	UAnimSequence* Desired = Data ? GetCompatibleAnimation(
		DesiredState == ERogue10mTutorialAnimationState::Talk ? Data->GuideTalkAnimation.Get()
		: DesiredState == ERogue10mTutorialAnimationState::Walk ? Data->GuideWalkAnimation.Get()
		: Data->GuideIdleAnimation.Get()) : nullptr;
	if (!Desired)
	{
		Desired = Data ? GetCompatibleAnimation(Data->GuideIdleAnimation) : nullptr;
		if (!Desired) { Desired = GetCompatibleAnimation(FallbackIdleAnimation.Get()); }
		DesiredState = ERogue10mTutorialAnimationState::Idle;
	}
	AnimationState = DesiredState;
	CurrentAnimation = Desired;
	if (!Desired) { return; }
	const UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance();
	if (Mesh->GetAnimationMode() != EAnimationMode::AnimationSingleNode
		|| !Instance || Instance->GetAnimationAsset() != Desired)
	{
		Mesh->PlayAnimation(Desired, true);
	}
	const float Rate = DesiredState == ERogue10mTutorialAnimationState::Walk
		? FMath::Clamp(MeasuredSpeed / FMath::Max(1.0f, Data->GuideWalkReferenceSpeed), 0.01f, 10.0f) : 1.0f;
	Mesh->SetPlayRate(Rate);
}
