# 첫 빌드의 동시 편집 링크 실패

- 실행: `Scripts/BuildEditor.ps1`, 실제 원문 `tmp/CharacterLobbyStyle20261119/BuildA/editor-build.log`.
- 시작: 2026-11-18 16:58:01 UTC. 종료: 17:01:01 UTC. 종료 코드 6, 실행 178.23초.
- 선택창 C++·종료 위젯·새 QA driver는 각각 정상 컴파일됐다. 첫 UHT는 생성 파일 0개 갱신이었다.
- 중요한 실패: `LNK2019` — `URogue10mRunHUD::GetPrototypeMinimapMarkers`의 오래된 reflected thunk가 구현을 찾지 못했다. 최종 `LNK1120` unresolved symbol 1개.
- 독립 조사: 다른 작업이 `Rogue10mRunHUD.h`의 해당 선언을 16:59:56 UTC, `.cpp` 구현을 16:59:57 UTC에 제거했다. 빌드의 UHT 스캔 뒤 네이티브 컴파일 전에 소스가 바뀌어 이전 thunk와 새 구현이 어긋난 시점이 확인됐다.
- 대응: 다른 작업의 소스를 되돌리거나 제거한 API를 복원하지 않았다. `Intermediate`를 직접 수정하지 않았다. 변경이 반영된 소스에서 정상 빌드를 다시 실행해 UHT 재생성을 요청했다.
- 함께 보인 `git status` 자식 타임아웃 및 XGE 라이선스 경고는 별도 빌드 환경 메시지다. 실제 실패 원인은 위 링크 오류이며 이를 시안 파일의 컴파일 실패나 런타임 검증 통과로 바꿔 기록하지 않는다.

이 기록은 첫 실행의 실패로 보존한다. 이후 빌드 결과는 별도 최종 검증 기록을 따른다.
