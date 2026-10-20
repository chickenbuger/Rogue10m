#include "Rogue10mTraitCatalogDataAsset.h"

bool URogue10mTraitCatalogDataAsset::IsSupportedTrait(ERogue10mTownTrait Trait)
{
	return static_cast<uint8>(Trait) < static_cast<uint8>(ERogue10mTownTrait::Count);
}

int32 URogue10mTraitCatalogDataAsset::GetMaximumSupportedRank(ERogue10mTownTrait Trait)
{
	if (!IsSupportedTrait(Trait)) { return 0; }
	if (Trait == ERogue10mTownTrait::Vitality || Trait == ERogue10mTownTrait::Power) { return 5; }
	return Trait == ERogue10mTownTrait::LastStand ? 1 : 3;
}

float URogue10mTraitCatalogDataAsset::GetMaximumBonusPerRank(ERogue10mTownTrait Trait)
{
	switch (Trait)
	{
	case ERogue10mTownTrait::Vitality:
	case ERogue10mTownTrait::Power:
	case ERogue10mTownTrait::Defense:
	case ERogue10mTownTrait::KillRecovery:
	case ERogue10mTownTrait::MaxStamina: return 200000.0f;
	case ERogue10mTownTrait::CriticalChance:
	case ERogue10mTownTrait::LastStand: return 1.0f;
	case ERogue10mTownTrait::DodgeEfficiency: return .30f;
	case ERogue10mTownTrait::GoldFind: return .05f;
	case ERogue10mTownTrait::MoveSpeed: return 3.0f;
	case ERogue10mTownTrait::StaminaRecovery: return 10000.0f;
	default: return IsSupportedTrait(Trait) ? 10.0f : 0.0f;
	}
}

bool FRogue10mTraitDefinition::IsTuningValid() const
{
	if (!URogue10mTraitCatalogDataAsset::IsSupportedTrait(Trait)
		|| static_cast<uint8>(Category) > static_cast<uint8>(ERogue10mTraitCategory::Exploration)
		|| DisplayName.IsEmpty() || MaxRank < 1
		|| MaxRank > URogue10mTraitCatalogDataAsset::GetMaximumSupportedRank(Trait)
		|| PointCost < 1 || PointCost > 1000 || UnlockLevel < 1 || UnlockLevel > 1000000
		|| !FMath::IsFinite(ValuePerRank) || ValuePerRank < 0.0f
		|| ValuePerRank > URogue10mTraitCatalogDataAsset::GetMaximumBonusPerRank(Trait)
		|| !FMath::IsFinite(DurationSeconds) || DurationSeconds < 0.0f || DurationSeconds > 60.0f
		|| !FMath::IsFinite(CooldownSeconds) || CooldownSeconds < 0.0f || CooldownSeconds > 300.0f
		|| RequiredHits < 1 || RequiredHits > 20) { return false; }
	if ((Trait == ERogue10mTownTrait::Vitality || Trait == ERogue10mTownTrait::Power) && PointCost != 1) { return false; }
	if (Trait == ERogue10mTownTrait::LastStand && (PointCost < 3 || UnlockLevel < 20 || ValuePerRank <= 0.0f)) { return false; }
	if ((Trait == ERogue10mTownTrait::Combo || Trait == ERogue10mTownTrait::DodgeCounter) && DurationSeconds <= 0.0f) { return false; }
	return true;
}

