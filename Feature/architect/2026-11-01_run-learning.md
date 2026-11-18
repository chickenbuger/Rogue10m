# 필드 런 수동 학습 연결

## 문제와 범위

기준 브랜치 `Sprint#9-16-basic-play-loop`, 기준 HEAD `c35aecf4758bb5a3b9f9d52ab487f91fd286ed88`. 부모가 세이브311개, 보존 대상6826파일, 파생 팔6개 및 기준 빌드 PASS를 확보했다.

현재 K 스킬트리에서 수동 습득은 StartHub/Waiting/NPC 범위의 로비 편집 권한을 재사용한다. 따라서 필드에서 얻은 런 포인트로 선택형 성장을 배울 수 없다. 이번 묶음은 런 수동 학습 접근만 열고 로비 영구 강화와 편집 권한을 유지한다.

## 구현 계약

- 새 `CanLearnDuringRun()`은 Running, Standalone, 살아 있는 권한 PlayerState와 그 소유 캐릭터, 로컬 조종, 현재 선택 프로필 일치, 정상 부착 및 저장 준비를 모두 요구한다. 전투 중 학습 자체는 획득으로 허용한다.
- `CanUnlockNode`의 수동 학습 접근만 기존 마을 학습 또는 위 런 학습으로 확장한다. RequiredAll/선행/선택/트랙/카탈로그/포인트·숙련 검증은 보존한다.
- `CanChangeInvestments`와 환불·Respec·수동 Identity 재전환·마을 Trait gate는 바꾸지 않는다. 새로운 프리셋 정책은 구현하지 않는다.
- 첫 IdentityEntry 학습 시 ActiveIdentity=None이면 기존대로 자동 활성화한다. 기존 Identity를 수동으로 다른 것으로 바꾸는 권한과 구분한다.
- 학습은 기존 런 SkillTree ledger에 기록한다. 영구 예산/세이브 버전/맵/원본 에셋/사용자 세이브 형식은 바꾸지 않는다. Discovery 보존과 다음 판 런 초기화는 기존 정산 경로를 재사용한다.

## 소유 파일

- Source/Rogue10m/Components/Rogue10mSkillTreeProgressionComponent.h/.cpp
- Source/Rogue10m/Tests/Rogue10mRunLearningRuntimeTest.cpp
- Scripts/VerifyRunLearning.ps1
- 이 계획 문서

## 검증 계획

`VerifyRunLearning.ps1`은 명시적인 새 label과 QA 전용 Profile/Progression/SkillKey 슬롯3개를 요구하고 기존 슬롯/증거가 있으면 실행 전 중단한다. StartHub에서 신규 프로필을 만든 뒤 실제 포털로 OpenRunMap Running에 진입하고 런 XP/학습/편집 거부/정산 후 초기화·Discovery 보존을 검증한다. 별도 Normal/Abandon/Defeat 실행은 각각 새 label을 사용한다. 프로그램 입력과 렌더링 캡처를 물리 입력이나 정상 필드 완주로 표현하지 않는다.

회귀: 포인트 및 숙련 경로, RequiredAll 미달/선행/트랙/포인트 부족, 중복 클릭 시 1회 지출, 골드·로비 예산 무변경, 최초 Identity 자동 활성, 공격 중 첫 학습과 기존 수락 공격 효과/자원 비용, 필드 환불·수동 재전환·Trait 거부, 사망/terminal/profile 미준비/미부착 거부, 포기 기준선·런 학습 초기화·Discovery 유지. 기존 Hybrid/Requirements/AttackResources 및 RunDomains/Result/Menu 회귀는 부모가 빌드 후 실행한다.

## 완료와 롤백

자식은 파일 구현과 검토까지만 수행한다. 빌드·UE 실행·커밋은 부모가 담당한다. 새 조건이 실패하면 학습 거부 상태를 유지하고 이유를 기록한다. 롤백은 이번 접근 메서드/호출부 및 신규 QA 파일 단위이며 기존 원본 자산·맵·사용자 저장·다른 변경을 reset/삭제하지 않는다. 원격 push/병합/설치는 하지 않는다.


## 렌더링에서 확인한 타이머 가림 보정

LearnA1101R 화면에서 F6 디버그 버튼이 우측 상단 타이머 위치(Y 24..56)를 덮었다. 디버그 진입 버튼을 Y76으로 옮기고, 렌더링 QA에서 실제 타이머·버튼의 캐시 geometry가 유효하며 겹치지 않는지 확인한다. 타이머 규칙·20분 제한·게임 상태는 바꾸지 않는다. 기존 실패/수정 전 사진은 보존한다.
