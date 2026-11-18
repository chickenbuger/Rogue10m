# Win64 패키징 및 지상형 보스 권한 확인

51f2def0의 보스·HUD·전리품 검증 기반을 보존한다. 현재 설치된 Cactus/ThornWarden·TurtleShell/IronShell의 라이브러리·라이선스 근거를 확인하고, 확정 전에는 에셋을 취득·설치·교체하지 않는다. 권한 확인은 별도 읽기 전용 조사로 진행하며 독립적인 패키징 검증을 수행한다.

## 범위와 완료 조건

- UE5.8.4의 Game 타깃 Rogue10m Win64 Development를 빌드한다. 이 프로젝트에는 별도 Client 타깃이 없으므로 -client를 사용하지 않는다.
- L_Menu/StartHub/OpenRunMap을 명시하여 BuildCookRun으로 cook/stage/pak/archive를 수행한다. 새 task/packaging-qa/Package1103A 아래 cook·stage·archive를 분리한다. 기존 Saved/Cooked·StagedBuilds 또는 Build/Windows 결과를 덮거나 -clean으로 지우지 않는다.
- 실제 에러가 있으면 소스·설정·cook 참조 문제를 구분하고 최소 수정 후 관련 검증을 수행한다. vendor 원본을 수정하거나 실패를 경고 무시로 숨기지 않는다.
- 완성된 exe를 격리된 -UserDir 아래에서 실행하고 실제 기본 메뉴·맵·로그·필수 에셋을 확인한다. 패키지의 profile 슬롯 override는 WITH_EDITOR 전용이라 무시될 수 있으므로 세 QA 슬롯 인자만을 저장 격리 근거로 사용하지 않는다. 기본 프로필 생성 전 실제 격리 경로를 확인한다.
- 프로젝트 저장386개 및 기존 packaged 사용자 저장을 시작 전후 SHA로 비교한다. 사용자 Nov1 문서 WIP와 엔진·vendor 에셋을 보존한다.
- cook/package 성공, cold start·화면 검증, 실제20분 플레이는 별도 상태로 보고한다. 기술적으로 확인되지 않은 항목을 성공으로 처리하지 않는다.

## 환경과 롤백

엔진은 D:/Program Files/UE_5.8, 프로젝트는 D:/Project/Rogue10m. 읽기 전용 용량 확인은 C162.76GiB/D1133.46GiB 여유였다. 출력은 새 task 디렉터리이며 재사용 시도는 먼저 기존 결과를 확인한다. 생성·캐시는 엔진 도구로만 작성한다. 코드 수정이 생기면 해당 변경만 검토·기록하며 기존 자료는 삭제하지 않는다.
