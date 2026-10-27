#pragma once

#include "CoreMinimal.h"
#include "Rogue10mProgressionRequirementTypes.generated.h"

UENUM(BlueprintType)
enum class ERogue10mProgressionRequirementKind : uint8 { MasteryAtLeast = 0, DisciplineRankAtLeast = 1 };

USTRUCT(BlueprintType)
struct FRogue10mProgressionRequirement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Requirements") ERogue10mProgressionRequirementKind Kind = ERogue10mProgressionRequirementKind::MasteryAtLeast;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Requirements") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Requirements", meta=(ClampMin="0")) int32 Minimum = 0;
	bool IsValid() const;
};

/** Explicit evaluation input only; not a grant API or a run/lobby lifecycle policy. */
USTRUCT(BlueprintType)
struct FRogue10mProgressionRequirementContext
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Requirements") int32 GreatSwordLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Requirements") int32 BrawlerLevel = 1;
	/** Identity is not a rank source. Unwired live input is zero; progression lifetime is a separate policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Requirements") int32 MageRank = 0;
	bool IsValid() const { return GreatSwordLevel >= 0 && BrawlerLevel >= 0 && MageRank >= 0; }
	int32 GetValue(const FRogue10mProgressionRequirement& Requirement) const;
};

USTRUCT(BlueprintType)
struct FRogue10mProgressionRequirementEvaluation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") bool bSatisfied = false;
	UPROPERTY(BlueprintReadOnly, Category="Rogue10m|Requirements") TArray<FRogue10mProgressionRequirement> MissingRequirements;
};

struct ROGUE10M_API FRogue10mProgressionRequirements
{
	static bool IsValid(const TArray<FRogue10mProgressionRequirement>& RequiredAll);
	static FRogue10mProgressionRequirementEvaluation Evaluate(const TArray<FRogue10mProgressionRequirement>& RequiredAll, const FRogue10mProgressionRequirementContext& Context);
};
