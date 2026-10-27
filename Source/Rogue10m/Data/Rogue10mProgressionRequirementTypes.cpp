#include "Rogue10mProgressionRequirementTypes.h"

bool FRogue10mProgressionRequirement::IsValid() const
{
	if (Minimum < 0) { return false; }
	if (Kind == ERogue10mProgressionRequirementKind::MasteryAtLeast) { return Id == TEXT("GreatSword") || Id == TEXT("Brawler"); }
	return Kind == ERogue10mProgressionRequirementKind::DisciplineRankAtLeast && Id == TEXT("Discipline.Mage");
}

int32 FRogue10mProgressionRequirementContext::GetValue(const FRogue10mProgressionRequirement& Requirement) const
{
	if (!Requirement.IsValid()) { return -1; }
	if (Requirement.Kind == ERogue10mProgressionRequirementKind::DisciplineRankAtLeast) { return MageRank; }
	return Requirement.Id == TEXT("GreatSword") ? GreatSwordLevel : BrawlerLevel;
}

bool FRogue10mProgressionRequirements::IsValid(const TArray<FRogue10mProgressionRequirement>& RequiredAll)
{
	TSet<FString> Seen;
	for (const auto& Requirement : RequiredAll)
	{
		if (!Requirement.IsValid()) { return false; }
		const FString Key = FString::Printf(TEXT("%d:%s"), static_cast<int32>(Requirement.Kind), *Requirement.Id.ToString().ToLower());
		if (Seen.Contains(Key)) { return false; }
		Seen.Add(Key);
	}
	return true;
}

FRogue10mProgressionRequirementEvaluation FRogue10mProgressionRequirements::Evaluate(
	const TArray<FRogue10mProgressionRequirement>& RequiredAll, const FRogue10mProgressionRequirementContext& Context)
{
	FRogue10mProgressionRequirementEvaluation Result;
	Result.bValid = IsValid(RequiredAll) && Context.IsValid();
	if (!Result.bValid) { return Result; }
	for (const auto& Requirement : RequiredAll)
	{
		if (Context.GetValue(Requirement) < Requirement.Minimum) { Result.MissingRequirements.Add(Requirement); }
	}
	Result.bSatisfied = Result.MissingRequirements.IsEmpty();
	return Result;
}
