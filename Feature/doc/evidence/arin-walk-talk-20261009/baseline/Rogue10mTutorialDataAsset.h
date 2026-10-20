// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rogue10mTutorialDataAsset.generated.h"

class UAnimSequence;
class USkeletalMesh;
class URogue10mItemDataAsset;

/** 안내 문구와 외형만 설정하며 진행 판정은 NPC의 C++에서 처리합니다. */
UCLASS(BlueprintType)
class ROGUE10M_API URogue10mTutorialDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	FText GuideName = NSLOCTEXT("Rogue10mTutorial", "GuideName", "길잡이 아린");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	FText GuideRole = NSLOCTEXT("Rogue10mTutorial", "GuideRole", "모험가 안내 · 출발 보급");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	FText WelcomeText = NSLOCTEXT("Rogue10mTutorial", "Welcome", "어서 오세요. 마을에서는 천천히 준비해도 괜찮아요. 북쪽 포탈을 지나면 20분 탐험이 시작됩니다. 출발 전에 회복 물약을 챙겨 드릴게요.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	FText RewardReceivedText = NSLOCTEXT("Rogue10mTutorial", "RewardReceived", "보급품을 가방에 넣어 드렸어요. I를 눌러 인벤토리에서 확인하세요. 준비되면 북쪽 포탈로 출발해 보세요.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	FText RewardAlreadyReceivedText = NSLOCTEXT("Rogue10mTutorial", "RewardAlreadyReceived", "이번 출발 보급품은 이미 받으셨어요. 장비는 I, 기술은 K로 확인할 수 있어요. 북쪽 포탈을 지나면 탐험이 시작됩니다.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	FName RewardId = TEXT("StartVillageSupplies");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial")
	TObjectPtr<URogue10mItemDataAsset> StartingItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial", meta=(ClampMin="1"))
	int32 StartingItemQuantity = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial|Appearance")
	TObjectPtr<USkeletalMesh> GuideSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rogue10m|Tutorial|Appearance")
	TObjectPtr<UAnimSequence> GuideIdleAnimation;
};
