# 아린 기본 걷기·대화 애니메이션

아린의 기존 RefinedV3 모델과 Idle을 유지하면서 전용 걷기와 대화 몸짓을 제작하고 시작마을 안내인 설정에 저장했다. 실제 NPC 이동에 따른 Walk와 대화 열기·닫기에 따른 Talk 전환을 StartHub PIE에서 확인했다. **제작·적용·통합 검증 완료** 상태이며, 최종 실행의 159개 확인 항목이 모두 통과했다. 새 애니메이션 검증 23단계와 기존 튜토리얼 67개 회귀 항목을 포함한다.

- 작업 브랜치: `Sprint#5-19-arin-walk-talk-animation`
- [설계·Ultrawork Packets](D:/Project/Rogue10m/Feature/architect/2026-10-09_arin-walk-talk-animation.md)
- [Blender 편집 원본](D:/Project/Rogue10m/Art/NPC/Arin/Animation/Arin_Motions.blend)
- [걷기 FBX](D:/Project/Rogue10m/Art/NPC/Arin/Animation/A_Arin_Walk.fbx) · [대화 FBX](D:/Project/Rogue10m/Art/NPC/Arin/Animation/A_Arin_Talk.fbx) · [사용 안내](D:/Project/Rogue10m/Art/NPC/Arin/Animation/README.md)

## 제작물과 Unreal 적용

| 구분 | Blender 제작 | Unreal 저장 결과 |
|---|---|---|
| Walk | `Arin_Walk`, 30fps, 1..37 키프레임, 36프레임 주기·1.2초 | 30/1 sample rate, 36프레임·약 1.20000005초 |
| Talk | `Arin_Talk`, 30fps, 1..91 키프레임, 90프레임 주기·3초 | 30/1 sample rate, 90프레임·3초 |
| 골격 | 기존 23개 제작 본과 스킨 가중치 유지 | 기존 `SK_Arin_Skeleton` 사용 |
| 이동 | root 고정, in-place Walk, 기준 속도 110cm/s | Root Motion 비활성, 실제 actor 이동 속도로 재생률 조절 |

Blender의 마지막 키는 첫 자세를 반복하는 루프 종점이다. 프리뷰 GIF에는 이 중복 종점을 넣지 않았다. 기존 메시·UV·재질·가중치를 다시 만들거나 bake하지 않았으며, 새 파일은 애니메이션 전용 FBX 두 개와 편집용 BLEND다.

저장된 새 패키지는 다음 두 개다.

```text
/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Walk
/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Talk
```

두 클립은 `/Game/Rogue10m/Characters/Arin/RefinedV3/SK_Arin_Skeleton`을 사용한다. `/Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide`에 Walk·Talk 참조, 기준 속도 110cm/s, 걷기 시작 기준 10cm/s를 저장했다. BP CDO와 실제 StartHub 안내인에 native `GuideAnimation` 컴포넌트가 존재하는 것도 읽어서 확인했다. [적용 기록](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/ue-application.json)과 [저장 후 재검증](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/ue-reloaded-validation.json)은 `passed`다.

## 게임에서 재생하는 방식

[Rogue10mTutorialAnimationComponent](D:/Project/Rogue10m/Source/Rogue10m/World/Rogue10mTutorialAnimationComponent.cpp)는 **대화 요청 → 걷기 → Idle** 순서로 클립을 선택한다. NPC와 컴포넌트의 Tick은 비활성화하고, 0.1초 타이머로 actor의 실제 XY 이동 거리를 측정한다. 타이머는 프레임당 최대 한 번 실행하며 같은 게임 시간의 중복 샘플은 상태를 초기화하지 않는다.

기본값에서는 속도가 10cm/s를 초과하면 Walk를 시작하고, 이미 걷는 상태에서는 5cm/s 이하로 떨어지면 멈춘다. 걷기 재생률은 `실제 속도 / 110`이며 0.01..10 범위로 제한한다. 같은 클립을 타이머마다 다시 시작하지 않는다. 대화가 닫히거나 상호작용 범위를 벗어나면 대화 요청을 해제하고 이동 상태에 맞는 Walk 또는 Idle로 돌아간다. 300cm를 초과한 위치 변경은 순간이동으로 취급해 이동 측정을 초기화한다. 클립이 없거나 골격이 호환되지 않으면 기존 Idle로 돌아간다.

안내인의 기존 위치를 유지한다. 이 컴포넌트는 이동을 감지해 애니메이션을 바꾸며 순찰 경로·AI 이동·NavMesh 행동을 새로 생성하지 않는다. Walk는 기본 평지용 in-place 모션이다.

## 레퍼런스 출처

