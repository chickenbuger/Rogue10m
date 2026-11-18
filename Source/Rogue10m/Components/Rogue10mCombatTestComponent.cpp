#include "Rogue10mCombatTestComponent.h"

#include "AI/Rogue10mMonsterAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Rogue10mLiveCollisionDebug.h"
#include "Rogue10mGameState.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Rogue10m.h"
#include "Rogue10mAttributeSet.h"
#include "Rogue10mBasicMonster.h"
#include "Rogue10mCharacter.h"
#include "Rogue10mCombatTestWidget.h"
#include "Rogue10mMonsterAttackPatternComponent.h"
#include "Rogue10mPlayerController.h"

URogue10mCombatTestComponent::URogue10mCombatTestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	const TCHAR* Names[] = {TEXT("초록 슬라임"), TEXT("포자 몬스터"), TEXT("붉은 슬라임"), TEXT("철갑 몬스터"), TEXT("가시 수호자"), TEXT("아자토르 · 최종 보스")};
	const TCHAR* Classes[] = {TEXT("VerdantSlime"), TEXT("Sporeling"), TEXT("CrimsonSlime"), TEXT("IronShell"), TEXT("ThornWarden"), TEXT("FinalBoss")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		FRogue10mCombatTestCreature& Creature = Creatures.AddDefaulted_GetRef();
		Creature.Name = FText::FromString(Names[Index]);
		Creature.MonsterClass = TSoftClassPtr<ARogue10mBasicMonster>(FSoftObjectPath(FString::Printf(
			TEXT("/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_%s.BP_OW_%s_C"), Classes[Index], Classes[Index])));
	}
}

bool URogue10mCombatTestComponent::IsAvailable() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	return PC && PC->IsLocalPlayerController() && PC->HasAuthority() && GetWorld()
		&& GetWorld()->GetNetMode() == NM_Standalone && !PC->IsMenuWorld();
#endif
}

void URogue10mCombatTestComponent::InitializeUI()
{
	if (!IsAvailable() || Widget) return;
	auto* PC = CastChecked<ARogue10mPlayerController>(GetOwner());
	Widget = CreateWidget<URogue10mCombatTestWidget>(PC, URogue10mCombatTestWidget::StaticClass());
	if (!Widget) return;
	Widget->InitializeTools(this);
	Widget->AddToPlayerScreen(180);
	PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
    if (auto* Run=GetWorld()->GetGameState<ARogue10mGameState>())
    { Run->OnRunPhaseChanged.AddUniqueDynamic(this,&ThisClass::HandleSpatialRunPhaseChanged); }
	SetStatus(TEXT("몬스터를 골라 현재 위치 앞에 소환하세요. 최대 6마리."));
}

void URogue10mCombatTestComponent::BindInput(UInputComponent* Input)
{
	if (!IsAvailable() || !Input) return;
	Input->BindKey(EKeys::F6, IE_Pressed, this, &ThisClass::TogglePanel).bExecuteWhenPaused = true;
}

void URogue10mCombatTestComponent::TogglePanel() { SetPanelOpen(!bPanelOpen); }

void URogue10mCombatTestComponent::SetPanelOpen(bool bOpen)
{
	if (!IsAvailable() || bOpen == bPanelOpen) return;
	auto* PC = CastChecked<ARogue10mPlayerController>(GetOwner());
	if (bOpen && PC->IsRunResultVisible()) return;
	if (bOpen)
	{
		InitializeUI();
		if (!Widget) return;
		PC->CloseAllBlockingPanels();
		bPanelOpen = true;
		if (auto* Character = Cast<ARogue10mCharacter>(PC->GetPawn())) Character->GetCharacterMovement()->StopMovementImmediately();
		bOwnsPause = !UGameplayStatics::IsGamePaused(this) && PC->SetPause(true);
	}
	else
	{
		bPanelOpen = false;
		if (bOwnsPause) PC->SetPause(false);
		bOwnsPause = false;
	}
	RefreshView();
	PC->RefreshInputMode();
}