URogue10mTraitCatalogDataAsset::URogue10mTraitCatalogDataAsset()
{
	const auto Add = [this](ERogue10mTownTrait Id, ERogue10mTraitCategory Category, const TCHAR* Name,
		const TCHAR* Summary, const TCHAR* Description, float Value, int32 Ranks = 3,
		int32 Cost = 1, int32 Unlock = 1, float Duration = 0.0f, float Cooldown = 0.0f, int32 Hits = 1)
	{
		FRogue10mTraitDefinition& Def = Definitions.AddDefaulted_GetRef();
		Def.Trait = Id; Def.Category = Category; Def.DisplayName = FText::FromString(Name);
		Def.Description = FText::FromString(Description); Def.EffectSummary = FText::FromString(Summary);
		Def.ValuePerRank = Value; Def.MaxRank = Ranks; Def.PointCost = Cost; Def.UnlockLevel = Unlock;
		Def.DurationSeconds = Duration; Def.CooldownSeconds = Cooldown; Def.RequiredHits = Hits;
	};
	Add(ERogue10mTownTrait::Power, ERogue10mTraitCategory::Combat, TEXT("날카로운 일격"), TEXT("공격력 +2 / 단계"), TEXT("공격력이 증가합니다."), 2.0f, 5);
	Add(ERogue10mTownTrait::CriticalChance, ERogue10mTraitCategory::Combat, TEXT("약점 간파"), TEXT("치명 확률 +2%p / 단계"), TEXT("치명타 확률이 증가합니다. 최종 확률은 100%를 넘지 않습니다."), .02f);
	Add(ERogue10mTownTrait::CriticalDamage, ERogue10mTraitCategory::Combat, TEXT("정밀 타격"), TEXT("치명 피해 +0.10배 / 단계"), TEXT("치명타 피해 배율이 증가합니다."), .10f);
	Add(ERogue10mTownTrait::Combo, ERogue10mTraitCategory::Combat, TEXT("연격의 달인"), TEXT("연격 피해 +4% / 단계"), TEXT("같은 적에게 3초 안에 공격 3회 적중하면 3초 동안 피해가 증가합니다. 다단 공격은 1회로 세며 빗나감·대상 변경 시 초기화됩니다."), .04f, 3, 1, 1, 3.0f, 0.0f, 3);
	Add(ERogue10mTownTrait::Vitality, ERogue10mTraitCategory::Survival, TEXT("강인한 육체"), TEXT("최대 체력 +10 / 단계"), TEXT("최대 체력이 증가합니다. 강화로 현재 체력을 충전하지 않습니다."), 10.0f, 5);
	Add(ERogue10mTownTrait::Defense, ERogue10mTraitCategory::Survival, TEXT("철벽 방어"), TEXT("방어력 +2 / 단계"), TEXT("방어력이 증가합니다. 고정 피해 감산이며 최소 피해는 1입니다."), 2.0f);
	Add(ERogue10mTownTrait::KillRecovery, ERogue10mTraitCategory::Survival, TEXT("사냥꾼의 회복"), TEXT("처치 회복 +2 / 단계"), TEXT("직접 처치하면 체력을 회복합니다. 공통 재사용 시간 2초, 최대 체력을 넘지 않습니다."), 2.0f, 3, 1, 1, 0.0f, 2.0f);
	Add(ERogue10mTownTrait::LastStand, ERogue10mTraitCategory::Survival, TEXT("불굴"), TEXT("치명상 생존 · 탐험당 1회"), TEXT("치명적인 전투 피해를 받을 때 체력 25%로 생존합니다. 탐험당 1회. 시간 종료·강제 종료에는 적용되지 않습니다."), .25f, 1, 3, 20);
	Add(ERogue10mTownTrait::MoveSpeed, ERogue10mTraitCategory::Mobility, TEXT("기민한 발걸음"), TEXT("이동 속도 +3% / 단계"), TEXT("걷기와 질주 속도가 증가합니다."), .03f);
	Add(ERogue10mTownTrait::MaxStamina, ERogue10mTraitCategory::Mobility, TEXT("깊은 호흡"), TEXT("최대 스태미나 +10 / 단계"), TEXT("최대 스태미나가 증가합니다. 강화로 현재 스태미나를 충전하지 않습니다."), 10.0f);
	Add(ERogue10mTownTrait::DodgeEfficiency, ERogue10mTraitCategory::Mobility, TEXT("절약의 보법"), TEXT("회피 소모 −5% / 단계"), TEXT("회피에 필요한 스태미나가 감소합니다."), .05f);
	Add(ERogue10mTownTrait::DodgeCounter, ERogue10mTraitCategory::Mobility, TEXT("회피 반격"), TEXT("회피 후 첫 공격 +6% / 단계"), TEXT("회피 후 2초 안에 시작한 첫 공격의 피해가 증가합니다. 재사용 8초. 중첩 불가, 다단 공격은 공격 실행 1회로 처리합니다."), .06f, 3, 1, 1, 2.0f, 8.0f);
	Add(ERogue10mTownTrait::GoldFind, ERogue10mTraitCategory::Exploration, TEXT("전리품 감각"), TEXT("금화 보상 +5% / 단계"), TEXT("몬스터의 기본 금화 보상이 증가합니다. 경험치는 바뀌지 않습니다. 소수 금화는 현재 탐험에서 누적해 지급합니다."), .05f);
	Add(ERogue10mTownTrait::StaminaRecovery, ERogue10mTraitCategory::Exploration, TEXT("빠른 재충전"), TEXT("스태미나 재생 +1/초 / 단계"), TEXT("살아 있는 동안 스태미나 초당 회복량이 증가합니다."), 1.0f);
	Add(ERogue10mTownTrait::PotionMastery, ERogue10mTraitCategory::Exploration, TEXT("치유의 숙련"), TEXT("물약 회복량 +5% / 단계"), TEXT("물약의 체력 회복량이 증가합니다. 처치 회복·자연 재생·불굴에는 적용되지 않습니다."), .05f);
	Add(ERogue10mTownTrait::BossHunter, ERogue10mTraitCategory::Exploration, TEXT("보스 사냥꾼"), TEXT("보스 피해 +4% / 단계"), TEXT("중간 보스와 최종 보스에 가하는 피해가 증가합니다."), .04f);
}

FRogue10mTraitDefinition URogue10mTraitCatalogDataAsset::GetTraitDefinition(ERogue10mTownTrait Trait) const
{
	const auto* Found = Definitions.FindByPredicate([Trait](const auto& Def) { return Def.Trait == Trait; });
	return Found ? *Found : FRogue10mTraitDefinition();
}

bool URogue10mTraitCatalogDataAsset::IsTuningValid() const
{
	if (Definitions.Num() != static_cast<int32>(ERogue10mTownTrait::Count)) { return false; }
	TSet<ERogue10mTownTrait> Seen;
	for (const auto& Def : Definitions)
	{
		if (!Def.IsTuningValid() || Seen.Contains(Def.Trait)) { return false; }
		Seen.Add(Def.Trait);
	}
	return true;
}
