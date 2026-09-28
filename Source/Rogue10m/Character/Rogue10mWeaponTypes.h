// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rogue10mWeaponTypes.generated.h"

// 캐릭터와 인벤토리가 함께 사용하는 무기 타입 기준입니다.
// 숙련도, 스킬, 장착 아이템은 이 enum을 기준으로 확장합니다.
UENUM(BlueprintType)
enum class ERogue10mWeaponType : uint8
{
	// 아무 무기도 장착하지 않은 기본 주먹 상태입니다.
	Unarmed,

	// 빠른 근접 공격용 단검입니다.
	Dagger,

	// 느리지만 강한 근접 공격용 대검입니다.
	GreatSword,

	// 다단 히트와 빠른 연계를 위한 쌍검입니다.
	DualBlades,

	// 거리 유지와 차지 공격을 위한 활입니다.
	Bow,

	// 마법/기력 계열 스킬을 연결할 지팡이입니다.
	Staff,

	// 주먹 기반 전투를 강화하는 권 계열입니다.
	Knuckle,

	// 손목 투척과 원거리 연계를 사용하는 표창입니다.
	Shuriken,

	// 쌍검보다 짧고 빠른 교차 연계를 위한 쌍단검입니다.
	DualDaggers,

	// 정석 베기와 찌르기 연계를 사용하는 장검입니다.
	LongSword,

	// 방패 밀치기와 방어 반격을 위한 대형 방패입니다.
	Shield,

	// 한손검과 작은 방패를 함께 사용하는 균형형 장비입니다.
	SwordBuckler
};