bool URogue10mCombatTestComponent::IsSpatialDebugEnabled() const
{
    const auto* PC=Cast<ARogue10mPlayerController>(GetOwner());
    const auto* Player=PC?Cast<ARogue10mCharacter>(PC->GetPawn()):nullptr;
    const auto* Run=GetWorld()?GetWorld()->GetGameState<ARogue10mGameState>():nullptr;
    return bSpatialDebugEnabled && IsAvailable() && Player && !Player->IsDead()
        && !PC->IsRunResultVisible() && (!Run || !Run->IsRunTerminal()) && GetTestMonsterCount()>0;
}

bool URogue10mCombatTestComponent::SetSpatialDebugEnabled(bool bEnabled)
{
    if (bEnabled && (!IsAvailable() || GetTestMonsterCount()==0))
    { SetSpatialDebugEnabled(false); SetStatus(TEXT("판정 표시 전에 살아 있는 테스트 몬스터를 소환하세요.")); return false; }
    bSpatialDebugEnabled=bEnabled;
    if (bEnabled && !IsSpatialDebugEnabled())
    { SetSpatialDebugEnabled(false); SetStatus(TEXT("살아 있는 플레이어의 진행 중 테스트에서만 표시할 수 있습니다.")); return false; }
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SpatialDebugTimer);
    if (!bEnabled)
    {
        SetCollisionReferenceEnabled(false);
        if (SpatialLines) { SpatialLines->Flush(); SpatialLines->DestroyComponent(); SpatialLines=nullptr; }
        LastAttackQuery=FRogue10mAttackDebugQuery(); AcceptedTargets.Reset(); LastQueryTime=-1.f;
        SupportedColliderCount=UnsupportedColliderCount=RecordedQueryCount=LiveSkeletalShapeCount=UnresolvedColliderCount=0;
        bHasFirstLiveSkeletalShape=false;
    }
    else
    {
        if (!SpatialLines)
        {
            SpatialLines=NewObject<ULineBatchComponent>(GetWorld(),NAME_None,RF_Transient);
            SpatialLines->bCalculateAccurateBounds=false;
            SpatialLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SpatialLines->RegisterComponentWithWorld(GetWorld());
        }
        RefreshSpatialDebug();
        GetWorld()->GetTimerManager().SetTimer(SpatialDebugTimer,this,&ThisClass::RefreshSpatialDebug,0.05f,true);
    }
    SetStatus(bEnabled?TEXT("판정 표시 켜짐 · 청록=실제 쿼리, 노랑=조건 통과, 초록=피해 성공. F6를 닫고 공격하세요.")
        :TEXT("판정 표시 꺼짐 · 이 테스트의 도형만 정리했습니다."));
    return true;
}

int32 URogue10mCombatTestComponent::GetSpatialDebugLineCount() const
{ return SpatialLines?SpatialLines->BatchedLines.Num()+SpatialLines->BatchedPoints.Num():0; }

bool URogue10mCombatTestComponent::SetCollisionReferenceEnabled(bool bEnabled)
{
    if (!bEnabled)
    {
        // Restore only the exact viewport flag acquired here; no global console command.
        if (bOwnCollisionReference)
            if (auto* Viewport=CollisionReferenceViewport.Get()) Viewport->EngineShowFlags.SetCollision(bSavedViewportCollision);
        bOwnCollisionReference=false; CollisionReferenceViewport.Reset(); RefreshView(); return true;
    }
    if (!IsSpatialDebugEnabled() || !GetWorld() || !GetWorld()->GetGameViewport()) return false;
    auto* Viewport=GetWorld()->GetGameViewport();
    if (bOwnCollisionReference && CollisionReferenceViewport.Get()!=Viewport) SetCollisionReferenceEnabled(false);
    if (!bOwnCollisionReference)
    {
        CollisionReferenceViewport=Viewport; bSavedViewportCollision=Viewport->EngineShowFlags.Collision;
        bOwnCollisionReference=true;
    }
    Viewport->EngineShowFlags.SetCollision(true); RefreshView(); return true;
}

bool URogue10mCombatTestComponent::IsCollisionReferenceEnabled() const
{
    const auto* Viewport=CollisionReferenceViewport.Get();
    return bOwnCollisionReference && Viewport && Viewport->EngineShowFlags.Collision;
}

bool URogue10mCombatTestComponent::TryGetFirstLiveSkeletalShapeCenter(FVector& OutCenter) const
{
    if (!IsSpatialDebugEnabled() || !bHasFirstLiveSkeletalShape) return false;
    OutCenter=FirstLiveSkeletalShapeCenter; return true;
}

