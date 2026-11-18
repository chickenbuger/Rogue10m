#pragma once

#include "CoreMinimal.h"
#include "Rogue10mVillageQuestPresentation.generated.h"

/** Editable presentation for the optional transport-record story. No gameplay or reward state. */
USTRUCT(BlueprintType)
struct ROGUE10M_API FRogue10mVillageOptionalStoryText
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FText AnalyzeAction = NSLOCTEXT("Rogue10mVillageQuest", "AnalyzeClue", "선택 이야기 · 운송 기록 분석하기");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FText ReportAction = NSLOCTEXT("Rogue10mVillageQuest", "ReportClue", "선택 이야기 · 분석 결과 전하기");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FText AnalysisSuccess = NSLOCTEXT("Rogue10mVillageQuest", "ClueAnalyzed", "운송 기록에 비를 맞은 흔적이 있습니다. 낙엽촌에 보급품의 방수 포장을 부탁해 주세요.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FText ReportSuccess = NSLOCTEXT("Rogue10mVillageQuest", "ClueReported", "분석 결과를 전달했습니다. 보급지기가 다음 운송부터 방수 포장을 준비하기로 했습니다.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FText AnalystMissingHint = NSLOCTEXT("Rogue10mVillageQuest", "AnalystMissingHint", "선택 이야기 · 낙엽촌 기록이 없어도 이곳의 의뢰는 끝낼 수 있습니다.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FText AnalystAnalyzedHint = NSLOCTEXT("Rogue10mVillageQuest", "AnalystAnalyzedHint", "분석한 운송 기록을 낙엽촌 보급지기에게 전할 수 있습니다.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FText SourceFoundHint = NSLOCTEXT("Rogue10mVillageQuest", "SourceFoundHint", "선택 이야기 · 운송 기록을 기록촌에서 분석할 수 있습니다. 의뢰 보상과는 별개입니다.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Village Quests|Optional Story")
	FText SourceReportedHint = NSLOCTEXT("Rogue10mVillageQuest", "SourceReportedHint", "기록촌의 분석 결과까지 전달했습니다. 운송 기록 이야기를 마쳤습니다.");

	bool IsValid() const
	{
		return !AnalyzeAction.IsEmpty() && !ReportAction.IsEmpty()
			&& !AnalysisSuccess.IsEmpty() && !ReportSuccess.IsEmpty()
			&& !AnalystMissingHint.IsEmpty() && !AnalystAnalyzedHint.IsEmpty()
			&& !SourceFoundHint.IsEmpty() && !SourceReportedHint.IsEmpty();
	}
};
