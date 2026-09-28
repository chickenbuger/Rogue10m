# 오버워치풍 1인칭 전투 VFX 구현 결과

## 결과 요약

오버워치 계열의 짧고 선명한 영웅 슈터 피드백을 기준으로 기존 10종 전투 Niagara를 재조정했다. 화면 중앙을 비우고 손·화면 가장자리에서 공격 역할과 무게가 읽히도록 스케일, 수명, 속도, 오프셋을 데이터화했다. 원본 Marketplace 효과는 수정하지 않았으며 기존 프로젝트 파생 Niagara와 Data Asset만 Unreal Editor를 통해 저장했다.

| 항목 | 결과 |
| --- | --- |
| 전투 스타일 | 단검, 표창, 쌍단검, 장검, 대검, 쌍검, 방패, 한손검·작은방패, 마법사, 권사 10종 |
| 핵심 공격 데이터 | 50개, 스타일별 Primary 3연계·Special·Charged |
| 실제 권사 진행 스킬 | Jab, Straight, ChargedShockwave, JumpSlam 4개에 권사 Montage·Niagara 연결 |
| Niagara | Cast·Charge·Impact 30개, 공통 `NET_CombatReadable` Effect Type 배정 |
| 실제 게임 QA | 10종, 연계 27회, 차징 9회, 권사 기본 권법 1회, 캡처 10장, 실패 0건 |

## 구현 내용

- `HeroShooterFP_v1` 가독성 프로필을 추가해 무기 무게와 콤보 단계별 스케일·지속시간·재생 속도를 한곳에서 관리한다.
- Cast는 손 근처의 짧은 시동 신호, Charge는 작은 지속 핵, Impact는 적 위치의 가장 강한 신호로 역할을 분리했다.
- 양손 공격의 1인칭 보조 손 효과에 최대 0.70 배율을 추가해 중첩 밝기와 화면 점유를 낮췄다.
- 첫 번째·두 번째·세 번째 공격과 Special·Charged의 강도 계층을 유지하되 모든 값은 공통 1인칭 상한 안에 둔다.
- 30개 Niagara System에 UE 5.8 `NiagaraEffectType`을 공통 배정하고 로컬 플레이어 효과는 거리 컬링으로 사라지지 않도록 설정했다.
- 실제 권사 진행 스킬 4개는 피해·타격 수·해금 조건을 유지한 채 기존 권사 Montage와 Cast·Charge·Impact만 연결했다.
- 에디터 첫 Niagara 컴파일 지연을 런타임 테스트 워밍업으로 분리해 실제 플레이 프레임의 출력만 캡처한다.

## 1인칭 가독성 예산

| 구분 | 최종 최대값 | 상한 |
| --- | ---: | ---: |
| Cast 실제 스케일 | 0.058080 | 0.060 |
| Charge 실제 스케일 | 0.024000 | 0.026 |
| Impact 실제 스케일 | 0.178200 | 0.180 |
| Cast 실제 방출 시간 | 0.057330초 | 0.060초 |
| Impact 실제 방출 시간 | 0.065520초 | 0.070초 |
| 양손 보조 효과 | 0.70배 | 0.70배 |

손 부착 효과는 로컬 소켓 기준 좌우 14 이상, 아래 11 이상으로 이동했다. 화면 전체 왜곡·불투명 원판·지속 연기는 추가하지 않았다.

## 검증

| 검증 | 결과 |
| --- | --- |
| UE 5.8 Editor 빌드 | `Result: Succeeded` |
| 핵심 자산 Validator | `RESULT=PASSED profiles=10 attacks=50 montages=50 niagara=30 first_person_safe=50` |
| 권사 진행 스킬 Validator | `RESULT=MARTIAL_ART_READABLE_VFX_PASSED skills=4 montages=4 first_person_safe=4` |
| 실제 게임 런타임 | `RESULT=CLASS_COMBAT_RUNTIME_PASSED styles=10 combos=27 charged=9 screenshots=10 failures=0` |
| Python | 적용·검증 스크립트 6개 UE 실행 및 구문 통과 |
| Harness | `CheckGeneratedChanges.ps1`: Harness path check 통과 |

10장 캡처를 육안 검토한 결과 쌍수·대검·마법·권사를 포함한 모든 스타일에서 중앙 표적 영역이 유지됐다. 큰 마법·방패 효과도 우측 또는 하단 주변부에 머물렀고, 효과 컴포넌트 누적은 발견되지 않았다.

검증 중 `/Game/AdvancedPortalsSystemVFX/Meshes/SM_Plane`의 기존 Convex Collision 경고 3건이 반복됐으나 이번 전투 VFX와 무관하며 오류는 0건이었다.

## 참고 기준

- Blizzard의 Overwatch 1인칭 제작 사례: 화면 공간, 역할 구분, 플레이 테스트 기반 전투 명료성
- Riot VALORANT 무기 VFX 원칙: 효과가 조준점과 기본 발사 실루엣을 방해하지 않도록 제한
- Epic UE 5.8 Niagara Scalability·Effect Type 지침: 공통 Effect Type, 인스턴스와 컬링 예산 관리

## 관련 파일

- `Feature/architect/2026-08-26_overwatch-inspired-combat-vfx.md`
- `Feature/architect/2026-08-26_overwatch-inspired-combat-vfx-amendment.md`
- `Scripts/Editor/ClassCombatVFXReadabilityProfile.py`
- `Scripts/Editor/ApplyReadableClassCombatVFX.py`
- `Scripts/Editor/ApplyMartialArtsReadableVFX.py`
- `Scripts/Editor/ValidateClassCombatAnimationVFX.py`
- `Scripts/Editor/ValidateMartialArtsReadableVFX.py`
- `Source/Rogue10m/Tests/Rogue10mClassCombatPresentationRuntimeTest.cpp`
