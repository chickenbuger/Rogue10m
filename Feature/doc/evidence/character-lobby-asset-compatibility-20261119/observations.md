# 작업 중 확인한 실패와 대응

- 첫 스켈레톤 조사에서 Python에 공개되지 않은 `preview_skeletal_mesh` 조회가 기본 `Exception`을 발생시켰다. 로그 17줄을 남겼고 이 실행은 저장 전에 종료됐다. 선택적 진단 필드의 예외를 처리한 다음 여섯 파일을 백업하고 저장했다.
- 첫 정규 빌드는 별도 체크아웃이 열어 둔 Live Coding의 공용 실행 파일 mutex 때문에 멈췄다. 작업 프로젝트의 에디터가 없음을 확인하고 `-NoHotReload -NoHotReloadFromIDE`로 해당 체크아웃의 DLL을 빌드했다. 다른 세션을 종료하지 않았다.
- 첫 게임 실행 `AssetFix1119A`는 기능 238개를 통과하고 기존 Humanoid 오류를 재현하지 않았지만, 드워프 얼굴의 회색 원본과 셰이더 준비 중 캡처된 회색 프리뷰가 화면에 남았다. 후속 `AssetFix1119B`의 세 슬롯 캡처에서 색 보정을 확인했고, 생성 화면 첫 프리뷰는 셰이더 완료 후 다시 그려야 했다.
- 중간 원문 로그와 화면은 `tmp/CharacterAssetRepair20261119`, `tmp/CharacterLobbyStyle20261119`, `tmp/CharacterLobbyStyleQA`의 각 실행 폴더에 남겼다. 최종 결과의 대표 화면과 상태를 보여 주는 생성 화면만 이 작업의 근거로 선별한다.
