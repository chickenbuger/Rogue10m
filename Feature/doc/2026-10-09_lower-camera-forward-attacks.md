# 1인칭 카메라 하향과 플레이어 공격 방향 점검

카메라를 약7cm 낮췄다. 시야각·마우스 조준과 V키 3인칭 검사 시점은 유지했다. 현재 맨손·너클 프로필에 연결된 공격의 원본 자세와 실제 플레이를 점검했으며, 전방 타격의 방향 오류는 발견되지 않았다.

## 구현

- 브랜치: `Sprint#5-21-lower-camera-forward-attacks`. 커밋·푸시 없음.
- `Rogue10mAppearanceCameraComponent.h`의 기본 `EyeOffset=(8,0,4) → (8,0,-3)cm`만 생산 설정에서 변경했다. 머리 위치 추적·회전 안정화·초기 pitch=-20도·HUD·몽타주·피해 시점은 유지한다.
- 카메라와 공격 원점이 같은 계산을 사용하므로 실제 판정도 낮춘 시점에서 실행된다. 오프셋은 안정화된 머리 방향을 따르므로 월드 수직 변위는 자세에 따라 약7cm다.
- 여섯 종족·성별 Blueprint의 새 기본값을 fresh Editor에서 확인했다. Blueprint 에셋 수정은 필요하지 않았다.
- 읽기 전용 공격 감사 스크립트와 Editor 전용 실제 전투 fixture, 실행 wrapper를 추가했다. 훅·어퍼컷·반격·점프·원형 공격의 의도를 구분하며 실제 타격손으로 측정한다.

## 점검 범위와 결과

| 검증 | 결과 |
| --- | --- |
| 전체 공격/프로필 인벤토리 | 74개 AttackSkill·13개 SkillProfile의 참조·몽타주 타이밍 조사 |
| 현재 Unarmed/Knuckle의 연결 공격 | 콤보까지24개 참조, 패시브1개 제외23개 공격·중복 제거16개 시퀀스120Hz 측정, 방향 검토 항목0 |
| 실제 StartHub 전투 | 기본9종+현재 트리 파생4종=13종, yaw90도 변화2건 추가15건. runtime1,572프레임·최종4,978검사·실패0 |
| 실제 1인칭 렌더 | 같은15건·1,572프레임·최종4,979검사·실패0, idle2장+접촉15장 저장. 에셋 컴파일 준비 잔여0 확인 |
| 동일 자세 카메라 비교 | 오프셋4/-3 비교 변위 길이7.000000cm, 독립 위치 오차0, 회전·머리 자세 유지 |
| 실제 피해 | 체력 감소21펄스. 전방 타격과 너클 연타6펄스 유지, 첫 피해 시점 오차 최대0.02917초 |
| 원형 점프 내려찍기 | 실제 점프 후 앞1회·뒤1회 피해. 원형 범위 계약과 일치하며 후방 오작동으로 분류하지 않음 |
| 기존 회귀 | AppearanceCamera570·InitialHeadFraming90·ThirdPersonInspection280프레임 실패0. MovingAttack1,100프레임·최종258,066검사 실패0 |
| 콘텐츠 보존 | Content6,366개 전부 SHA256 동일, 추가·삭제·변경0. 공격 데이터/프로필/몽타주 등 집중207패키지도 동일 |

기본9종의 실제 리타게팅 타격손은 어깨 기준 전방45.14~56.70cm에 도달했다. 왼손 반격 `FlowingCloud_LeftCounter`는 실제 왼손 기준 전방50.48cm·idle 대비43.55cm 확장·측면각8.25도다. 초기 fixture가 해당 파생 기술의 오른쪽 가드손을 요약하던 문제는 검증 도구에서 수정했고, 실제 애니메이션은 수정하지 않았다.

Manny의 원본 전방은 skeletal +Y다. 런타임에서는 원시 로컬 뼈와 모든 부모, 실제 Mesh의 component transform을 합성해 actor/camera 좌표로 변환했다. 머리 숨김 렌더 스케일과 3인칭 검사 POV가 공격 방향의 근거가 되지 않도록 별도로 처리한다. 실제 피해 판정은 기존 카메라 원점/방향을 사용한다.

## Builder / Reviewer / Exit Gate

초기 신규 fixture의 protected 점프 함수 접근 오류를 public `Jump/StopJumping`으로 해결했다. 생산 코드의 접근 지정자는 바꾸지 않았다. 이후 최종 Editor 빌드가 성공했고, 검증 손 선택·렌더 준비 보완 이후에도 runtime/render를 다시 통과했다.

독립 리뷰는 동일 자세 카메라 oracle, 실제 타격손, 원형 피해와 타깃 수 계약, 원본 데이터 보존을 확인했다. 현재 데이터의 넉넉한 MaxTargets와 Single/Continuous 계약을 명시해 향후 다른 데이터가 들어오면 검증이 조용히 잘못 통과하지 않게 한다. 접촉 좌표는 프레임 단위 샘플이며 타격 창의 수치와 실제 체력 변화, 시각 자료를 함께 해석한다.

StartHub를 최종 빌드로 다시 열었다. 여섯 Blueprint 기본값 `(8,0,-3)`, pitch=-20도, PIE 종료, 미저장 콘텐츠/맵0, 120FPS를 확인했다. 기존 아린·월드·HUD·이동 상체 공격과 이전 미커밋 작업을 보존했다.

## 자료와 재현

- [설계](../architect/2026-10-09_lower-camera-forward-attacks.md)
- [읽기 전용 원본 방향 감사](evidence/camera-forward-attacks-20261009/audit.json)
- [최종 전투 수치](evidence/camera-forward-attacks-20261009/final-runtime/metrics.txt)
- [실제 카메라 비교](evidence/camera-forward-attacks-20261009/final-runtime/camera-height.csv)
- [실제 공격별 결과](evidence/camera-forward-attacks-20261009/final-runtime/results.csv)
- [낮춘 1인칭 화면](evidence/camera-forward-attacks-20261009/visual/idle_current_eye_minus3.png)
- [전방 스트레이트 화면](evidence/camera-forward-attacks-20261009/visual/contact_02_DA_BasicBrawler_RightStraight.png)
- [최종 에디터 상태](evidence/camera-forward-attacks-20261009/final-editor-state.json)
- [콘텐츠 보존 검사](evidence/camera-forward-attacks-20261009/content-after.json)
- [독립 리뷰](evidence/camera-forward-attacks-20261009/review.json)

```powershell
.\Scripts\BuildEditor.ps1 -NoHotReload
.\Scripts\VerifyCameraForwardAttacks.ps1 -Phase runtime -Label fresh-camera-runtime
.\Scripts\VerifyCameraForwardAttacks.ps1 -Phase render -Label fresh-camera-render
```

다른 무기 프로필은 인벤토리·참조·타이밍을 조사했으며 이번 실제 손 방향/전투 검증은 현재 맨손·너클·무투가 경로다. QA 몬스터는 판정용 임시 대상이며 저장되지 않는다. 화면의 기본 회색 팔 재질과 일부 빠른 기술의 모션 블러는 기존 표현이고 이번 작업에서 변경하지 않았다.