FText URogue10mCombatTestComponent::GetSpatialDebugSummary() const
{
    if (!IsSpatialDebugEnabled()) return FText::FromString(TEXT("판정 표시 꺼짐"));
    const FString PulseKind=RecordedQueryCount==0?TEXT("공격 실행 대기 · 실제 판정 시점의 쿼리만 표시합니다.")
        :LastAttackQuery.bLockedTargetReuse?TEXT("고정 대상 재사용 · 새 범위 쿼리 없음")
        :FString::Printf(TEXT("실행 쿼리 %d · pulse %d · 후보 component %d / 조건 통과 %d / 피해 성공 %d"),
            RecordedQueryCount,LastAttackQuery.Pulse,LastAttackQuery.OverlapComponents.Num(),LastAttackQuery.EligibleTargets.Num(),AcceptedTargets.Num());
    FString ComponentNames;
    int32 NameCount=0;
    for (const auto& Weak:LastAttackQuery.OverlapComponents)
    {
        if (const auto* Primitive=Weak.Get())
        {
            if (NameCount++) ComponentNames+=TEXT(", ");
            ComponentNames+=Primitive->GetName();
            if (NameCount>=3) break;
        }
    }
    if (ComponentNames.IsEmpty()) ComponentNames=TEXT("없음");
    return FText::FromString(FString::Printf(TEXT("live query 도형 %d (skeletal %d) · convex/mesh/wrapper 미표시 %d · live body 미확인 %d\n%s\n최근 실제 overlap component: %s\nArc: 구체 query + Actor 원점 각도 필터 · 벽 가림 검사 없음"),
        SupportedColliderCount,LiveSkeletalShapeCount,UnsupportedColliderCount,UnresolvedColliderCount,*PulseKind,*ComponentNames));
}

void URogue10mCombatTestComponent::RecordAttackQuery(const FRogue10mAttackDebugQuery& Query)
{
    if (!IsSpatialDebugEnabled()) return;
    LastAttackQuery=Query; AcceptedTargets.Reset(); ++RecordedQueryCount;
    LastQueryTime=GetWorld()->GetTimeSeconds(); RefreshSpatialDebug();
}

void URogue10mCombatTestComponent::RecordAcceptedTarget(AActor& Actor,uint32 ExecutionId,int32 Pulse,bool bLockedReuse)
{
    if (!IsSpatialDebugEnabled()) { if (bSpatialDebugEnabled) SetSpatialDebugEnabled(false); return; }
    if (LastAttackQuery.ExecutionId!=ExecutionId || LastAttackQuery.Pulse!=Pulse) return;
    LastAttackQuery.bLockedTargetReuse=bLockedReuse;
    AcceptedTargets.AddUnique(&Actor); RefreshSpatialDebug();
}

bool URogue10mCombatTestComponent::DrawQueryCollider(UPrimitiveComponent& Primitive,const FLinearColor& Color)
{
    if (!SpatialLines) return false;
    TArray<FRogue10mLiveQueryShape> Shapes;
    Rogue10mLiveCollisionDebug::ReadQueryPrimitives(Primitive,Shapes,UnsupportedColliderCount,UnresolvedColliderCount);
    for (const auto& Shape:Shapes)
    {
        switch (Shape.Type)
        {
        case ERogue10mLiveQueryShape::Sphere: SpatialLines->DrawSphere(Shape.Center,Shape.Radius,24,Color,0.08f,0,1.8f); break;
        case ERogue10mLiveQueryShape::Box: SpatialLines->DrawBox(Shape.Center,Shape.Extent,Shape.Rotation,Color,0.08f,0,1.8f); break;
        case ERogue10mLiveQueryShape::Capsule: SpatialLines->DrawCapsule(Shape.Center,Shape.HalfHeight,Shape.Radius,Shape.Rotation,Color,0.08f,0,1.8f); break;
        }
        ++SupportedColliderCount;
        if (Primitive.IsA<USkeletalMeshComponent>())
        {
            ++LiveSkeletalShapeCount;
            if (!bHasFirstLiveSkeletalShape) { bHasFirstLiveSkeletalShape=true; FirstLiveSkeletalShapeCenter=Shape.Center; }
        }
    }
    return !Shapes.IsEmpty();
}

