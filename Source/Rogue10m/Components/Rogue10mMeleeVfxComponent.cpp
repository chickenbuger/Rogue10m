#include "Rogue10mMeleeVfxComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Rogue10mMeleeVfxGeometry.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr TCHAR MaterialPath[] = TEXT("/Game/Rogue10m/VFX/Character/Combat/Melee20261121/M_MeleeReadability.M_MeleeReadability");
constexpr int32 MaxTrails = 4;
constexpr int32 MaxSamplesPerTrack = 24;
constexpr int32 MaxRanges = 4;
constexpr int32 MaxRangePoints = 96;
constexpr int32 MaxImpacts = 24;
constexpr int32 TrailSection = 0;
constexpr int32 RangeSection = 1;
constexpr int32 ImpactSection = 2;

double TrailDuration(int32 Tier) { return 0.15 + 0.03 * Tier; }
double RangeDuration(int32 Tier) { return 0.24 + 0.03 * Tier; }
double ImpactDuration(int32 Tier) { return 0.18 + 0.025 * Tier; }

float Fade(double Age, double Duration)
{
	return 1.0f - static_cast<float>(FMath::Clamp(Age / Duration, 0.0, 1.0));
}

FLinearColor Tint(bool bSword)
{
	return bSword ? FLinearColor(0.035f, 0.69f, 1.0f) : FLinearColor(1.0f, 0.38f, 0.055f);
}

FLinearColor Opacity(FLinearColor Color, float Alpha)
{
	Color.A = Alpha;
	return Color;
}

// A single depth-tested material reads interpolated vertex color and opacity.
// Every section is short-lived and can only submit geometry within the bounded event queues.
struct FCueGeometry
{
	explicit FCueGeometry(const FVector& InOrigin) : Origin(InOrigin) {}

	int32 AddVertex(const FVector& Point, const FLinearColor& Color)
	{
		Colors.Add(Color);
		return Vertices.Add(Point - Origin);
	}

	void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, const FLinearColor& CD)
	{
		const int32 Start = AddVertex(A, CA);
		AddVertex(B, CB);
		AddVertex(C, CC);
		AddVertex(D, CD);
		Indices.Append({Start, Start + 1, Start + 2, Start + 1, Start + 3, Start + 2});
	}

	void Streak(const FVector& A, const FVector& B, const FVector& Side,
		const FLinearColor& Color, float AlphaA, float AlphaB)
	{
		const FLinearColor Clear = Opacity(Color, 0.0f);
		const FLinearColor CA = Opacity(Color, AlphaA);
		const FLinearColor CB = Opacity(Color, AlphaB);
		Quad(A - Side, A, B - Side, B, Clear, CA, Clear, CB);
		Quad(A, A + Side, B, B + Side, CA, Clear, CB, Clear);
	}

	void Submit(UProceduralMeshComponent& Mesh, int32 Section)
	{
		const FProcMeshSection* Existing = Mesh.GetProcMeshSection(Section);
		if (Vertices.IsEmpty())
		{
			if (Existing && !Existing->ProcVertexBuffer.IsEmpty())
			{
				Mesh.ClearMeshSection(Section);
			}
			return;
		}
		// Quad topology is shared by each cue. Reserving a small bucket avoids rebuilding
		// the scene proxy and empty collision setup whenever one ribbon sample arrives.
		const int32 Bucket = static_cast<int32>(FMath::RoundUpToPowerOfTwo(Vertices.Num()));
		const bool bReuseTopology = Existing && Existing->ProcVertexBuffer.Num() == Bucket;
		const FVector PaddingPosition = Vertices.Last() + Origin;
		while (Vertices.Num() < Bucket)
		{
			const int32 VertexIndex = Vertices.Num();
			AddVertex(PaddingPosition, FLinearColor::Transparent);
			AddVertex(PaddingPosition, FLinearColor::Transparent);
			AddVertex(PaddingPosition, FLinearColor::Transparent);
			AddVertex(PaddingPosition, FLinearColor::Transparent);
			if (!bReuseTopology)
			{
				Indices.Append({VertexIndex, VertexIndex + 1, VertexIndex + 2,
					VertexIndex + 1, VertexIndex + 3, VertexIndex + 2});
			}
		}
		if (bReuseTopology)
		{
			Mesh.UpdateMeshSection_LinearColor(Section, Vertices, TArray<FVector>(), TArray<FVector2D>(),
				Colors, TArray<FProcMeshTangent>(), false);
			return;
		}
		Mesh.CreateMeshSection_LinearColor(Section, Vertices, Indices,
			TArray<FVector>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false);
	}

	FVector Origin;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	TArray<FLinearColor> Colors;
};
}

