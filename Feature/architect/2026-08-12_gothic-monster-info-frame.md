# 고딕 몬스터 정보 프레임

## 목표

플레이어가 몬스터를 조준할 때 표시되는 `WBP_MonsterInfo`를 꾸밀 수 있도록, 현재 420×68 배치에 맞는 고딕 프레임 이미지를 준비한다.

## 현재 UI 계약

- 표시 위치: 화면 상단 중앙.
- 기준 크기: 420×68.
- 동적 정보: `LV N : 몬스터 이름`, 체력 ProgressBar, 현재/최대 체력.
- 필수 바인딩: `UI_MonsterNameText`, `UI_MonsterHealthBar`, `UI_MonsterHealthText`.

## 이미지 설계

- 최종 크기: 840×136 RGBA PNG. UMG에서 420×68로 0.5배 표시한다.
- 상단: 동적 이름/레벨을 위한 빈 명패.
- 하단: 동적 체력 채움을 위한 빈 트랙.
- 좌우: 추후 속성/상태 아이콘을 올릴 수 있는 작은 마름모 홈.
- 고정 문자, 숫자, 아이콘, 몬스터 초상, 체력 채움색은 포함하지 않는다.
- 기존 고딕 HUD의 흑철·청동·붉은 룬 재질을 유지한다.

## 작업 패킷

### Packet 1 — 이미지 제작

- 수정 위치: `Content/UI/HUD/Gothic/`.
- 완료 조건: 투명 모서리, 840×136, 비율 왜곡 없음.
- 롤백 경계: 신규 PNG와 Source 원본만 제거.

### Packet 2 — Unreal 임포트

- 수정 위치: `Content/UI/HUD/Gothic/T_HUD_GothicMonsterInfoFrame.uasset`.
- 완료 조건: UI Texture Group, NoMipmaps, NeverStream, 크기 검증 통과.
- 롤백 경계: 신규 Texture2D만 제거.

### Packet 3 — 문서화

- 수정 위치: `Feature/doc/`, `Docs/SprintChangeLog.md`, `DevLog/20260812.txt`.
- 완료 조건: UMG 적층 권장 구조와 검증 결과 기록.

## 비범위

이번 작업에서는 기존 `WBP_MonsterInfo`의 배치나 바인딩을 변경하지 않는다.
