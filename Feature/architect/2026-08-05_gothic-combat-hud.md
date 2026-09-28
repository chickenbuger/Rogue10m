# 고딕 전투 HUD 비주얼 개편

## 목표

기존 `WBP_Rogue10mMainHUD`의 화면 배치와 C++ 바인딩 이름을 유지하면서, 레퍼런스처럼 어두운 흑철·청동 테두리·붉은 룬 포인트를 사용하는 전투 HUD로 시각 품질을 높인다. 생성한 이미지는 프로젝트 내부 원본 PNG와 Unreal Texture2D 자산으로 남기고 실제 Widget Blueprint Designer 트리에 배치한다.

## 범위와 제약

- 대상: `Content/Widget/WBP_Rogue10mMainHUD` 및 HUD 파트 Widget Blueprint.
- 유지: 체력/스테미나/아이덴티티/경험치/스킬/아이템/로그의 기존 앵커, 좌표, 기능 바인딩 이름.
- 변경: 배경 프레임, 슬롯 프레임, 중앙 엠블럼, 바 장식의 텍스처와 색상.
- 직접 바이너리 편집 금지: 모든 `.uasset` 변경은 Unreal Editor Python API로 수행한다.
- 레퍼런스는 분위기와 재질의 참고용이며 원본 UI를 복제하지 않는다.

## Ultrawork Packets

### Packet 1 — 배치 기준선 고정

- 목표: 현재 Designer 위젯 트리의 부모 관계와 Canvas 슬롯 값을 기록한다.
- 입력: `WBP_Rogue10mMainHUD`와 `Content/Widget/Parts`.
- 수정 위치: `Scripts/Editor/InspectCombatHUDLayout.py`, `Saved/CombatHUDLayoutBefore.txt`.
- 완료 조건: 주요 바인딩 위젯의 부모·클래스·앵커·좌표·크기·ZOrder가 출력된다.
- 검증 명령: UnrealEditor-Cmd에서 검사 스크립트 실행.
- 롤백 경계: 읽기 전용 스크립트와 Saved 출력만 제거하면 된다.

### Packet 2 — 고딕 HUD 텍스처 제작

- 목표: 흑철, 낡은 청동, 룬 문양을 사용한 하단 HUD 장식 텍스처를 만든다.
- 입력: 사용자 레퍼런스 이미지(스타일 참고).
- 수정 위치: `Content/UI/HUD/Gothic/Source/*.png`, `Content/UI/HUD/Gothic/*.uasset`.
- 완료 조건: 투명 배경 PNG가 프로젝트에 저장되고 Texture2D로 임포트된다.
- 검증 명령: PNG 알파/해상도 검사 및 Unreal 자산 로드 검사.
- 롤백 경계: Gothic 폴더만 제거하면 기존 HUD 로직과 자산은 영향받지 않는다.

### Packet 3 — Designer 스타일 적용

- 목표: 기존 배치를 유지하고 생성 텍스처를 장식 레이어에 적용한다.
- 입력: Packet 1 좌표 덤프와 Packet 2 Texture2D.
- 수정 위치: `WBP_Rogue10mMainHUD`, 관련 `WBP_*` 파트, `Scripts/Editor/BuildGothicCombatHUD.py`.
- 완료 조건: 기존 필수 바인딩 위젯이 모두 남아 있고 생성 텍스처가 Image/Brush에 연결된다.
- 검증 명령: `ValidateGothicCombatHUD.py`로 이름·부모·좌표·텍스처 참조 검사.
- 롤백 경계: 변경 전 좌표 덤프와 Git의 개별 `.uasset` 단위로 복원 가능하다.

### Packet 4 — 통합 검증과 문서화

- 목표: 컴파일, 에디터 자산 로드, 렌더 스크린샷, 생성물 검사를 완료한다.
- 수정 위치: `Feature/doc/`, `Docs/SprintChangeLog.md`, `DevLog/20260805.txt`.
- 완료 조건: Editor 빌드 성공, Widget Blueprint 컴파일 성공, 스크린샷에서 레이아웃 유지와 고딕 스타일이 확인된다.
- 검증 명령: `Scripts/BuildEditor.ps1`, `Scripts/CheckGeneratedChanges.ps1`, UnrealEditor-Cmd 검증 스크립트.
- 롤백 경계: 문서 변경은 코드/자산과 독립적으로 되돌릴 수 있다.

## 위험과 대응

- 생성 이미지의 투명 가장자리 품질: 크로마키 제거 후 알파 모서리와 색 번짐을 검사한다.
- UMG 스크립트가 기존 바인딩을 삭제할 위험: 수정 전 트리 덤프를 저장하고 필수 이름을 검증한다.
- 해상도별 비율 왜곡: 장식은 ScaleBox 또는 비율 보존 Image로 배치하고 기준 해상도 1280×720에서 검수한다.
- 사용자 미추적 파일: 작업 대상 목록 밖의 파일은 수정하거나 정리하지 않는다.
