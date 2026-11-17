# 정산 UI 실제 렌더링 증거
2026-10-29 UTC, D:\Project\Rogue10m, Sprint#9-8-result-lobby-ui, 기준7891e1b2.
라벨UINormalD1029, assertion 118 PASS. 실제UE화면1280×720, 프로그램콘솔/API및상태fixture검증이며수동플레이/최종보스처치증거가아니다.
재현 Scripts/VerifyRunResultUI.ps1 -Label <fresh> -Mode <해당모드> -Rendered.
01초기정산(실패모드는저장대기),02반복요청,03저장완료. AbandonGain의04는실제허브귀환후초기화와기준재화유지화면.
파일은원본픽셀그대로이며재화+12/사용가능110,cap+5,포기+0/98/70골드5크리스탈등실제검사값을보인다. 모든PNG디코딩확인. 추가설명은Feature/doc/2026-10-29_result-lobby-ui.md. 미디어는Git미추적보존.
