# Rogue10m 개인 스튜디오

작성: 2026-09-28 / Sprint#4-44-personal-studio

## Scope Gate

게임 개발 일정, 현재 상황, 개선/수정 내역을 확인하는 PC 전용 웹 대시보드. 기존 미커밋 변경을 보존하고 Studio/에 독립 구현한다. Unreal 코드/에셋 변경이나 UE 빌드는 필요 없다. 실제 DevLog, SprintChangeLog, Feature 문서를 읽고 출처와 기록 시점을 표시한다. 확정되지 않은 미래 일정을 만들지 않는다.

## ULW 패킷

|패킷|목표와 영역|완료 조건|검증|롤백 경계|
|---|---|---|---|---|
|S1 기록 연결|Studio/server.mjs, lib.mjs|Sprint 본문/개발 일지/Feature 출처와 실제 Git 브랜치를 제공|node --test Studio/tests/*.test.mjs|Studio 서버/파서|
|S2 작업 화면|Studio/public|현황, 일정, 개선·수정, 개발 기록 조회 및 로컬 일정 편집|구문 검사와 브라우저 데스크톱/모바일 확인|Studio UI|
|S3 실행·검증·문서|실행기, 테스트, Feature/DevLog/Sprint 문서|재실행 가능한 로컬 서버, 파일 저장·재조회, 원문 열기, 오류 안내|HTTP/API 통합 검사, CheckGeneratedChanges.ps1|추가 도구/문서|

## 설계 계약

- 별도 설치가 필요 없는 Node 표준 라이브러리 기반 서버. 127.0.0.1에만 바인딩한다.
- 원문은 읽기 전용, 사용자 작업은 Studio/data/tasks.json에 원자적으로 저장한다. 문서 기반 상태는 과거 기록이며 현재 런타임 보장을 의미하지 않는다.
- 파일 접근은 DevLog/*.txt, Feature/architect/*.md, Feature/doc/*.md 및 SprintChangeLog 허용 목록으로 제한한다. 임의 경로·심볼릭 링크 탈출·외부 Origin 쓰기를 차단한다.
- 날짜 미정은 별도 표시, 완료율은 사용자가 등록한 작업에만 적용한다. Sprint 문서의 날짜는 기록일이며 계획 마감일로 재해석하지 않는다.
- 원문 텍스트는 textContent로 표시한다. 입력 검증, JSON 크기 제한, 동시 저장 충돌 감지와 손상 파일 보존, 로컬 백업 다운로드를 제공한다.
- 디자인: 검은 남색 작업실, 라임색 핵심 동작, 한국어 중심의 밀도 있는 편집 화면. 불필요한 장식 이미지는 사용하지 않는다.

## Reviewer / Exit Gate

역사적 검증을 현재 빌드 성공으로 표시하지 않는지, 날짜 미정과 과거 기록을 구별하는지, 기존 게임 파일 및 사용자 작업을 보존하는지 확인한다. 커밋/푸시 없음.
