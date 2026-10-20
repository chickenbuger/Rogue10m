# 아린 Walk / Talk 사용 안내

기존 RefinedV3 아린 모델을 유지하는 **기본 걷기와 대화 몸짓** 패키지다. 두 동작은 기존 23개 제작 본에 자체 키프레임으로 제작했으며, Unreal의 기존 `SK_Arin_Skeleton`을 사용한다. 시작마을 DataAsset에 두 애니메이션 참조를 저장했고 **제작·적용·통합 검증을 완료했다**. 최종 StartHub PIE의 159개 확인 항목이 모두 통과했다.

## 파일

| 파일 | 용도 |
|---|---|
| [A_Arin_Walk.fbx](D:/Project/Rogue10m/Art/NPC/Arin/Animation/A_Arin_Walk.fbx) | 애니메이션 전용 FBX, 30fps·1.2초 in-place 걷기 |
| [A_Arin_Talk.fbx](D:/Project/Rogue10m/Art/NPC/Arin/Animation/A_Arin_Talk.fbx) | 애니메이션 전용 FBX, 30fps·3초 대화 몸짓 |
| [Arin_Motions.blend](D:/Project/Rogue10m/Art/NPC/Arin/Animation/Arin_Motions.blend) | 기존 모델·재질과 Idle/Walk/Talk action을 포함하는 Blender 편집 원본 |

기존 메시를 다시 가져오거나 골격·가중치를 교체할 필요가 없다. `Arin_Motions.blend1`은 Blender의 저장 백업이며 전달용 최종 파일은 위 `.blend`다.

## 현재 Unreal 적용 위치

```text
Walk: /Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Walk
Talk: /Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Talk
Skeleton: /Game/Rogue10m/Characters/Arin/RefinedV3/SK_Arin_Skeleton
Guide Data: /Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide
Map: /Game/Rogue10m/Maps/StartHub
```

다른 프로젝트에 가져올 때는 위 RefinedV3 모델의 **기존 Skeleton을 선택하고 Import Animations를 켜며, Import Mesh와 골격 reference pose 갱신은 끈다**. 애니메이션 파일만으로 생성한 다른 골격을 기존 모델에 덮어쓰지 않는다. Root Motion은 비활성화한다. 파일의 take 이름은 둘 다 `Arin_Rig|Scene`일 수 있으므로 파일명에 맞춰 Walk와 Talk를 구분한다.

저장된 Unreal Walk는 36프레임·약 1.20000005초, Talk는 90프레임·3초이고 둘 다 30/1 sample rate다. Blender에는 첫 자세를 반복하는 종점 키가 있어 각각 1..37, 1..91 범위로 보인다.

## 게임 동작과 설정

C++ `Rogue10mTutorialAnimationComponent`가 **대화 요청 > 실제 이동 > Idle** 순서로 재생한다. 0.1초 타이머로 actor의 XY 속도를 읽고 기본값에서 10cm/s 초과로 Walk를 시작, 5cm/s 이하로 정지한다. Walk 재생률은 실제 속도 / 기준 속도 110cm/s이며 0.01..10으로 제한된다. 같은 클립을 계속 처음부터 다시 재생하지 않는다.

F로 대화를 열면 Talk를 재생하고 닫거나 범위를 벗어나면 이동 상태에 맞춰 Walk/Idle로 돌아간다. 잘못된 골격이나 누락된 클립은 기존 Idle로 돌아간다. NPC 위치를 유지하며 자동 순찰이나 AI 경로 이동은 추가하지 않았다. DataAsset의 `GuideWalkReferenceSpeed`, `GuideMinimumWalkSpeed`로 속도 기준을 조절할 수 있다.

F 수동 확인 순서는 `StartHub` Play → 320cm 안으로 접근 → F로 대화/Talk 시작 → 미수령 시 F로 물약 2개 수령 → 다음 F로 닫기/Idle 복귀다. 이미 수령한 경우 두 번째 F가 닫는다. 대화 중 범위를 벗어나 Talk가 해제되는지도 확인한다. 이 절차는 QA 안내이며 실제 물리 F 입력을 이번 작업에서 완료했다는 주장은 아니다.