void URogue10mCombatTestComponent::RefreshSpatialDebug()
{
    if (!IsSpatialDebugEnabled()) { if (bSpatialDebugEnabled) SetSpatialDebugEnabled(false); return; }
    if (!SpatialLines) return;
    SpatialLines->Flush(); SupportedColliderCount=UnsupportedColliderCount=LiveSkeletalShapeCount=UnresolvedColliderCount=0;
    bHasFirstLiveSkeletalShape=false;
    const FLinearColor ColliderColor(0.72f,0.32f,1.f), QueryColor(0.f,0.9f,1.f), EligibleColor(1.f,0.75f,0.05f), AcceptedColor(0.1f,1.f,0.2f);
    for (const auto& Weak:TestMonsters)
    {
        auto* Monster=Weak.Get(); if (!Monster || Monster->IsDead()) continue;
        TInlineComponentArray<UPrimitiveComponent*> Primitives(Monster);
        for (UPrimitiveComponent* Primitive:Primitives) if (Primitive) DrawQueryCollider(*Primitive,ColliderColor);
    }
    if (LastQueryTime>=0.f && GetWorld()->GetTimeSeconds()-LastQueryTime<=0.6f)
    {
        if (!LastAttackQuery.bLockedTargetReuse)
        {
            if (LastAttackQuery.Shape==ERogue10mAttackShape::Arc || LastAttackQuery.Shape==ERogue10mAttackShape::Circle)
            { SpatialLines->DrawSphere(LastAttackQuery.Center,LastAttackQuery.Radius,32,QueryColor,0.08f,0,2.f); }
            else SpatialLines->DrawBox(LastAttackQuery.Center,LastAttackQuery.Extent,LastAttackQuery.Rotation,QueryColor,0.08f,0,2.f);
            if (LastAttackQuery.Shape==ERogue10mAttackShape::Arc)
            {
                // Only a filter illustration; cyan sphere above is the actual overlap geometry.
                SpatialLines->DrawCone(LastAttackQuery.Origin,LastAttackQuery.Forward,LastAttackQuery.Radius,
                    FMath::DegreesToRadians(LastAttackQuery.ArcAngleDegrees*0.5f),FMath::DegreesToRadians(LastAttackQuery.ArcAngleDegrees*0.5f),24,EligibleColor,0.08f,0,1.f);
            }
        }
        for (const auto& Weak:LastAttackQuery.EligibleTargets)
            if (const auto* Actor=Weak.Get()) SpatialLines->DrawPoint(Actor->GetActorLocation(),EligibleColor,12.f,0,0.08f);
        for (const auto& Weak:AcceptedTargets)
            if (const auto* Actor=Weak.Get()) SpatialLines->DrawPoint(Actor->GetActorLocation(),AcceptedColor,20.f,0,0.08f);
    }
    if (Widget) RefreshView();
}

void URogue10mCombatTestComponent::HandleSpatialRunPhaseChanged(ERogue10mRunPhase Phase)
{
    if (Phase==ERogue10mRunPhase::Victory || Phase==ERogue10mRunPhase::Defeat || Phase==ERogue10mRunPhase::Abandoned)
    { SetSpatialDebugEnabled(false); }
}

void URogue10mCombatTestComponent::SelectCreature(int32 Index)
{
	if (IsAvailable() && Creatures.IsValidIndex(Index)) SelectedIndex = Index;
}

ARogue10mBasicMonster* URogue10mCombatTestComponent::GetTestTarget() const
{
	auto* Monster = Target.Get();
	return IsValid(Monster) && !Monster->IsDead() ? Monster : nullptr;
}

int32 URogue10mCombatTestComponent::GetTestMonsterCount() const
{
	int32 Count = 0;
	for (const auto& Weak : TestMonsters) if (Weak.IsValid() && !Weak->IsDead()) ++Count;
	return Count;
}

