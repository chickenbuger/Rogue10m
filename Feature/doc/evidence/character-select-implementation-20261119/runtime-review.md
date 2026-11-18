# 실제 게임 검증의 구분

## A1119 — 실행기 설정 실패

맵을 직접 지정하면서 메뉴 모드를 생략해 기본 gameplay GameMode로 실행됐고, opt-in driver의 안전 사전 검사가 거절했다. Editor 전용 Python toolset은 `-game`에서 필요한 에디터 타입이 없어 시작 traceback을 냈다. 총 오류 59줄이며 로그 출력기의 cp949 인코딩도 특수문자에서 실패했다. 기존 저장 491개는 모두 보존됐고 QA 프로필 생성은 없었다.

실행기에 실제 시작 화면 URL의 메뉴 GameMode를 명시하고, 기존 프로젝트 게임 QA와 같은 `-DisablePython`을 사용했다. 이것은 게임에서 쓰지 않는 에디터 Python 초기화의 런처 구분이며, 자산 저작에는 Python을 켜고 관련 검사를 전부 실행한다. 테스트나 로그 수준을 끄지 않았다. 로그 검사 Python은 `-X utf8`로 호출한다.

## B1119 — 기능 통과, 시각 실패

1920×1080 실제 렌더 게임에서 프로그램 UMG 입력·실제 Slate 중앙 hit path를 포함한 237검사/실패0, 105.001초를 확인했다. 생성·취소·3슬롯·삭제 두 번 확인·빈 슬롯·선택·StartHub 접속을 포함한다. 기존 저장 491개가 바이트 그대로 보존됐고 새 저장은 승인된 QA 이름에만 생성됐다.

추가로 컴퓨터 사용 도구를 통해 실제 마우스로 첫 번째 이름표를 클릭해 선택 배지가 옮겨지는 것, 두 번째 캐릭터 프리뷰를 클릭해 다시 선택되는 것, 접속 버튼을 클릭해 실제 StartHub로 진입하는 것을 관찰했다. Driver 자체 summary의 `physical_input=not_performed`는 프로그램 드라이버의 범위이며 원본 기록은 변경하지 않는다. 외부 물리 클릭 확인과 별도로 구분한다.

육안 검토에서는 커스텀 테두리 전체가 창의 데스크톱 원점만큼 약 +326/+190px 어긋났고, 모델이 프리뷰 안에서 작게 떠 보였다. 따라서 수치 통과를 시각 통과로 기록하지 않았다. 실제 실패 대표 사진 1장 `important-failure/B1119-displaced-outlines.png`를 보존했다. NativePaint의 paint-space/cached desktop-space 혼용을 캐시된 루트 기하 기준으로 수정했고, 프리뷰 프레이밍을 별도로 조사했다.

## 기존 자산 호환성 문제

B1119 전체 로그 검사는 실패1이며 LoadErrors 5건이 남았다. 인간 남/여·드워프 남·오크 남의 vendor skeleton이 제거된 `/Engine/EngineMeshes/Humanoid` RigConfig를 참조하는 문제다. 이는 `Feature/doc/2026-11-16_reference-scanner-compatibility.md`에 이미 남겨진 별도 호환성 문제와 일치한다. 새 UI가 만든 에러로 단정하지 않지만 신규 세션의 5건을 0건으로 보고하지 않는다. 원본 skeleton을 임의 resave하거나 known-missing/verbosity 설정으로 숨기지 않는다.

몸체가 기본 회색 재질로 보이는 문제도 기존 `/Engine/ArtTools/RenderToTexture/Materials/Debug/M_2sidedLit`의 SkeletalMesh usage 경고와 함께 관찰됐다. UI 작업에서 원본 모델·의상·재질을 교체하거나 고쳤다고 주장하지 않는다. 최종 UI 화면과 기능 검증 결과는 별도 최종 결과를 따른다.
