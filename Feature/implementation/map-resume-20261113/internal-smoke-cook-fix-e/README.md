# 비GUI 내부 Development smoke 패키지 도구

**아직 실행하지 않은 UNSEALED 준비물**이다. UI7과 시각 검수는 UNVERIFIED, release_eligible는 항상 false다. 현 체크포인트의 맵은1800×1800m·미축소이며 NPC 미배치·기본맵 미전환 상태다. 실제 UE/UBT/UAT/Cook/게임·Computer Use·GUI 입력은 이 Work에서 실행하지 않았다.

- `internal_smoke.py`: 전체 입력/맵 외부 패키지·Editor/DLL·native43·보호 저장·소유권 검증, 정규 Game Win64 Development/cook/IoStore/archive 인수와 사후 검증.
- `Invoke-InternalSmoke.ps1`: 정확한 소유 PowerShell release·새 lease/출력6개·phase별 재해시를 요구하는 별도 실행 경계. 기본은 계획만 출력한다.
- `native43_validation.py`, `native43-policy.json`: raw43 결과·실제 명령·동일 DLL/Source/Config·6 warning cases/12경고·순차 완료/queue/exit 검증.
- `strict_primitives.py`: 기존 CandidateWindows 검증기의 바이트 동일 사본. 순수 경로/해시/manifest/인수/패키지 사후 검증 함수만 재사용한다. 기존 strict/B/B2/ZIP은 수정하지 않았다.
- `OWNER-RUNBOOK.md`: 소유자가 캡처·검토·봉인 후 실행하는 순서와 필요한 필드.
- `results/native43-retained-a-preflight.json`: 실제 보관 증거를 읽기만 한 결과. 현재 Source/Config366·DLL180D…76D가 일치하지만 **시작 Error15줄의 승인된 검토가 없어 차단**된다.
- `startup-red-review.NOT_APPROVED.json`: 정확15행·로그 SHA·엔진 소스4개 pin을 보존한 미승인 자료다. 원인/의도는 확정하지 않았다.
- `results/Final02/validation-summary.json`: 최종 오프라인 결과. 이전 Final01은 중간 검증이다.

실행 전 남은 항목은 전체 최종 입력 캡처와 동결, 정확한 startup15 검토, 소유자 release다. 이 도구는 실제 UI 회귀7을 통과로 기록하거나 기존 strict release 게이트를 대체하지 않는다. 생성된 artifact에도 실제 UI·입력·시각·저장 routing·플레이/포털 검수의 미완료 상태가 남는다. 기존 native43에는 UserDir/Python/MCP/save-isolation 증거가 없어 해당 격리 보증으로 승격하지 않는다.

오프라인 확인은 `python -B -m unittest discover -v`로 수행할 수 있다. 합성 테스트의 fake 승인·DLL·맵은 실제 실행 증거가 아니며 모두 owned scratch에서 정리된다.