bool URogue10mCombatTestComponent::FindPlacement(UClass* MonsterClass, float Distance,
	AActor* IgnoredActor, FTransform& OutTransform) const
{
	const auto* PC = Cast<ARogue10mPlayerController>(GetOwner());
	const auto* Player = PC ? Cast<ARogue10mCharacter>(PC->GetPawn()) : nullptr;
	const auto* CDO = MonsterClass ? Cast<ARogue10mBasicMonster>(MonsterClass->GetDefaultObject()) : nullptr;
	if (!Player || !CDO || !GetWorld()) return false;
	const auto* Capsule = CDO->GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatTestGround), false, Player);
	if (IgnoredActor) Params.AddIgnoredActor(IgnoredActor);
	const FVector Origin = Player->GetActorLocation();
	const float Angles[] = {0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 110.0f, -110.0f, 180.0f};
	for (const float Angle : Angles)
	{
		const FVector Direction = FRotator(0, PC->GetControlRotation().Yaw + Angle, 0).Vector();
		const FVector Candidate = Origin + Direction * Distance;
		FHitResult Ground;
		if (!GetWorld()->LineTraceSingleByChannel(Ground, Candidate + FVector(0,0,500),
			Candidate - FVector(0,0,1000), ECC_Visibility, Params) || Ground.ImpactNormal.Z < 0.7f
			|| FMath::Abs(Ground.ImpactPoint.Z - (Origin.Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())) > 250.0f) continue;
		const FVector Location = Ground.ImpactPoint + FVector(0,0,HalfHeight + 4.0f);
		FHitResult Barrier;
		if (GetWorld()->LineTraceSingleByChannel(Barrier, Origin, Location, ECC_Visibility, Params)) continue;
		if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(Radius + 4.0f, HalfHeight), Params)) continue;
		OutTransform = FTransform((Origin - Location).Rotation(), Location);
		return true;
	}
	return false;
}