URogue10mMeleeVfxComponent::URogue10mMeleeVfxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Rendered mesh bone finalization and weapon grip updates precede presentation sampling.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Material(MaterialPath, LOAD_NoWarn);
	CueMaterial = Material.Get();
}

bool URogue10mMeleeVfxComponent::EnsureMesh()
{
	if (CueMesh)
	{
		return true;
	}
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	if (!CueMaterial)
	{
		CueMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath, nullptr, LOAD_NoWarn);
		if (!CueMaterial)
		{
			return false;
		}
	}

	CueMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("MeleeVfxMesh"), RF_Transient);
	CueMesh->ComponentTags.Add(TEXT("Rogue10m.MeleeVfx"));
	CueMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CueMesh->SetGenerateOverlapEvents(false);
	CueMesh->SetCanEverAffectNavigation(false);
	CueMesh->SetCastShadow(false);
	CueMesh->SetReceivesDecals(false);
	CueMesh->SetTranslucentSortPriority(1);
	CueMesh->SetWorldLocation(Owner->GetActorLocation());
	CueMesh->SetMaterial(TrailSection, CueMaterial);
	CueMesh->SetMaterial(RangeSection, CueMaterial);
	CueMesh->SetMaterial(ImpactSection, CueMaterial);
	CueMesh->RegisterComponent();
	return true;
}

void URogue10mMeleeVfxComponent::BeginTrail(uint32 ExecutionId, UAnimInstance* AnimInstance, UAnimMontage* Montage,
	int32 MontageInstanceId, USkeletalMeshComponent* BodyMesh, UStaticMeshComponent* BladeMesh,
	FName PrimarySocket, FName SecondarySocket, int32 Tier, bool bSword, float SecondsToFirstContact)
{
	if (!AnimInstance || !Montage || MontageInstanceId == INDEX_NONE || !BodyMesh
		|| (bSword ? !BladeMesh : !BodyMesh->DoesSocketExist(PrimarySocket)) || !EnsureMesh())
	{
		return;
	}
	EndTrail(ExecutionId);
	if (Trails.Num() == MaxTrails)
	{
		Trails.RemoveAt(0, EAllowShrinking::No);
	}
	FTrail& Trail = Trails.AddDefaulted_GetRef();
	Trail.ExecutionId = ExecutionId;
	Trail.Animation = AnimInstance;
	Trail.Montage = Montage;
	Trail.MontageInstanceId = MontageInstanceId;
	Trail.Body = BodyMesh;
	Trail.Blade = BladeMesh;
	Trail.PrimarySocket = PrimarySocket;
	Trail.SecondarySocket = SecondarySocket;
	Trail.Tier = FMath::Clamp(Tier, 0, 3);
	Trail.bSword = bSword;
	const float Windup = FMath::IsFinite(SecondsToFirstContact) ? SecondsToFirstContact : 0.0f;
	Trail.CaptureAfter = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, Windup - (bSword ? 0.145f : 0.105f));
	SetComponentTickEnabled(true);
}

void URogue10mMeleeVfxComponent::EndTrail(uint32 ExecutionId)
{
	for (FTrail& Trail : Trails)
	{
		if (ExecutionId == 0 || Trail.ExecutionId == ExecutionId)
		{
			Trail.bTracking = false;
		}
	}
}

void URogue10mMeleeVfxComponent::ShowRange(const TArray<FVector>& WorldPolyline, int32 Tier, bool bSword)
{
	if (WorldPolyline.Num() < 3 || WorldPolyline.Num() > MaxRangePoints
		|| WorldPolyline.ContainsByPredicate([](const FVector& Point) { return Point.ContainsNaN(); }) || !EnsureMesh())
	{
		return;
	}
	if (Ranges.Num() == MaxRanges)
	{
		Ranges.RemoveAt(0, EAllowShrinking::No);
	}
	Ranges.Add({WorldPolyline, GetWorld()->GetTimeSeconds(), FMath::Clamp(Tier, 0, 3), bSword});
	SetComponentTickEnabled(true);
}

