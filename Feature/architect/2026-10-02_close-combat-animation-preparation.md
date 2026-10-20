# 보유 Close Combat 동작의 Manny 프리뷰 준비

기준 프로젝트: `D:\Project\Rogue10m\Rogue10m.uproject`, UE 5.8.
기준 HEAD: `b7ab6b6`, 기존 브랜치: `Sprint#5-5-editor-ai-assistant-error`.
작업 브랜치: `Sprint#5-6-close-combat-animation-preparation`.

## 범위

9월 28일 안정화와 9월 29일 FistAuthored·머리 카메라, 9월 30일 스킬트리의 미커밋 상태를 보존한다. 실제 VaultCache에 있는 Close Combat Animset의 좌훅·우어퍼컷·로킥·직선 킥·가드·4방향 회피를 기본 Manny로 리타기팅한다. 게임용 DA·몽타주·AnimBP·카메라·전투 C++은 수정하지 않는다. 이 단계는 클립 선정과 적용 준비이며 게임 통합 완료로 기록하지 않는다.

## Ultrawork Packets

| 작업 | 목표·입력 | 수정 위치 | 완료 조건·검증 | 복구 경계 |
| --- | --- | --- | --- | --- |
| 1 조사 | 기존 git/uproject/지침, 로컬 에셋과 현재 타이머 계약 확인 | 읽기 전용, 새 evidence | 실제 경로·에셋 출처·baseline SHA256 기록 | 기존 파일 변경 없음 |
| 2 프리뷰 | 보유 VaultCache의 필요한 패키지를 원래 package 경로로 복사, UE4→Manny FK 프리뷰 | 새 `Content/CloseCombatAnimSet/`, 새 `Animation/Preview/CloseCombat/`, 새 Editor 스크립트 | Unreal API로 로드·베이크·Manny skeleton·root/IK 설정·포즈 수치 검사 | 신규 폴더만 독립; 게임 참조 연결 없음 |
| 3 검토 | 공격 접촉 후보 및 머리·골반·발 이동, RM 방향 증거, 남은 소스 확인 | 새 Feature 문서·evidence, DevLog와 SprintChangeLog append | 에디터 실행 로그·JSON, Python 문법, Harness 경로, 기존 파일 SHA256 재검사 | 새 문서 및 로그 추가분 |

## 수용 기준

- 원본 VaultCache와 기존 프로젝트 파일의 바이트 보존. 새 프로젝트·결제·약관·업로드·삭제·커밋·push 없음.
- 새 리타기터에서 root를 실제 `root`로 설정하고 추가 Run IK Rig를 끈다. 기존 외형 리타기터와 Source FootIK→Slot 순서 유지.
- clip 머리 회전과 손/발 도달 peak는 에디터 포즈 수치다. 시각적 자연스러움·실제 접촉·판정 방향 검증과 구분한다.
- 첫 타격: `HitStartDelaySeconds / max(0.01, AttackSpeedMultiplier * AnimationPlayRate)`.
- 후속 타격: `max(0.01, HitInterval / AttackSpeedMultiplier)`. 새 연타의 간격은 클립 시간에서 `HitInterval * AnimationPlayRate`가 되므로 별도 검증한다.
- 판정 직전 AppearanceCamera 갱신 후 camera origin/forward/rotation을 사용한다. 새 공격의 머리 움직임은 정면 타깃과 함께 검증해야 한다. 카메라 설계는 변경하지 않는다.
- Close Combat 회피를 구르기로 표기하지 않는다. 실제 구르기는 Rolls and Dodges 소스가 확보될 때 별도 준비한다.

## 검증 계획

새 Editor 스크립트의 기본 실행은 읽기 전용 audit이며 명시 `-RogueCloseCombatPrepare`에서만 비어 있는 새 Preview 폴더에 생성한다. 기존 패키지를 덮어쓰지 않는다. 에디터 포즈를 60Hz로 샘플링해 skeleton·유한수·본 길이·머리 및 pelvis 회전·접촉 후보를 기록한다. 이번 단계에서 DA를 연결하지 않으므로 새 후보의 실제 표적 피해와 debug volume 화면 검증은 다음 통합 단계의 완료 기준으로 남긴다.
