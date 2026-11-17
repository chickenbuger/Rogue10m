# Native51 내부 Windows Smoke — Cook Fix F

**UNSEALED / NOT_EXECUTED.** 이 폴더는 UBT 컴파일 ABI 문제를 수정한 별도 도구 파생본입니다. 실제 빌드·Cook·패키징·Game 실행은 하지 않았습니다. UI7·시각·물리 플레이·배포 수락은 UNVERIFIED이며 `release_eligible=false`입니다.

## 변경

- 원본 B 패킷의 manifest 대상 32개와 manifest 자체 1개, 총 33개를 바이트 일치 복사한 뒤 파생했습니다. 원본 B와 D9/E helper는 수정하지 않았습니다.
- `strict_primitives.argument_plan`의 `-UbtArgs`에서 컴파일용 `-NoLiveCoding` **한 개만 제거**했습니다. `-NoHotReload`, `-Rebuild`, 실행기·동시성·격리 설정은 유지했습니다.
- postflight는 전용 Game UBT 로그의 유일한 실제 `Command line:`에 `-NoLiveCoding`이 있으면 거부합니다. 대소문자·따옴표·`=value` 형태도 거부합니다. 전체 BCR/Cook 로그에는 이 금지를 적용하지 않습니다.
- Cooker와 IoStore 실행의 `-NoLiveCoding`은 그대로입니다. Native51 validator/policy 및 PowerShell owner dispatcher는 원본과 바이트 일치합니다. same-PS identity·소유 lease·보호 Save·전체 입력·Editor/DLL·새 6개 출력·Native51·stage/archive·IoStore 검사를 완화하지 않았습니다.
- core의 helper pin과 derivative hash 검사를 FixF helper에 맞췄습니다.

## 오프라인 결과

- 기존 47개와 신규 2개: 최종 **49/49 통과**, 실패/오류/skip 0 (`results/FixF03`).
- 첫 실행은 49개 중 신규 양성 테스트의 옵션 접두어 파싱 오류 1개로 실패했습니다. 테스트 파싱만 고쳤고 `results/FixF01`을 보존했습니다. 실제 UBT override 주입 차단 검사는 첫 실행부터 통과했습니다.
- 독립 리뷰에서 지적한 Windows quote concatenation 누락을 보수적 quote 제거 검사로 보완했습니다. 최종 독립 리뷰 Critical/Medium 0건이며, 중간 49/49 통과 회차 `results/FixF02`도 보존했습니다.
- Python 8파일 AST 파싱 통과. PowerShell dispatcher AST parse 오류 0, termination 호출 0. dispatcher 자체는 실행하지 않았습니다.
- 새 두 회귀는 (1) UBT override 없음·Rebuild/NoHotReload 유지·Cooker/IoStore 플래그 유지, (2) 실제 Game UBT 명령에 override 여섯 형태를 주입하면 차단됨을 확인합니다.

`BASELINE-COPY.json`은 복사 직후 33개 pin입니다. `FIXF-CHANGES.json`과 `FIXF-DIFF.patch`는 전후 변경 근거이고, **현재 파생본 파일 검증에는 `FIXF-ARTIFACTS.sha256.json`을 사용**합니다. 복사된 기존 `ARTIFACTS.sha256.json`, PLAN/REVIEW/checkpoint 문서와 Final01/retained audit 결과는 원본 패킷의 역사 자료입니다. FixF 결과로 재해석하지 않습니다.

## Root가 별도로 수행할 일

1. 이 파생본 helper/core/runner/native validator/policy의 현재 pin으로 새 capture·seal을 생성합니다. 기존 B/B2/E5 seal/receipt는 수정·재사용하지 않습니다. Source/Config 368, 기존 Native51B Editor DLL CA647… 및 Content 9526·생성 cache 1개·전체 10466은 예상 전제이며 새 capture에서 실제 일치를 확인해야 합니다.
2. Root가 지정한 새 C 출력의 evidence/archive/cook/stage/cook_user/qa_user 6개를 모두 새 sibling으로 사용합니다. 보호 원본·Save·이전 실패 패키지·E5는 보존합니다. owner release는 기존 OWNER-RUNBOOK의 정확한 동일 PS7 PID/start/exe/SHA 및 만료·lease 계약을 그대로 따릅니다.
3. 기존 Native51B raw evidence/build/launcher/source/DLL pin, 필요한 6개 map과 새 DA의 정확한 IoStore identity를 새 봉인에 연결합니다. 실제 현재 입력과의 일치 없이는 진행하지 않습니다. UI7 수락이나 새 Native51 실행 결과를 만들지 않습니다.
4. 새 Game 빌드 후 실제 compile definitions의 WITH_LIVE_CODING/WITH_RELOAD 및 생성 배열과 엔진 reader의 stride가 일치하는지 별도로 확인합니다. postflight의 금지 옵션 부재만으로 ABI 정합이나 startup 성공이 증명되지는 않습니다. Cook/runtime의 동명 플래그를 UBT compile 옵션과 혼동하지 않습니다.
5. 실제 Windows Game QA는 Root의 별도 실행 범위입니다. 본 도구 결과는 정적 초기화 수정의 실증이나 production 수락이 아닙니다. 거절된 root-startup 승인 및 metadata/clone QA는 계속 STOP이며 이 파생본에서 접근·생성·재시도하지 않았습니다.

기존 OWNER-RUNBOOK의 schema와 실행 절차는 유지됩니다. `verify_offline.py` 및 실제 dispatcher/capture/freeze/release는 이번 작업에서 실행하지 않았습니다.

합성 테스트만 재현하는 명령:

```powershell
& "D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" -I -B -X utf8 -m unittest discover -s "D:\Project\Rogue10m\Feature\implementation\native51-internal-smoke-cook-fix-f" -p "test_*.py" -v
```