void URogue10mMeleeVfxComponent::ShowImpact(const FVector& WorldPoint, const FVector& SurfaceNormal, int32 Tier, bool bSword)
{
	if (WorldPoint.ContainsNaN() || SurfaceNormal.ContainsNaN() || !EnsureMesh())
	{
		return;
	}
	if (Impacts.Num() == MaxImpacts)
	{
		Impacts.RemoveAt(0, EAllowShrinking::No);
	}
	Impacts.Add({WorldPoint, SurfaceNormal.GetSafeNormal(), GetWorld()->GetTimeSeconds(), FMath::Clamp(Tier, 0, 3), bSword});
	SetComponentTickEnabled(true);
}

void URogue10mMeleeVfxComponent::AddTrailSample(FTrailTrack& Track, FTrailSample Sample, double MinimumSpeed)
{
	if (Sample.Base.ContainsNaN() || Sample.Tip.ContainsNaN())
	{
		Track.Previous.Reset();
		return;
	}
	if (Track.Previous)
	{
		const FTrailSample& Previous = Track.Previous.GetValue();
		const double Delta = Sample.Time - Previous.Time;
		const double Travel = FMath::Max(FVector::Distance(Previous.Base, Sample.Base), FVector::Distance(Previous.Tip, Sample.Tip));
		// A quiet pose, hitch or teleport starts a separate strip on the next real motion.
		if (Delta > 0.0 && Delta <= 0.085 && Travel <= 120.0 && Travel >= FMath::Max(0.6, MinimumSpeed * Delta))
		{
			if (Track.Samples.IsEmpty() || Track.Samples.Last().Time != Previous.Time)
			{
				Track.Samples.Add(Previous);
			}
			FTrailSample Connected = Sample;
			Connected.bConnect = true;
			Track.Samples.Add(Connected);
			if (Track.Samples.Num() > MaxSamplesPerTrack)
			{
				Track.Samples.RemoveAt(0, Track.Samples.Num() - MaxSamplesPerTrack, EAllowShrinking::No);
			}
		}
	}
	Track.Previous = Sample;
}

void URogue10mMeleeVfxComponent::SampleTrails(double Now)
{
	for (FTrail& Trail : Trails)
	{
		const double Oldest = Now - TrailDuration(Trail.Tier);
		const auto Expired = [Oldest](const FTrailSample& Sample) { return Sample.Time <= Oldest; };
		Trail.Primary.Samples.RemoveAll(Expired);
		Trail.Secondary.Samples.RemoveAll(Expired);
		if (!Trail.bTracking)
		{
			continue;
		}

		UAnimInstance* Animation = Trail.Animation.Get();
		const UAnimMontage* Montage = Trail.Montage.Get();
		USkeletalMeshComponent* Body = Trail.Body.Get();
		const FAnimMontageInstance* Instance = Animation && Montage
			? Animation->GetActiveInstanceForMontage(Montage) : nullptr;
		if (!Body || !Instance || Instance->GetInstanceID() != Trail.MontageInstanceId || !Instance->IsPlaying())
		{
			Trail.bTracking = false;
			continue;
		}
		if (Now < Trail.CaptureAfter)
		{
			continue;
		}
		if (Trail.bSword)
		{
			UStaticMeshComponent* Blade = Trail.Blade.Get();
			FTrailSample Sample;
			Sample.Time = Now;
			if (Blade && Blade->IsVisible() && Rogue10m::MeleeVfx::GetBladeSegment(*Blade, Sample.Base, Sample.Tip))
			{
				AddTrailSample(Trail.Primary, Sample, 125.0);
			}
			else
			{
				Trail.bTracking = false;
			}
			continue;
		}

		const auto SampleHand = [Body, Now](FName Socket, FTrailTrack& Track)
		{
			if (!Socket.IsNone() && Body->DoesSocketExist(Socket))
			{
				const FVector Center = Body->GetSocketLocation(Socket);
				AddTrailSample(Track, {Center, Center, Now}, 105.0);
			}
		};
		SampleHand(Trail.PrimarySocket, Trail.Primary);
		if (Trail.SecondarySocket != Trail.PrimarySocket)
		{
			SampleHand(Trail.SecondarySocket, Trail.Secondary);
		}
	}
	Trails.RemoveAll([](const FTrail& Trail)
	{
		return !Trail.bTracking && Trail.Primary.Samples.IsEmpty() && Trail.Secondary.Samples.IsEmpty();
	});
}

