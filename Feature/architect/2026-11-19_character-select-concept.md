# 캐릭터 선택창 ImageGen 시안

- 작업: Sprint#9-34, `Sprint#9-34-character-select-concept`.
- 요청: imagegen으로 캐릭터 선택창 수정 시안을 제작한다.
- 범위: 기존 배경을 참조한 16:9 래스터 UI 시안과 생성 프롬프트. 실제 Widget, C++, 저장 데이터, Unreal 자산 변경은 포함하지 않는다.

## 작업 패킷

- 목표: 세 저장 슬롯과 선택/생성/삭제/접속 관계가 읽히는 완성 시안 1장.
- 입력: `SourceArt/UI/CharacterLobby/T_CharacterLobby_Background.png`(배경 참조), `Feature/doc/evidence/quit-menu-20261111/TwoMapsMenu1118/02_lobby_launcher.png`(기존 UI 구성 참조), 현재 Lobby C++/Editor 구성.
- 수정 위치: `SourceArt/UI/CharacterLobby/2026-11-19/`, 짝 문서, Sprint 변경 이력, 한국어 일일 DevLog.
- 완료 조건: 내장 image_gen 결과 저장, 전신 프리뷰/3슬롯/선택 강조/기본 액션/한글 문구 육안 검수, 최종 프롬프트 보존.
- 검증: 이미지 디코딩·치수·SHA256, `Scripts/CheckGeneratedChanges.ps1`, 범위 한정 `git diff --check`. 런타임 변경이 없어 UE 빌드/PIE는 실행하지 않는다.
- 롤백 경계: 이번 시안과 짝 문서, 이번 변경 이력/DevLog 항목만 제거. 기존 WIP와 원본 배경을 보존한다.

## 유지할 계약

직업 선택이 아닌 최대 3개 저장 프로필 선택이다. 인간/드워프/오크, 남녀를 지원한다. 정면 전신 프리뷰, 이름 및 종족·성별, 빈 슬롯 생성, 단일 클릭 선택·더블클릭 접속, 새 캐릭터/삭제/접속 액션을 유지한다. 레벨·직업·전투력·장비 선택 등 새 기능을 시안에 추가하지 않는다.

밝은 성채 테라스·석재/금속 장식·푸른 산맥의 기존 방향을 참조한다. UI 위계를 개선하고 과도한 네온·광선 효과를 피한다. 이미지의 예시 캐릭터는 레이아웃 설명용 생성 표현이며 기존 게임 모델이나 Fab 소유 확인 증거가 아니다. 사용자가 새 제작을 명시했으므로 내장 ImageGen을 사용하며 새 Fab 반입·구매는 없다.

## 단계별 역할

Architect: 기존 코드·배경·검증 캡처 조사. Implementer: image_gen 생성 및 원본 그대로 보존. Builder: 이미지/문서 검사. Reviewer: 레이아웃·문구·기존 계약 확인. Doc Writer: 결과·한계·생성 방식·프롬프트 위치와 DevLog 기록.

맵 제작 진행 중이므로 완성 시안 체크포인트만 작업 브랜치에 게시하고 develop/test/main 승격은 보류한다. 이 시안 완료를 실제 선택창 적용 완료로 기록하지 않는다.
