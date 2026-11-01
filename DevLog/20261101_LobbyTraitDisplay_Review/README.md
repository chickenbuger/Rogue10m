# 로비 강화 카드 표시 검증

2026-11-01 UTC, `D:\Project\Rogue10m`, `Sprint#9-20-lobby-trait-display`, 기준 `9578fa52`. 로비 재화와 판 내 성장 포인트를 구분하고, 카드 레벨 잠금을 기존 backend와 같은 최고 런 레벨 기록으로 표시했다. 가격·구매 조건·경제·저장·원본 자산은 변경하지 않았다.

최종 `LobbyDisplay1101B`는38검사/실패0이다. 현재LV1에서 최고20·예산20 허용, 최고1·예산20 잠금, 최고20·예산0 부족 표시를 카드·버튼·backend와 대조했다. 프로필 미선택·미부착 격리 fixture이며 temporary seed/XP1500은 원상 복원했다. 구매나 `SaveNow`는 하지 않았다. 실제 물리 입력 검증이 아니다.

원본 사진은 다음과 같다. 3장 모두 직접 열어 검토했으며,3번의 부족 사유가 로비 재화로 정정됐음을 확인했다. 이 폴더에 기능별 사본3개와 VERIFICATION.json을 보존했고, 모두1280×720 디코딩 및 SHA256 검증을 통과했다.

- [최고20·예산20](01_highest20_budget20.png)
- [최고1·예산20](02_highest1_budget20.png)
- [최고20·예산0](03_highest20_budget0.png)

재현: 프로젝트 PowerShell에서 `Scripts/VerifyLobbyTraitDisplay.ps1 -Label <새 영숫자·밑줄20자이내>`를 실행하고 위 세 상태의 실제 카드·텍스트·버튼·backend 결과를 대조한다. 사용자 프로필에 seed/XP fixture를 적용하거나 기존 label의 증거를 덮어쓰지 않는다. 다른 UE 실행을 먼저 조율하고 실행기의 격리·보존 gate를 따른다.

빌드: 제품28.53초, `-gather` 뒤 테스트 실제 컴파일9.79초, 최종24.24초 성공. 첫0 action 빌드는 새 테스트 컴파일로 세지 않는다. A36/0은 보존하고 최종 B38/0과 구분한다. 재화 회귀 `RunCurrencyBaseline_Lobby1101Final`6/0·경고0·미실행0 통과. wrapper 성공 코드 누락으로 처음 자동 연쇄가 실행되지 않은 기록을 보존했고, 수정 뒤6개를 별도 실행했다.

B PID41880 정상 exit0. B 실행 전후 세이브340개 SHA 동일. 전체 작업 후 독립 검사에서도 세이브340개·원본31개 자산·맵 SHA256이 전부 동일했다. 실제 구매·저장/다음 판 강화/승리/물리 완주 및 옛9포인트 TraitUI fixture는 미검증이다. 새 애니메이션·에셋·원격 push·병합은 없다.

상세: `Feature/doc/2026-11-01_lobby-trait-display.md`.