void URogue10mMeleeVfxComponent::DrawCues(double Now)
{
	const FVector Origin = GetOwner()->GetActorLocation();
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraPoint = Camera ? Camera->GetCameraLocation() : Origin + FVector(-260.0, 0.0, 160.0);
	CueMesh->SetWorldLocation(Origin);

	FCueGeometry TrailGeometry(Origin);
	for (const FTrail& Trail : Trails)
	{
		const FLinearColor Color = Tint(Trail.bSword);
		const auto BlendSample = [](const FTrailSample& A, const FTrailSample& B, double T)
		{
			return FTrailSample{FMath::Lerp(A.Base, B.Base, T), FMath::Lerp(A.Tip, B.Tip, T), FMath::Lerp(A.Time, B.Time, T)};
		};
		const auto SampleOpacity = [Now, &Trail](const FTrailSample& Sample)
		{
			return FMath::SmoothStep(0.f, 1.f, Fade(Now - Sample.Time, TrailDuration(Trail.Tier)));
		};
		const auto DrawSpan = [&](const FTrailSample& A, const FTrailSample& B, float FadeA, float FadeB)
		{
			if (Trail.bSword)
			{
				// Both layers lie in the local blade span. A feathered boundary keeps the blade tip sharp.
				const double InnerEdge = 0.75 - 0.13 * Trail.Tier;
				const FVector RootA = FMath::Lerp(A.Base, A.Tip, InnerEdge);
				const FVector RootB = FMath::Lerp(B.Base, B.Tip, InnerEdge);
				const FVector EdgeA = FMath::Lerp(A.Base, A.Tip, 0.95);
				const FVector EdgeB = FMath::Lerp(B.Base, B.Tip, 0.95);
				const float Fill = 0.17f + 0.045f * Trail.Tier;
				const FLinearColor EdgeColor = FMath::Lerp(Color, FLinearColor::White, 0.16f);
				TrailGeometry.Quad(RootA, EdgeA, RootB, EdgeB, Opacity(Color, 0.0f), Opacity(Color, Fill * FadeA),
					Opacity(Color, 0.0f), Opacity(Color, Fill * FadeB));
				TrailGeometry.Quad(EdgeA, A.Tip, EdgeB, B.Tip, Opacity(Color, Fill * FadeA), Opacity(Color, 0.0f),
					Opacity(Color, Fill * FadeB), Opacity(Color, 0.0f));
				const FVector CoreA = FMath::Lerp(A.Base, A.Tip, 0.94);
				const FVector CoreB = FMath::Lerp(B.Base, B.Tip, 0.94);
				const FVector Side = ((A.Tip - A.Base) + (B.Tip - B.Base)).GetSafeNormal() * 2.0;
				TrailGeometry.Streak(CoreA, CoreB, Side, EdgeColor, 0.57f * FadeA, 0.57f * FadeB);
				if (Trail.Tier >= 2)
				{
					const FVector InnerA = FMath::Lerp(A.Base, A.Tip, 0.74);
					const FVector InnerB = FMath::Lerp(B.Base, B.Tip, 0.74);
					TrailGeometry.Streak(InnerA, InnerB, Side * 0.7, Color, 0.28f * FadeA, 0.28f * FadeB);
				}
				return;
			}

			const FVector Center = (A.Base + B.Base) * 0.5;
			const FVector Side = FVector::CrossProduct(B.Base - A.Base, CameraPoint - Center).GetSafeNormal();
			TrailGeometry.Streak(A.Base, B.Base, Side * (4.5 + 1.5 * Trail.Tier),
				Color, 0.70f * FadeA, 0.70f * FadeB);
			if (Trail.Tier >= 1)
			{
				const FLinearColor WarmCore = FMath::Lerp(Color, FLinearColor::White, 0.40f);
				TrailGeometry.Streak(A.Base, B.Base, Side * 1.7, WarmCore, 0.28f * FadeA, 0.28f * FadeB);
			}
			if (Trail.Tier >= 2)
			{
				const FVector Accent = Side * (7.0 + Trail.Tier);
				TrailGeometry.Streak(A.Base + Accent, B.Base + Accent, Side * 1.7, Color,
					0.37f * FadeA, 0.37f * FadeB);
			}
			if (Trail.Tier >= 3)
			{
				const FVector Accent = Side * -8.0;
				TrailGeometry.Streak(A.Base + Accent, B.Base + Accent, Side * 1.2, Color,
					0.30f * FadeA, 0.30f * FadeB);
			}
		};
		const auto DrawTrack = [&](const FTrailTrack& Track)
		{
			for (int32 Index = 1; Index < Track.Samples.Num(); ++Index)
			{
				const FTrailSample& A = Track.Samples[Index - 1];
				const FTrailSample& B = Track.Samples[Index];
				if (!B.bConnect)
				{
					continue;
				}
				const bool bStart = Index == 1 || !A.bConnect;
				const bool bEnd = Index + 1 == Track.Samples.Num() || !Track.Samples[Index + 1].bConnect;
				const FTrailSample Left = bStart ? A : BlendSample(A, B, 0.26);
				const FTrailSample Right = bEnd ? B : BlendSample(A, B, 0.74);
				if (bStart || bEnd)
				{
					const FTrailSample Middle = BlendSample(A, B, 0.5);
					DrawSpan(Left, Middle, bStart ? 0.f : SampleOpacity(Left), SampleOpacity(Middle));
					DrawSpan(Middle, Right, SampleOpacity(Middle), bEnd ? 0.f : SampleOpacity(Right));
				}
				else
				{
					DrawSpan(Left, Right, SampleOpacity(Left), SampleOpacity(Right));
				}
				if (!bEnd)
				{
					// Cut each corner within the sampled triangle, avoiding extrapolation beyond the blade.
					const FTrailSample Join = BlendSample(B, Track.Samples[Index + 1], 0.26);
					const FTrailSample Middle = BlendSample(BlendSample(Right, B, 0.5), BlendSample(B, Join, 0.5), 0.5);
					DrawSpan(Right, Middle, SampleOpacity(Right), SampleOpacity(Middle));
					DrawSpan(Middle, Join, SampleOpacity(Middle), SampleOpacity(Join));
				}
			}
		};
		DrawTrack(Trail.Primary);
		DrawTrack(Trail.Secondary);
	}
	TrailGeometry.Submit(*CueMesh, TrailSection);

	FCueGeometry RangeGeometry(Origin);
	for (const FRangeCue& Range : Ranges)
	{
		const float Alpha = Fade(Now - Range.Time, RangeDuration(Range.Tier)) * (0.31f + 0.04f * Range.Tier);
		const FLinearColor Color = Tint(Range.bSword);
		double Perimeter = 0.0;
		for (int32 Index = 0; Index < Range.Outline.Num(); ++Index)
		{
			Perimeter += FVector::Distance(Range.Outline[Index], Range.Outline[(Index + 1) % Range.Outline.Num()]);
		}
		if (Perimeter < 1.0)
		{
			continue;
		}
		const int32 Accents = FMath::Clamp(FMath::RoundToInt(Perimeter / 70.0), 3, 9);
		const double Wavelength = Perimeter / Accents;
		const auto EdgeOpacity = [Alpha, Wavelength](double Distance)
		{
			const double Crest = 0.5 + 0.5 * FMath::Cos(2.0 * UE_PI * Distance / Wavelength);
			return Alpha * static_cast<float>(0.42 + 0.58 * Crest * Crest);
		};
		double Distance = 0.0;
		for (int32 Index = 0; Index < Range.Outline.Num(); ++Index)
		{
			const FVector& A = Range.Outline[Index];
			const FVector& B = Range.Outline[(Index + 1) % Range.Outline.Num()];
			const FVector Side = FVector::CrossProduct(FVector::UpVector, B - A).GetSafeNormal();
			const double Length = FVector::Distance(A, B);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length * 4.0 / Wavelength));
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const double Left = static_cast<double>(Step) / Steps;
				const double Right = static_cast<double>(Step + 1) / Steps;
				RangeGeometry.Streak(FMath::Lerp(A, B, Left), FMath::Lerp(A, B, Right),
					Side * (1.75 + 0.31 * Range.Tier), Color,
					EdgeOpacity(Distance + Left * Length), EdgeOpacity(Distance + Right * Length));
			}
			Distance += Length;
		}
	}
	RangeGeometry.Submit(*CueMesh, RangeSection);

	FCueGeometry ImpactGeometry(Origin);
	for (const FImpactCue& Impact : Impacts)
	{
		const double Age = Now - Impact.Time;
		const float Alpha = Fade(Age, ImpactDuration(Impact.Tier));
		const FLinearColor Color = Tint(Impact.bSword);
		const FVector Point = Impact.Point + Impact.Normal * 1.5;
		const FVector View = (CameraPoint - Point).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector Right = FVector::CrossProduct(View, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		const FVector Up = FVector::CrossProduct(Right, View).GetSafeNormal();
		// Small, high-contrast center followed by thin colored ejecta. The center does not expand to range size.
		const float Core = Alpha * Alpha;
		const double CoreExtent = 8.5 + Impact.Tier * 0.85;
		for (const FVector& Axis : {Right, -Right, Up, -Up})
		{
			const FVector Side = FVector::CrossProduct(Axis, View) * (2.15 + Impact.Tier * 0.19);
			ImpactGeometry.Streak(Point, Point + Axis * CoreExtent, Side, FLinearColor::White, Core, 0.0f);
		}

		const int32 SparkCount = 4 + Impact.Tier * 2;
		for (int32 Index = 0; Index < SparkCount; ++Index)
		{
			const double Angle = 0.42 + Index * 2.399963;
			const FVector Axis = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
			const FVector Side = FVector::CrossProduct(Axis, View) * (1.15 + 0.16 * Impact.Tier);
			const double Spread = 6.8 + Age * (68.0 + 10.0 * (Index % 3));
			const double Length = 6.3 + Impact.Tier * 1.55 + 2.0 * (Index % 2);
			ImpactGeometry.Streak(Point + Axis * Spread, Point + Axis * (Spread + Length),
				Side, Color, Alpha * (0.82f + 0.05f * Impact.Tier), 0.0f);
		}
	}
	ImpactGeometry.Submit(*CueMesh, ImpactSection);
}

