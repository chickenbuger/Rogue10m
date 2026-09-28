# Sprint4 main 저장 검증

2026-09-29 사용자 요청으로 현재 누적 작업 저장을 준비했다. 개별 변경 파일과 설명은 `evidence/main-save-20260929/changed-files.tsv`에 기록한다.

- Unreal Editor 빌드 성공(최신 상태, 2.14초).
- Studio 6개 JS 구문 검사 및 루트 실행 Node 테스트19건 통과.
- Studio/data, 엔진 생성물, 로컬 tmp 출력은 제외. 정식 제작에 필요한 tmp 입력5개만 예외 포함.
- 이전 head shadow 검증390프레임, V 회귀280프레임 결과도 함께 보존.
- git fetch 후 develop/test/main 원격과 로컬 일치 및 작업 브랜치 조상 관계 확인.

현재 게임·Studio 검증 범위는 각 Feature 문서의 한계를 따른다. 이번 저장은 모든 기능의 신규 전체 QA를 의미하지 않는다. Studio 테스트에는 cwd 의존이 있어 프로젝트 루트에서 `node --test Studio/tests/*.test.mjs`를 사용했다. 외부 Downloads의 FBX 입력은 기존 로컬 의존이며 저장소에 포함하지 않는다.

## Git 승격 결과

누적 작업 커밋은 `8c9bb8a`(1,332개 파일)이며 develop → test → main의 로컬 fast-forward 병합을 완료했다. 종료 기록은 후속 문서 커밋에 담는다. Sprint4 종료로 다음 신규 개발 번호는 Sprint5-1이다.

원격 `https://github.com/chickenbuger/Rogue10m.git` 푸시는 자동 승인 검토에서 특정 목적지와 1,332개 파일의 전송 승인이 필요하다는 사유로 거절되었다. 푸시는 실행하지 않았으며 별도 사용자 승인을 요청한다. Studio/data와 캐시·임시 출력은 커밋에 포함하지 않았다.

추가 변경 파일: `Docs/SprintChangeLog.md`는 Sprint 종료, `DevLog/20260929.txt`는 당일 반영 결과, 이 문서는 저장 결과를 기록한다.


## 원격 저장 완료

후속 사용자 메시지 「푸시해」로 전송 대상과 범위가 명시 승인됐다. `git push --atomic origin develop test main`이 성공하여 세 원격 브랜치가 모두 `aa941d4`로 갱신됐다. 앞선 원격 대기 상태는 해소됐다. 이 완료 기록을 후속 문서 커밋으로 같은 브랜치 흐름에 반영하고 최종 로컬/원격 SHA를 대조한다. 강제 갱신은 사용하지 않았으며 제품 코드는 추가 수정하지 않았다.
