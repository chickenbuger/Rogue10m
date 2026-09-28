# 전체 폭 경험치·레벨 바 이미지

## 목표

기존 고딕 전투 HUD와 동일한 흑철·청동·붉은 룬 재질로 화면 하단 전체 폭을 사용하는 경험치·레벨 바 이미지를 제작한다. 좌측에는 동적 레벨 숫자를 표시할 빈 메달을 두고, 나머지 영역은 하나의 긴 경험치 트랙으로 사용한다.

## 제약

- 최종 PNG 폭은 1920px로 고정하고 세로 비율은 원본을 유지한다.
- 이미지 내부에는 고정 레벨 숫자, 문자, 아이콘, 경험치 채움색을 넣지 않는다.
- 동적 레벨 텍스트와 경험치 ProgressBar는 UMG가 위에 렌더링한다.
- 기존 `T_HUD_GothicExperienceFrame`과 Widget Blueprint는 변경하지 않는다.
- 신규 자산명은 `T_HUD_GothicXPLevelBar`를 사용한다.

## 작업 패킷

### Packet 1 — 이미지 생성 및 투명화

- 목표: 전체 폭 고딕 경험치·레벨 바 PNG 제작.
- 수정 위치: `Content/UI/HUD/Gothic/`.
- 완료 조건: 알파 채널, 투명 모서리, 1920px 폭, 빈 레벨 메달과 연속 경험치 트랙 확인.
- 롤백 경계: 신규 PNG와 Source 원본만 제거.

### Packet 2 — Unreal 임포트 및 검증

- 목표: UI용 Texture2D 생성.
- 수정 위치: `Content/UI/HUD/Gothic/T_HUD_GothicXPLevelBar.uasset`.
- 완료 조건: UI LOD Group, NoMipmaps, NeverStream 설정과 1920px 폭 확인.
- 검증 명령: UnrealEditor-Cmd Python 임포트/검증.
- 롤백 경계: 신규 Texture2D만 제거.

### Packet 3 — 문서화

- 목표: 제작 방식과 적용 권장 구조 기록.
- 수정 위치: `Feature/doc/`, `Docs/SprintChangeLog.md`, `DevLog/20260812.txt`.
- 완료 조건: 한국어 DevLog와 Notion 요약 후보 포함.
