# 보스 대응 입력 전략과 기존 애니메이션 관찰 결과

2026-11-01 UTC. 프로젝트 D:\Project\Rogue10m, 브랜치 Sprint#9-21-prepared-run-input-qa, 기준1928869d. 적용·빌드·실행한 QA의 결과와 미확정 시각 문제를 기록한다. 최종 로컬 커밋은 이 문서의 Git 이력에서 확인한다.

정상 게임 입력을 쓰는 선택형 QA 전략을 추가하고, 실제 보스 도달 후 자연 사망 → 저장 완료 → 마을 귀환을 확인했다. 보스 승리는 달성하지 못했다. 기본 시점의 몸통 가림과 준비 자세의 손 가독성도 남아 있다.

## 변경 범위

- `Source/Rogue10m/Tests/Rogue10mBasicRunPlaytest.cpp`: 기본 OFF인 `RogueBasicRunPreparedBoss`와 `RogueBasicRunAnimationEvidence` 옵션. 전자는 실제 비용을 읽어 걷기/자연 회복으로 회피 예산을 남기고, 공개 패턴 상태의 Windup에 일반 방향키+Alt, Recovery에 일반 공격 입력을 보낸다. 후자는 기존 공격/회피의 자산·상태·손 좌표와 단계별 캡처를 관찰한다.
- `Scripts/VerifyBasicRunPlay.ps1`: 요청한 두 옵션의 실제 summary echo를 확인한다. 프로세스 정상 종료 코드만으로 검증 완료나 승리로 처리하지 않는다. fresh QA 3슬롯 및 종료 후 원본 저장 해시 보존 검사를 유지한다.
- 첫 빌드의 C4458은 지역 `State`와 기존 PlayerState 멤버 이름 충돌이었다. 지역 이름만 `PhaseState`로 바꿨으며 전략이나 게임 규칙은 바꾸지 않았다.

제품 수치·판정·AI·맵·원본 애니메이션·저장 스키마를 변경하지 않았다. HP/XP/자원/위치 setter, 무적, 강제 승리, 임의 보상, 테스트용 스폰을 추가하지 않았다. 패턴 상태를 읽는 자동 전략이므로 사람의 반응 가능성이나 공정성 증거가 아니다. OwnerArms/Framing 기본 OFF 상태도 유지했다.

## 실제 실행과 결과

실행 label `BasicRun1101I`, 두 옵션 ON. wrapper 시작 `04:55:27.4586449Z`, 종료 영수증 `05:02:26.0319808Z`; 실행기는 PID 43024의 `05:02:25 UTC` 정상 종료/exit 0을 확인했다. driver wall time은 `400.675초`다. 게임의 1200초 런 제한은 바꾸지 않았다. wrapper 1560초는 관찰용 상한이다.

| 항목 | 관측 결과 |
|---|---|
| 메뉴/캐릭터 생성, NPC F 대화·보급, 필드 진입 | 해당 milestone 모두 1 |
| 실제 대검 overlap 획득, 자연 처치 XP, K의 정상 UMG 런 학습 | 해당 milestone 모두 1. 이번 실행은 대검 UI 장착 검증이 아니다. |
| 실제 보스 도달 | 1. FinalBoss 대상 표본에서 HP 시작 10000, 최저 9937.703, 마지막 9939.703. 회복이 있어 최저값을 종료 HP나 총 피해량으로 해석하지 않는다. |
| 준비 전략의 Alt | 요청 5, 수락 관측 5, 미관측 0. 매번 실제 Stamina 6 차감과 이동 velocity를 관측했다. 피해 회피 성공이나 무적을 뜻하지 않는다. |
| 종료/정산/귀환 | wall 397.059 자연 사망, 399.169 `returned_death`; +6 로비 정산 및 결과 UI 귀환을 확인했다. `returned_reset_observed=1`, Discovery 유지 gate 통과. |
| 귀환 sample | StartHub/WaitingToStart, level 1, XP 0, Brawler mastery XP 0, run gold 0, save_ready 1. `sword/learned` 열은 누적 milestone이므로 현재 보유량으로 읽지 않는다. |
| 전체 보스 완료/물리 입력 | `full_boss_completion=0`, `physical_input=not_performed`. |

빌드 로그: 첫 32.58초 실패를 보존했고 이름 충돌 수정 후 11.45초 빌드 성공. wrapper 두 옵션 echo gate의 task 텍스트 fixture 10/10 통과는 UE나 한 판 플레이 테스트 수가 아니다. 실행 결과 근거는 `DevLog/BasicRunPlayQA/BasicRun1101I/{session.json,save-comparison.json,Driver/summary.txt,Driver/events.csv,Driver/samples.csv}`다.

## 저장·자산 보존

`save-comparison.json`은 실행 전 340개 → 실행 후 342개, 원본 변경/누락 0, 허용된 새 QA 저장 2개, 미확인 새 저장 0을 확인한다. 자동 복구/삭제/속성 변경은 없다. 기존 사용자 저장을 초기화하지 않았다. 종료 후 독립 검사에서도 기존340개 저장, 원본31개 자산과 OpenRunMap.umap SHA256이 전부 동일했다. 새 저장은 허용된 QA2개뿐이다.

## 애니메이션 기록의 실제 한계

새 클립·몽타주·타이밍을 제작하거나 수정하지 않았다. 실제 관측 자산은 Knuckle Primary01 → AM_Knuckle_Jab/A_Knuckle_Jab, Manny Simple source mesh → Hu_M body, StoneFist dodge → AM_Safe_Dodge_Step이다. authored 값과 관측 시간·좌표·사진 판정은 [상세 기록](../../DevLog/20261101_PreparedRunInput_Diagnostics/README.md)에 분리했다.

6개 파일 생성은 관측했지만 attack_first_damage와 attack_recovered는 processed 시점 phase가 미확정이다. 나머지 4개도 드라이버 상태/파일 일치이며 자세나 시각 품질 PASS가 아니다. 사진을 직접 열어 확인한 결과 준비 자세 손이 보이지 않고 블러가 있으며 dodge_active에 기존 몸통이 화면 대부분을 가린다. first-damage PNG는 HUD 70/70과 블러 때문에 접촉 증거로 사용할 수 없다. 단계 사진6개와 보스/정산/귀환 사진3개를 이 기능의 DevLog에 복사했고9개 모두1280×720 디코딩과 SHA256 검증을 통과했다. 디코딩 성공을 자세 품질 통과로 바꾸지 않는다. callback 시간은 GPU 픽셀 시간이나 정확 접촉 시각이 아니다.

## 남은 검증

정상 보스 승리, 전체 20분 시간초과 경로, 실제 키보드/마우스 완주, 장착 대검/충전 공격/물약 전략, 애니메이션 손 가독성·접촉·회복 자세, 기본 시점 몸통 가림, 60초 지형 목표, 4개 액티브 슬롯/프리셋, 전체 밸런스·패키징은 이번 PASS 범위가 아니다. 다음 실행은 fresh QA label 및 UI 제어 인계를 확인한 뒤 결정한다. 원격 push/병합은 하지 않았다.
