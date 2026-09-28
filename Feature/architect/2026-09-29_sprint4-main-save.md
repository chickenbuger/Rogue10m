# Sprint4 누적 작업 main 저장

사용자가 2026-09-29 현재 작업을 Git main에 저장하도록 승인했다. 기존 누적 게임 코드·설정·에셋·제작 스크립트·기획·증거 및 Studio 소스를 현재 작업 브랜치에서 저장하고 develop → test → main으로 fast-forward 승격한다. 원격과 로컬 선행 관계를 확인했으며 강제 push는 사용하지 않는다.

| 패킷 | 목표 | 대상 | 완료 조건·검증 | 복구 경계 |
|---|---|---|---|---|
| G1 | 저장 범위 확정 | source/content/docs/studio 및 원본 입력 | 파일별 manifest, 생성·로컬 데이터 제외 | 작업 파일 삭제 없이 index만 조정 |
| G2 | 현재 상태 검증 | UE 및 Studio | BuildEditor, Node 구문·19 tests, CheckGeneratedChanges, staged diff | 실패 시 커밋 이전 상태 유지 |
| G3 | Git 저장 | 작업→develop→test→main | ff-only, atomic push, 원격 SHA 재확인 | 강제갱신 없이 실패 원인 기록 |

임시 촬영본·백업·도구 바이너리는 로컬에 보존한다. tmp에서 정식 제작 도구가 사용하는 HUD 원본2개 및 Martelo FBX/재생성 소스/명령3개만 저장한다. 외부 Downloads의 Boxing.fbx와 Martelo 2.fbx는 저장소 밖의 기존 입력이며 이번에 복사하지 않는다. Studio/data의 실행 토큰·사용자 일정·로그는 기존 제외 규칙을 유지한다.
