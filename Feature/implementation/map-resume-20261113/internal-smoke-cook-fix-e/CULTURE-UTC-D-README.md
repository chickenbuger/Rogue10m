# Culture-UTC-D: 소유 release의 UTC 문자열 보존

이 파생본은 Culture-D의 **18개 exact args + 전역 red 0** 정책을 유지한다. 함께 보존한 upstream README/OWNER-RUNBOOK/REVIEW의 15arg·startup allowance는 계보 자료이며 활성 정책이 아니다. old A15와 NOT_APPROVED review를 통과시키지 않는다.

## 단일 계약 수정

Culture-D runtime 39파일을 새 물리 폴더에 복사한 뒤 `Invoke-InternalSmoke.ps1` 두 줄만 바꿨다.

1. `#requires -Version 7.2` -> `#requires -Version 7.5`.
2. `Read-PinnedJson`의 `ConvertFrom-Json -AsHashtable`에 `-DateKind String` 추가.

기존 parser가 ISO UTC 문자열을 DateTime으로 바꾸면 `[string]` 변환에서 Z와 소수초가 사라졌다. 그 문자열을 DateTimeOffset으로 다시 읽을 때 KST +09로 해석해 실제 활성 UTC 창을 만료로 오판했다. 새 parser는 원래 Z/offset/소수초 문자열을 그대로 보존한다. Assert-OwnerRelease의 시간창·scope·PID/start ticks/exe/SHA·seal·owner 비교는 변경하지 않았다.

동결 wrapper SHA256은 `32A95E82FB7C68485FD90086D0F185E583BAC86A9B28287B2E69F1367C4CFBD9`다. native validator는 Culture-D의 `985500591447274B367B97A89257D1CA992BA7891890D220608D7B13B383CF98`, strict helper는 `02DF5B26A566EBF528280B3DEAB1E97081E9CD7B3AAE6B47A8277C8D6A029980` 그대로다. scope/UI7/visual_review/release_eligible=false, immutable 재검사, lease와 UAT 진입 경계도 그대로다.

이전 Culture-D, sealed 실패 근거, upstream/B/B2/strict release는 수정하지 않았다. root는 이 새 경로의 wrapper pin으로 새 capture/seal/정확한 live PowerShell release를 발급해야 한다. 이전 release의 PID나 유효기간을 복사하거나 가드를 생략하지 않는다. 이 작업은 실제 wrapper 본문·UE·UBT·UAT·Cook·Game·MCP·GUI를 실행하지 않았다.

## 순수 PowerShell 반례

`Test-OwnerReleaseUtc.ps1`은 동결 wrapper SHA를 확인하고 AST에서 다섯 함수만 추출한다. 전체 wrapper를 dot-source하거나 본문/UAT를 실행하지 않는다.

```powershell
& .\Test-OwnerReleaseUtc.ps1 -EvidenceRoot '<이 staging의 evidence 아래 새 절대경로>'
```

최종 결과: `evidence/utc-run01/validation.json`, raw log: **staging 루트 `utc-run01.log`**. PS7.6.9/Korea Standard Time(+09)에서 **34 checks PASS**다.

- 실제 JSON parse에서 UTC 값이 string이며 Z/7자리 소수초/ticks/offset 0을 보존한다.
- 현재 실제 PowerShell PID identity와 UTC now ±10분 창을 수락한다.
- en-US/ko-KR/de-DE thread culture에서 같은 결과를 확인하고 원래 culture를 복원했다.
- expired/future-issued/reversed window, wrong/extra scope, PID/start ticks/exe/SHA/owner/seal/project/schema/status는 계속 거부한다.
- 이전 자동 DateTime -> 문자열 -> +09 해석으로 활성 창을 거부하는 오판도 같은 세션에서 재현했다.

실측 PowerShell은 `C:\Users\PC\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe`, 버전 7.6.9, SHA256 `2A6769A3330DC1084D0154BFDE7D164AB750CA7F700F313F9D417C1BE78B2233`다. 생성된 release JSON은 SYNTHETIC으로 표시한 함수 시험 자료이며 실제 실행 release가 아니다. 테스트는 native43 validator도 실행하지 않았다.

독립 읽기 리뷰는 wrapper 두 줄의 정확한 변경과 반례/결과 모두 PASS, 새 Critical/blocker 없음으로 판정했다. 실제 SDK/Game/cook/IoStore/소유 세션 end-to-end 실행 성공은 이 결과로 주장하지 않는다.

## 해시와 원본 계보

`ARTIFACTS.sha256.json`은 원래 upstream manifest 그대로다. 현재 runtime의 통과용 manifest로 고치지 않았다. `UPSTREAM-CULTURE-PROVENANCE.json`이 Culture-D 기준 파일과 새 wrapper delta를 구분한다. `wrapper-utc-d.patch`는 wrapper 한 파일의 두 줄 diff다. `DERIVATIVE.sha256.json`은 현재 파생본의 파일 집합이며 자기 자신을 제외하고 외부 SHA를 별도로 전달한다.

부모가 추가로 지정한 Landscape 경계도 유지한다: CollapseAllLayers 금지, raw/composite 분리, heightmap-only 보존으로 대체 금지. 이 작업은 Landscape·Source·에셋을 변경하지 않았다.