## 출처와 검증

공식 Mixamo의 [Walking / Feminine Walk Forward](https://www.mixamo.com/#/?page=1&query=walking&type=Motion%2CMotionPack)와 [Talking / General Conversation](https://www.mixamo.com/#/?page=1&query=talking&type=Motion%2CMotionPack) 공개 프리뷰를 동작 참고로 관찰했다. 로그인 제약으로 다운로드한 Mixamo FBX는 **0개**이며, 적용 파일은 아린 리그에 직접 제작한 키프레임이다. [출처 기록](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/mixamo/source-review.md)을 참고한다.

- [적용·저장 기록](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/ue-application.json)과 [재검증](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/ue-reloaded-validation.json): `passed`.
- [새 Editor-Cmd PID 51740의 저장 자산 검사](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/ue-fresh-process.json): 보고서 `passed`. NullRHI 검사이며 렌더나 전체 Cmd 종료 코드 검증이 아니다.
- [독립 새 FBX 모션 검사](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/critic/final-motion-review.md): root 고정·루프 종점 정점차 0·유효한 자세·핵심 프레임 확인.
- [Content 보존 감사](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/content-preservation.json): 기존 6,363개 SHA 동일, DA 하나 변경, 새 클립 두 개, 삭제·범위 밖 변경 0. 기존 RefinedV3 9개 패키지와 v2/v3 Art 59개 파일 유지.
- [최종 Editor 빌드](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/build-final.log): 타이머 수정 반영, `Succeeded`, 6.51초.
- [최종 통합 PIE](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/tutorial-runtime.json): **159/159 통과**, 새 애니메이션 23단계 완료, 기존 튜토리얼 67개 통과·누락 0. 55/110/220cm/s의 Walk 재생률 약 0.5/1/2, 실제 클립 시간·본 변화·loop, 대화 우선순위·열기·닫기·범위 이탈, fallback·순간이동 복귀를 확인했다. 포션 2개 지급 → 실제 1개 소모 → 포탈 hard travel 후 잔여 1개 유지도 통과했다.

저프레임 fixture는 PlayerController BeginPlay가 기존 120fps 설정을 적용한 뒤 임시 `t.MaxFPS=5`를 다시 설정했다. 실제 게임·벽시계 프레임 간격이 0.1초를 초과하는 상태에서 native 타이머의 프레임당 한 번 호출과 중복 시간 샘플 보호를 검증했다. 종료 후 원래 `t.MaxFPS=120`으로 복원했으며 설정 파일은 저장하지 않았다. 물리 F 입력·한글 UI 배치는 위 수동 QA 절차로 별도 확인한다.

[기능 결과와 상세 QA](D:/Project/Rogue10m/Feature/doc/2026-10-09_arin-walk-talk-animation.md) · [설계](D:/Project/Rogue10m/Feature/architect/2026-10-09_arin-walk-talk-animation.md)

![Walk 실제 Blender 프리뷰](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/preview/walk.gif)

![Talk 실제 Blender 프리뷰](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/preview/talk.gif)

프리뷰는 Walk 18장·1.2초, Talk 30장·3초다. 실제 원본 동작은 30fps이며 프리뷰 GIF는 샘플 프레임으로 제작했다. [프리뷰 SHA·시간 기록](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/preview/render-manifest.json)에 원본 보존과 실제 재생 길이를 남겼다.

## SHA256와 범위

```text
A_Arin_Walk.fbx
cfb6f7d3dda9c3f3fe39da05f529da91d7f243bd78b9f3c92a9573d9af54c2b3

A_Arin_Talk.fbx
172601a141d9e41983ac949e2e00db1de9adf539f81466fc5b2f7ff0fa76a82e

Arin_Motions.blend
379e07e07388f8095cfce59093deba2de4534bf028c706440674bcf55bae5528
```

Talk는 몸짓이며 얼굴 립싱크·개별 손가락 동작은 포함하지 않는다. 옷·가방·머리카락 추가 물리, 지형 발 IK, 영화용 상태 전환 블렌딩은 포함하지 않는다. 기본 평지 동작을 기존 모델과 골격에 적용하는 범위다.
