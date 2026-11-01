# 로비 영구 강화 카드의 조건·재화 표시 정합성

작업 경로 `D:\Project\Rogue10m`, 브랜치 `Sprint#9-20-lobby-trait-display`, 기준 커밋 `9578fa52`. 2026-11-01 UTC 검증 기록이다. 최종 로컬 커밋은 이 문서의 Git 이력에서 확인한다. push·병합은 하지 않는다.

## 변경

강화 카드의 레벨 잠금을 현재 판 레벨 대신 기존 backend와 같은 최고 런 레벨 기록으로 표시한다. 로비 영구 강화 재화와 판 내 성장 포인트의 라벨·부족 사유를 구분한다.

- `Source/Rogue10m/UI/Widgets/Rogue10mTraitWidget.cpp`: 카드 잠금·예산 표시 정합성.
- `Source/Rogue10m/World/Rogue10mTutorialNPC.cpp`: 로비 재화 안내 라벨.
- `Source/Rogue10m/Components/Rogue10mTownProgressionComponent.cpp`: `NoPoints` 현지화 오류 문구만 로비 재화로 정정.

가격, 실제 구매 조건, 경제, 저장 형식, 원본 자산은 바꾸지 않았다. 새로운 영구 강화 규칙을 추가한 작업이 아니다.

## 검증

제품 빌드28.53초 성공. 새 테스트의 첫 빌드는0 action/up-to-date여서 새 코드 컴파일 증거로 세지 않았다. `-gather` 뒤 실제 테스트 컴파일9.79초 성공, 최종 부족 사유·테스트 반영 빌드24.24초 성공.

독립 UI fixture A36검사/실패0을 보존했다. 최종 B는38검사/실패0이며, 프로필 미선택·미부착 상태의 격리 fixture에서 다음 표시와 버튼 상태·backend 판정이 일치했다.

| 현재 레벨 | 최고 런 레벨 | 로비 예산 | 대상 카드 결과 |
|---:|---:|---:|---|
| 1 | 20 | 20 | 허용 상태 |
| 1 | 1 | 20 | 레벨 조건 잠금 |
| 1 | 20 | 0 | 로비 재화 부족 |

temporary seed·XP1500은 **fixture 설정**이며 정상 플레이 성장 결과가 아니다. fixture 상태를 원상 복원했고 테스트에서 만든 NPC 위젯만 정리했다. 구매·`SaveNow`는 호출하지 않았다. B 프로세스 PID41880은 정상 exit0으로 종료됐고, 해당 실행의 세이브340개 before/after SHA가 동일했다.

최종 PNG3장을 직접 열어 검토했다. `03_highest20_budget0.png`의 부족 사유가 판 내 성장 포인트가 아닌 로비 재화로 표시되는 것을 확인했다. 원본 증거 위치는 `DevLog/LobbyTraitDisplayQA/LobbyDisplay1101B/Driver/screenshots/`다. 기능별 사본은 DevLog/20261101_LobbyTraitDisplay_Review에 보존했다. 세 사진 모두1280×720 디코딩과 SHA256 검증을 통과했다.

`RunCurrencyBaseline_Lobby1101Final`은6검사/실패0/경고0/미실행0으로 통과했다. 보고서는 `Feature/doc/evidence/lobby-trait-display-20261101/`에 있다. wrapper의 성공 코드0 누락 때문에 자동 연쇄가 실행되지 않은 부분은 보존 후 수정했고, 위6개는 별도로 실행해 확인한 결과다. 연쇄가 처음부터 성공했다고 기록하지 않는다.

## 보존과 한계

모든 작업 종료 뒤 독립 대조에서도 기존340개 세이브, 원본31개 자산 및 OpenRunMap.umap의 SHA256이 전부 동일했다. 새 저장 파일은 생성되지 않았다. 상세 해시와 사진 기록은 DevLog/20261101_LobbyTraitDisplay_Review/VERIFICATION.json에 있다.

실제 구매·저장, 다음 판 강화 적용, 승리, 사람의 물리 입력·일반 한 판 완주는 이번 묶음에서 검증하지 않았다. 옛9포인트 `TraitUI` fixture도 실행하지 않았다. 자동 UI fixture·렌더 사진 검토와 정상 구매·필드 플레이를 구분한다. 새 애니메이션·에셋·모션 타이밍 변경은 없다.