void URogue10mMeleeVfxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!CueMesh || !GetOwner() || !GetWorld())
	{
		SetComponentTickEnabled(false);
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	SampleTrails(Now);
	Ranges.RemoveAll([Now](const FRangeCue& Range) { return Now - Range.Time >= RangeDuration(Range.Tier); });
	Impacts.RemoveAll([Now](const FImpactCue& Impact) { return Now - Impact.Time >= ImpactDuration(Impact.Tier); });
	DrawCues(Now);
	if (Trails.IsEmpty() && Ranges.IsEmpty() && Impacts.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

void URogue10mMeleeVfxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetComponentTickEnabled(false);
	if (CueMesh)
	{
		CueMesh->DestroyComponent();
		CueMesh = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
int32 URogue10mMeleeVfxComponent::GetActiveImpactCount() const { return Impacts.Num(); }
int32 URogue10mMeleeVfxComponent::GetActiveRangeCount() const { return Ranges.Num(); }

int32 URogue10mMeleeVfxComponent::GetTrailSampleCount() const
{
	int32 Count = 0;
	for (const FTrail& Trail : Trails)
	{
		Count += Trail.Primary.Samples.Num() + Trail.Secondary.Samples.Num();
	}
	return Count;
}

TOptional<FVector> URogue10mMeleeVfxComponent::GetLastImpactPoint() const
{
	return Impacts.IsEmpty() ? TOptional<FVector>() : TOptional<FVector>(Impacts.Last().Point);
}

const TArray<FVector>* URogue10mMeleeVfxComponent::GetLastRangeOutline() const
{
	return Ranges.IsEmpty() ? nullptr : &Ranges.Last().Outline;
}

int32 URogue10mMeleeVfxComponent::GetLastPresentationTier() const
{
	if (Ranges.IsEmpty())
	{
		return Impacts.IsEmpty() ? INDEX_NONE : Impacts.Last().Tier;
	}
	return Impacts.IsEmpty() || Ranges.Last().Time >= Impacts.Last().Time ? Ranges.Last().Tier : Impacts.Last().Tier;
}
#endif
