
### 비GUI 내부 Development smoke 실행 준비

- 목표: GUI 없이 정규 Windows Game/cook/IoStore/archive artifact를 준비하는 별도 소유자 실행 경계.
- 변경: internal_smoke.py, Invoke-InternalSmoke.ps1, native43 raw 검증기/정책, UNSEALED 초안, 합성 반례·소유자 runbook. 기존 strict/B/B2/ZIP은 보존.
- 검증: 정확한 최종 오프라인 결과는 results/Final02/validation-summary.json. 실제 Source/Config366·DLL 일치 읽기 확인, Native43 시작 Error15는 NOT_APPROVED로 차단. 실UE/빌드/GUI0회.
- 상태: 도구 준비·실제 smoke UNSEALED/NOT_EXECUTED. UI7/시각 UNVERIFIED, release_eligible=false. 맵1800m 미축소·NPC 미배치·기본맵 미전환.
- 남음: 최종 전체 입력 동결/캡처, startup15 기술 검토, 통합 소유 세션 release와 실제 UAT/IoStore 인수.
- 관련: NonGuiSmoke20261113A/README.md, OWNER-RUNBOOK.md, results/native43-retained-a-preflight.json. 공유 문서에는 직접 적용하지 않음.