공식 Mixamo의 [Walking 검색](https://www.mixamo.com/#/?page=1&query=walking&type=Motion%2CMotionPack)에서 **Walking / Feminine Walk Forward**, [Talking 검색](https://www.mixamo.com/#/?page=1&query=talking&type=Motion%2CMotionPack)에서 **Talking / General Conversation**의 실제 공개 프리뷰를 관찰했다. 교대 발 착지·반대팔 스윙과 설명 손짓·작은 고개 움직임을 참고해 아린 리그에 자체 키프레임을 제작했다.

현재 Mixamo 세션은 로그아웃되어 다운로드에 Adobe 로그인이 필요했다. 실제 다운로드한 Mixamo FBX는 **0개**다. 이 결과를 Mixamo 모캡 파일을 다운로드해 적용한 것으로 표현하지 않는다. [출처 확인 기록](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/mixamo/source-review.md)과 [획득 상태 JSON](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/mixamo/source-acquisition.json)에 관찰한 제목·옵션·로그인 제약을 남겼다. Adobe의 [공식 사용 안내](https://helpx.adobe.com/creative-cloud/help/animate-characters-mixamo.html)와 [FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html)도 확인했다.

## 검증 증거

| 검증 | 확인한 결과 | 증거 |
|---|---|---|
| 최종 Editor 타깃 빌드 | 타이머 수정이 반영된 최종 빌드 `Succeeded`, 6.51초 | [최종 빌드 로그](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/build-final.log) |
| 새 Blender 프로세스의 FBX 검사 | Walk 37·Talk 91개 자세 전체 검사, root 변동 0, 루프 종점 정점차 0, 비유한 좌표·4배 초과 edge 신장 0 | [독립 최종 검토](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/critic/final-motion-review.md), [Walk](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/critic/fresh-walk-motion-final.json), [Talk](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/critic/fresh-talk-motion-final.json) |
| 실제 Unreal 핵심 자세 | Walk 1/10/19/28, Talk 24/68 프레임의 imported animation 화면 | [엔진 화면·복원 기록](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/ue-motion/manifest.json) |
| 저장·재로드 | 30fps, 길이·골격·Root Motion·DataAsset 참조·보호한 안내인 데이터 통과 | [재검증](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/ue-reloaded-validation.json) |
| 새로운 Editor-Cmd 프로세스 | PID 51740, NullRHI 저장 자산 검사 보고서 `passed` | [새 프로세스 보고서](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/ue-fresh-process.json) |
| 자산 보존 | 기존 Content 6,364개 중 6,363개 SHA 동일, DA 하나 변경, 신규 애니메이션 두 개, 삭제·범위 밖 변경 0 | [Content 보존 감사](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/content-preservation.json) |
| 최종 통합 PIE | **159/159 통과**, 새 애니메이션 23단계 완료, 기존 튜토리얼 67개 모두 통과·누락 0 | [최종 실행 보고서](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/tutorial-runtime.json) |

새 Editor-Cmd 검사는 화면 렌더나 물리 F 키 검사가 아니다. 첫 commandlet 시도에서는 LevelEditor 컨텍스트를 요구하는 호출이 실패했고, 새 프로세스에서 이용 가능한 검사 경로로 수정한 뒤 위 보고서를 얻었다. 이 문서는 보고서의 통과를 근거로 하며, 확인하지 않은 전체 Cmd 종료 코드 0을 주장하지 않는다.

Content 감사는 기존 RefinedV3 메시·골격·Idle·PBR 재질·4개 텍스처 **9개 패키지가 모두 그대로임**을 별도로 확인했다. 기존 `ReferenceRefined`와 `Refined` 두 Art 폴더의 **59개 파일**도 추가·변경·삭제 없이 보존되었고, 이전 최종 검증의 고정 SHA 18개와 일치했다. Blueprint와 StartHub 안내인 ExternalActor 패키지는 이번 감사에서 변경되지 않았다. 대사·회복 포션 2개·reward ID와 기존 튜토리얼 설정도 유지했다.

### 실제 재생·기존 기능 회귀 결과

최종 PIE 보고서는 `status=passed`, `animation_fixture_complete=true`, `baseline_checks_passed=67`, `baseline_missing_checks=[]`다. 실제 native SingleNode에서 선택한 클립, 계속 증가하는 재생 시간, 허벅지·팔·머리 본의 변화, 고속 Walk의 반복을 관찰했다. 제어된 NPC 이동에서 55/110/220cm/s에 대응하는 재생률은 약 0.5/1/2였다. 대화 열기·닫기·이동 중 Talk 우선순위·범위 이탈 해제, 누락·다른 골격 클립의 Idle 복귀, 순간이동 후 상태 초기화도 통과했다.

기존 튜토리얼 회귀에서는 포션 2개를 실제 지급하고 한 개를 실제 소모했다. 기존 포탈을 통한 `OpenRunMap` hard travel 뒤 남은 한 개만 유지되고, 소모한 포션이 복원되거나 보급품이 중복되지 않았다. 이 검사는 실제 reflected 대화·인벤토리 API를 사용했다. 물리 F 입력과 한글 UI 배치의 별도 수동 확인을 대신하지 않는다.

저프레임 검사에서는 PlayerController의 BeginPlay가 기존 120fps 설정을 다시 적용해, PIE 시작 전에 건 임시 5fps 제한이 실제 플레이에 유지되지 않는 검사 순서 문제를 발견했다. 최종 fixture는 BeginPlay 뒤 `t.MaxFPS=5`를 적용하고 읽어 확인했다. 게임 프레임 간격 약 0.200003초와 실제 경과 시간 약 0.203초를 함께 관찰해 0.1초 이동 타이머보다 느린 프레임에서 시험했음을 확인했다. native 타이머의 프레임당 최대 한 번 호출과 중복 시간 샘플 보호를 수정한 상태에서 클립 재생이 진행되었다. 완료 후 `t.MaxFPS`는 원래 값 120에서 120으로 복원되었고 `configuration_saved=false`다.

## 실제 모션 프리뷰

[연속 프리뷰 기록](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/preview/render-manifest.json)은 렌더 전·후 및 GIF 조립 후 원본 SHA가 같음을 확인한다. Walk GIF는 18장·1,200ms, Talk는 30장·3,000ms다. GIF의 10ms 시간 단위에 맞춰 Walk는 70/60/70ms 지연을 반복해 실제 1.2초 주기를 유지한다. 원본 클립은 두 개 모두 30fps이며 GIF는 샘플 프레임 프리뷰다.

![아린 걷기 실제 Blender 프리뷰](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/preview/walk.gif)

![아린 대화 몸짓 실제 Blender 프리뷰](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/preview/talk.gif)

## 수동 F 대화 QA 안내

1. `StartHub`에서 Play를 시작하고 기존 안내인 `SV_TutorialGuide_Arin`에게 320cm 이내로 접근한다. 정지한 안내인은 기존 Idle을 재생해야 한다.
2. F를 한 번 눌러 대화를 연다. 한글 안내 카드가 유지되고 아린이 Talk 몸짓을 반복하는지 본다. 게임은 일시정지되지 않아야 한다.
3. 아직 보급품을 받지 않았다면 F를 다시 눌러 회복 포션 2개를 받는다. 대화 중 Talk가 유지되는지 확인하고, 다음 F로 닫아 Idle로 돌아오는지 본다. 이미 수령한 경우에는 두 번째 F가 대화를 닫는다.
4. 대화를 다시 열고 상호작용 범위 밖으로 이동한다. 대화 카드가 사라지고 Talk 요청이 해제되어야 한다. 다시 접근해도 보급품을 중복 지급하지 않아야 한다.
5. 이동 동작 검토용 복제 NPC나 PIE 전용 fixture를 일정 속도로 이동시켜 Walk 진입·속도 대응·정지 후 Idle을 확인한다. 기존 안내인의 위치나 맵을 저장해 바꾸지 않는다.

이 목록은 직접 확인할 절차이며 이 작업에서 물리 F 입력까지 새로 완료했다는 결과가 아니다. 자동 PIE 검증은 별도 보고서의 최종 상태를 따른다.

## 최종 파일 SHA256

| 파일 | SHA256 |
|---|---|
| `A_Arin_Walk.fbx` | `cfb6f7d3dda9c3f3fe39da05f529da91d7f243bd78b9f3c92a9573d9af54c2b3` |
| `A_Arin_Talk.fbx` | `172601a141d9e41983ac949e2e00db1de9adf539f81466fc5b2f7ff0fa76a82e` |
| `Arin_Motions.blend` | `379e07e07388f8095cfce59093deba2de4534bf028c706440674bcf55bae5528` |

위 값은 [제작·export 기록](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/motion-build.json)과 같은 파일을 가리킨다.

## 적용 범위와 한계

Talk는 상체·머리·팔의 대화 몸짓이며 음성 립싱크나 개별 얼굴·손가락 애니메이션이 아니다. 옷·가방·땋은 머리에는 추가 물리 시뮬레이션이 없다. 기본 평지 걷기를 제작했으며 경사면 발 IK·지형 추종·AI 경로 이동·모든 순간의 self-collision 검증은 포함하지 않는다. SingleNode의 기본 상태 교체는 영화용 전환 블렌딩 품질을 보장하지 않는다.
