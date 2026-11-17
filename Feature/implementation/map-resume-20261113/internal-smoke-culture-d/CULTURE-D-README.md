# Culture-D: 정확한 18개 인수와 전역 red 0

이 문서가 Culture-D의 활성 정책이다. 함께 보존한 원본 README, OWNER-RUNBOOK, REVIEW, Final01/02, 모듈 상단 설명과 기존 테스트의 **15개 인수·startup review allowance는 upstream 계보이며 이 derivative에 적용하지 않는다.** startup 오류를 승인 파일로 허용하는 경로는 제거됐다. old A와 `startup-red-review.NOT_APPROVED.json`은 수정하거나 승격하지 않았다.

## 변경과 계보

- upstream `NonGuiSmoke20261113A.delta.zip`의 39파일을 이 새 물리 폴더에 복사했다. ZIP SHA256은 `BADA1140C01AD87C70509F9185C156569186586136071910D48AAAB03A65484F`다.
- `ARTIFACTS.sha256.json`은 upstream 원본 그대로이며 SHA256은 `934FF7704553859CDEBC56F7D1B5E5BFD6CEF734292B7A5B4A122A1BBFFFF349`다. 현재 derivative의 통과용 manifest로 바꾸지 않았다.
- 원래 39파일 중 바뀐 것은 `native43_validation.py` 하나다. `validate_native43()` 바깥의 텍스트와 AST는 동일하다. 현재 validator SHA256은 `985500591447274B367B97A89257D1CA992BA7891890D220608D7B13B383CF98`다.
- `native43-culture-d.patch`는 이 파일 하나의 upstream -> derivative unified diff다. 이미 수정된 이 폴더에 다시 적용하지 않는다.
- `UPSTREAM-PROVENANCE.json`은 원본/후보/patch 및 보존 범위를 기록한다. `DERIVATIVE.sha256.json`이 현재 파일 집합의 새 해시 manifest이며 자기 자신은 제외한다. 그 manifest의 외부 SHA는 부모에게 별도로 전달한다.

## 실제 Native43B 입력 계약

process/result의 같은 command 배열은 기존 0..14 항목을 그대로 보존하고 **정확히 18개**여야 한다. 마지막 세 항목과 순서는 다음과 같다.

```json
["-culture=en", "-language=en", "-locale=en-US"]
```

실제 `LogInit: Command Line:`에도 이 세 값이 같은 순서로 끝에 있어야 한다. record만 바꾸거나 culture 값을 생략·교환·변형·중복·추가하면 거부한다. 기존 exact project/engine, 43개 selector, 동일 DLL/Source/Config, report/PID, 6 warning cases/12 events, 순차 completion, queue43, zero-status exit 검사는 유지했다. smoke test 비활성화나 오류 필터는 추가하지 않았다.

첫 Test Started 전 startup red는 반드시 0이며 test/shutdown red 거부도 유지한다. RED 검출기의 Error/Fatal/ensure/traceback 범위는 줄이지 않았다. startup_review가 없거나 NOT_APPROVED거나 fake/approved envelope가 있어도 red를 허용하지 않는다. clean B에는 startup allowance용 review를 만들거나 넣을 필요가 없다.

부모의 기존 DLL English startup-only probe는 새 helper v3 빌드의 Native43B 증거가 아니다. 실제 새 Editor build와 같은 root/DLL/Source/Config의 fresh B43 전체 증거를 요구한다. old A의 경로·해시를 고쳐 B처럼 만들 수 없다. 현재 폴더는 실제 B를 검증하거나 패키징하지 않았다.

## smoke 실행 경계

`internal_smoke.py`, `Invoke-InternalSmoke.ps1`, `strict_primitives.py`, `native43-policy.json`, smoke/release schema는 바이트 그대로다. 새로운 seal 캡처 때 이 폴더의 validator/runner/helper/policy 경로와 현재 raw SHA를 pin해야 한다. 원래 capture, map/WP inventory, Editor6, 동일 native43, SaveGames 보호, 독립 새 출력6개, 같은 PowerShell 7 소유 세션 release, 실행 창과 UAT mutex 요건은 유지된다.

기본 wrapper는 plan-only이고 root만 -Execute로 SDK/Game BuildCookRun을 수행한다. IoStore listing은 원래처럼 별도 소유 CLI 단계다. GUI·마우스·키보드·그 입력 API 우회, 게임 실행, 실제 시각/포털/저장 routing 검증을 이 derivative가 수행하지 않는다. UI7/visual_review는 UNVERIFIED, release_eligible=false다. B/B2/strict release와 기존 ZIP, 공유 Source/Config/Content/Git는 변경하지 않았다.

## 오프라인 반례

명시적인 derivative 테스트만 실행한다. 보존한 upstream `verify_offline.py`는 원래 manifest를 검사하므로 현재 validator 차이를 거부하는 것이 정상이다. 기존 `unittest discover`의 old33 통과를 이 derivative의 결과라고 주장하지 않는다. 이 테스트는 기존 synthetic fixture 생성기만 재사용하며 실제 UE·프로세스·MCP를 호출하지 않는다.

```powershell
& 'D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' -I -B -X utf8 .\test_native43_culture_d.py --evidence-root '<이 staging의 evidence 아래 새 절대경로>'
```

최종 결과는 `evidence/offline-run03/validation.json`과 `.log`다. **17 tests + 19 parameterized subcases, 실패/오류 0**이다. run01/02와 모든 합성 fixture도 보존했다. 정확한 clean18 positive, upstream에서 허용되던 old15 거부, 18+matching fake approval+startup15 거부, tail/LogInit 불일치, 기존 DLL/Source/selector/43 ID/warning/queue/exit 보호 및 evidence 경로 이탈 거부를 검사했다. 이 결과는 실제 B43 또는 UAT 성공 증거가 아니다.

독립 읽기 리뷰에서도 production delta 보존과 테스트의 실제 gate 도달을 확인했다. 테스트 harness evidence 경로 guard 권고는 보완하고 run03으로 재검증했다. 이번 작업의 UE/UBT/UAT/MCP/Cook/Game/GUI 실행은 0회다.
