// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Rogue10mHUD.generated.h"

/**
 * 보관된 구형 HUD Blueprint를 열 수 있게 유지하는 빈 호환 클래스입니다.
 * 실제 화면과 UI 상태는 PlayerController와 UMG Widget이 관리합니다.
 */
UCLASS()
class ROGUE10M_API ARogue10mHUD : public AHUD
{
	GENERATED_BODY()
};