bool URogue10mCombatTestComponent::SpawnMonster()
{
	TestMonsters.RemoveAll([](const auto& Weak) { return !Weak.IsValid(); });
	TestControllers.RemoveAll([](const auto& Weak) { return !Weak.IsValid(); });
	if (!Creatures.IsValidIndex(SelectedIndex)) return false;
	if (GetTestMonsterCount() >= 6) { SetStatus(TEXT("6마리 제한입니다. 테스트 몬스터 정리 후 다시 소환하세요.")); return false; }
	UClass* Class = Creatures[SelectedIndex].MonsterClass.LoadSynchronous();
	FTransform Transform;
	if (!Class || !FindPlacement(Class, FMath::Clamp(SpawnDistance, 250.0f, 1000.0f), nullptr, Transform))
	{
		SetStatus(TEXT("근처에 소환할 빈 지면이 없습니다. 조금 이동한 뒤 다시 시도하세요.")); return false;
	}
	auto* PC = CastChecked<ARogue10mPlayerController>(GetOwner());
	auto* Monster = GetWorld()->SpawnActorDeferred<ARogue10mBasicMonster>(Class, Transform,
		PC, PC->GetPawn(), ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
	if (!Monster) { SetStatus(TEXT("소환 위치가 막혔습니다. 다른 위치에서 시도하세요.")); return false; }
	Monster->MarkCombatTestInstance();
	UGameplayStatics::FinishSpawningActor(Monster, Transform);
	if (!IsValid(Monster)) return false;
	if (auto* AI = Cast<ARogue10mMonsterAIController>(Monster->GetController())) AI->StopMonsterAI();
	TestMonsters.Add(Monster);
	TestControllers.Add(Monster->GetController());
	Target = Monster;
	bTargetAI = false;
	SetStatus(FString::Printf(TEXT("%s 소환 완료 · AI 정지. 패턴 또는 자동 전투를 선택하세요."), *Monster->GetMonsterDisplayName().ToString()));
	return true;
}

void URogue10mCombatTestComponent::ClearMonsters()
{
    SetSpatialDebugEnabled(false);
	for (const auto& Weak : TestMonsters)
	{
		if (auto* Monster = Weak.Get())
		{
			Monster->Destroy();
		}
	}
	for (const auto& Weak : TestControllers) if (AController* AI = Weak.Get()) AI->Destroy();
	TestControllers.Reset(); TestMonsters.Reset(); Target.Reset(); bTargetAI = false;
}

bool URogue10mCombatTestComponent::StartPattern(ERogue10mMonsterAttackType Type)
{
	auto* Monster = GetTestTarget();
	auto* PC = CastChecked<ARogue10mPlayerController>(GetOwner());
	auto* Player = Cast<ARogue10mCharacter>(PC->GetPawn());
	auto* Pattern = Monster ? Monster->GetAttackPatternComponent() : nullptr;
	if (!Pattern || !Player || Player->IsDead()) { SetStatus(TEXT("살아 있는 플레이어와 테스트 대상이 필요합니다.")); return false; }
	const float Distance = Type == ERogue10mMonsterAttackType::Basic
		? FMath::Clamp(Pattern->GetProfile() ? Pattern->GetProfile()->AttackRange - 20.0f : 220.0f, 1.0f, 220.0f) : 600.0f;
	FTransform Transform;
	if (!FindPlacement(Monster->GetClass(), Distance, Monster, Transform))
	{ SetStatus(TEXT("공격 시험 위치가 막혔습니다. 빈 공간으로 조금 이동하세요.")); return false; }
	if (auto* AI = Cast<ARogue10mMonsterAIController>(Monster->GetController())) AI->StopMonsterAI();
	bTargetAI = false;
	Pattern->CancelAttack();
	Monster->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	if (!Pattern->TryStartCombatTestAttack(Player, Type))
	{ SetStatus(TEXT("이 몬스터에 없는 패턴이거나 공격 조건을 만족하지 못했습니다.")); return false; }
	SetStatus(TEXT("선택한 공격을 실행합니다. F6로 다시 열 수 있습니다."));
	SetPanelOpen(false);
	return true;
}

bool URogue10mCombatTestComponent::ExecuteAction(ERogue10mCombatTestAction Action)
{
	if (!IsAvailable()) return false;
	if (Action == ERogue10mCombatTestAction::TogglePanel) { TogglePanel(); return true; }
    if (Action == ERogue10mCombatTestAction::ToggleSpatialDebug) return SetSpatialDebugEnabled(!IsSpatialDebugEnabled());
    if (Action == ERogue10mCombatTestAction::ToggleCollisionReference) return SetCollisionReferenceEnabled(!IsCollisionReferenceEnabled());
	if (Action == ERogue10mCombatTestAction::Spawn) return SpawnMonster();
	if (Action == ERogue10mCombatTestAction::Clear) { ClearMonsters(); SetStatus(TEXT("이 창에서 소환한 테스트 몬스터를 모두 정리했습니다.")); return true; }
	if (Action == ERogue10mCombatTestAction::Basic) return StartPattern(ERogue10mMonsterAttackType::Basic);
	if (Action == ERogue10mCombatTestAction::FrontalWave) return StartPattern(ERogue10mMonsterAttackType::FrontalWave);
	if (Action == ERogue10mCombatTestAction::AbyssZone) return StartPattern(ERogue10mMonsterAttackType::AbyssZone);
	auto* PC = CastChecked<ARogue10mPlayerController>(GetOwner());
	auto* Player = Cast<ARogue10mCharacter>(PC->GetPawn());
	if (Action == ERogue10mCombatTestAction::Heal)
	{
		if (!Player || Player->IsDead() || !Player->GetRogueAttributeSet()) { SetStatus(TEXT("사망 후에는 플레이를 다시 시작하세요. 살아 있을 때만 회복 가능합니다.")); return false; }
		Player->GetRogueAttributeSet()->RestoreVitals(); SetStatus(TEXT("플레이어 체력·스태미나·마나 회복 완료.")); return true;
	}
	if (Action == ERogue10mCombatTestAction::Invulnerable)
	{
		if (ProtectedPawn.IsValid()) { RestoreDamageFlag(); SetStatus(TEXT("테스트 무적 해제 · 실제 피해를 받습니다.")); return true; }
		if (!Player || Player->IsDead()) { SetStatus(TEXT("살아 있는 플레이어가 필요합니다.")); return false; }
		ProtectedPawn = Player; bSavedCanBeDamaged = Player->CanBeDamaged(); Player->SetCanBeDamaged(false);
		SetStatus(TEXT("테스트 무적 켜짐 · 피해 시험을 하려면 무적을 해제하세요.")); return true;
	}
	auto* Monster = GetTestTarget();
	if (!Monster) { SetStatus(TEXT("먼저 테스트 몬스터를 소환하세요.")); return false; }
	if (Action == ERogue10mCombatTestAction::ToggleAI)
	{
		auto* AI = Cast<ARogue10mMonsterAIController>(Monster->GetController());
		if (!AI || !Player || Player->IsDead()) return false;
		bTargetAI = !bTargetAI;
		if (bTargetAI) { AI->InitializeMonsterAI(); AI->ExecuteBehaviorDecision(); }
		else AI->StopMonsterAI();
		SetStatus(bTargetAI ? TEXT("자동 AI 전투 시작. F6을 열면 일시정지합니다.") : TEXT("테스트 대상 AI 정지."));
		if (bTargetAI) SetPanelOpen(false);
		return true;
	}
	if (Action == ERogue10mCombatTestAction::Enrage)
	{
		auto* Pattern = Monster->GetAttackPatternComponent();
		if (!Pattern || !Pattern->GetProfile() || !Pattern->GetProfile()->bEnableBossPatterns) return false;
		auto* Attributes = Monster->GetRogueAttributeSet();
		Attributes->SetHealth(Attributes->GetMaxHealth() * 0.4f);
		SetStatus(TEXT("보스 HP40% · 강화 상태 적용. 정면 파동 또는 심연 영역을 실행하세요.")); return true;
	}
	if (Action == ERogue10mCombatTestAction::Reset)
	{
		// Reset only the current target, keeping the original species even if the picker changed.
		SetSpatialDebugEnabled(false);
		const int32 PickerSelection = SelectedIndex;
		for (int32 Index = 0; Index < Creatures.Num(); ++Index)
			if (Creatures[Index].MonsterClass.Get() == Monster->GetClass()) { SelectedIndex = Index; break; }
		if (AController* AI = Monster->GetController()) AI->Destroy();
		Monster->Destroy(); Target.Reset();
		const bool bSpawned = SpawnMonster();
		SelectedIndex = PickerSelection;
		return bSpawned;
	}
	return false;
}

void URogue10mCombatTestComponent::SetStatus(const FString& Message)
{
	Status = FText::FromString(Message);
	UE_LOG(LogRogue10m, Log, TEXT("Combat test: %s"), *Message);
	RefreshView();
}

void URogue10mCombatTestComponent::RefreshView()
{
	if (!Widget) return;
	auto* Monster = GetTestTarget();
	const auto* Pattern = Monster ? Monster->GetAttackPatternComponent() : nullptr;
	const bool bBoss = Pattern && Pattern->GetProfile() && Pattern->GetProfile()->bEnableBossPatterns;
	const auto* Attributes = Monster ? Monster->GetRogueAttributeSet() : nullptr;
	FString Summary = Monster && Attributes ? FString::Printf(TEXT("대상: %s\nHP %.0f / %.0f · %s · %s · 테스트 %d / 6"),
		*Monster->GetMonsterDisplayName().ToString(), Attributes->GetHealth(), Attributes->GetMaxHealth(),
		bTargetAI ? TEXT("자동 전투") : TEXT("AI 정지"), Pattern && Pattern->IsEnraged() ? TEXT("강화") : TEXT("일반"), GetTestMonsterCount())
		: TEXT("현재 테스트 대상 없음 · 몬스터를 소환하세요.");
	Widget->RefreshView(bPanelOpen, Status, FText::FromString(Summary), Monster != nullptr, bBoss, ProtectedPawn.IsValid());
}

void URogue10mCombatTestComponent::RestoreDamageFlag()
{
	if (APawn* Pawn = ProtectedPawn.Get()) Pawn->SetCanBeDamaged(bSavedCanBeDamaged);
	ProtectedPawn.Reset();
}

void URogue10mCombatTestComponent::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	RestoreDamageFlag();
	ClearMonsters();
	SetPanelOpen(false);
	SetStatus(TEXT("캐릭터 변경으로 테스트 대상과 무적을 초기화했습니다."));
}

void URogue10mCombatTestComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    SetSpatialDebugEnabled(false);
    if (auto* Run=GetWorld()?GetWorld()->GetGameState<ARogue10mGameState>():nullptr)
    { Run->OnRunPhaseChanged.RemoveDynamic(this,&ThisClass::HandleSpatialRunPhaseChanged); }
	if (auto* PC = Cast<ARogue10mPlayerController>(GetOwner())) PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	RestoreDamageFlag();
	if (bOwnsPause) if (auto* PC = Cast<ARogue10mPlayerController>(GetOwner())) PC->SetPause(false);
	bOwnsPause = false; bPanelOpen = false;
	ClearMonsters();
	if (Widget) Widget->RemoveFromParent();
	Widget = nullptr;
	Super::EndPlay(Reason);
}
