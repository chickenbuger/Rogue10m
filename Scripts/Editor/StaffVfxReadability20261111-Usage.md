# Staff Primary01 가독성 후보 적용 절차

이 구현은 격리 작업 사본에서 준비되었다. 공유 Unreal Editor, 빌드와 `.uasset`를 변경하거나 실행하지 않았다. `StageStaffVfxReadability20261111.py`는 기본 `prepare`에서 오프라인 계획만 기록한다. 실제 프로젝트에 C++/스크립트를 통합하고 Editor target을 빌드한 뒤, 담당자가 독점 에디터 슬롯을 확보해야 live 단계가 가능하다.

## 범위

- 기존 Primary01을 별도 `ReadabilityCandidates20261111/<label>/DA_Attack_Staff_Primary01_Readability_Candidate`로 복제하고 4개 3인칭 표시 필드만 조정한다.
- cast socket-local offset `(25,0,20)`cm는 손 본 축과 Staff tip을 아직 검증하지 않은 후보 수치다. impact는 시전자 쪽 타깃 XY bounds 표면에 4cm를 더한다. 정확한 충돌 접점이라는 뜻은 아니다.
- 기존 공통 scale=1, dilation=1, cast/impact destroy duration=.35/.27초는 전제 조건으로만 검사하며 수정하지 않는다. 유한 particle 수명 .30/.22초는 유지된다. timer 연장만으로 파티클 수명이 늘어나지 않는다.
- 기존 Niagara 원본, montage/play rate, HitStartDelay, 비용·피해·차지·1인칭 설정과 나머지 Staff 4개 스킬, 프로필, 캐릭터는 보존한다.
- `activate`는 후보를 새로 bind하지 않고 기존 Primary01의 4개 표시 필드만 복사한다. 기존 8-field Magic binding report 계보로 성공을 주장하지 않는다.

## 단계

`--label`은 후보 정체성이므로 계속 같게 두고, `--evidence-label`은 단계마다 새 값으로 둔다. 예시는 명령 구조이며 아래 live 단계는 이 작업에서 실행하지 않았다.

```powershell
$python = 'D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$script = 'Scripts/Editor/StageStaffVfxReadability20261111.py'
& $python -B $script --phase prepare --label StaffReadableA --evidence-label StaffReadablePrepA
& $python -B $script --phase inspect --label StaffReadableA --evidence-label StaffReadableInspectA --lease-granted
```

`inspect` 보고서의 project skills, 전체 reflected properties와 diff를 검토한다. 새 C++ 필드가 없거나 기존 shared tuning/binding이 다르면 중지한다. 지정 MCP port의 listener가 직접 `UnrealEditor.exe`이고 그 프로세스의 절대 `.uproject` 경로가 현재 스크립트 프로젝트와 같은지 확인한다. 같은 GUID의 다른 체크아웃이나 프록시는 자동 추정하지 않는다.

```powershell
$inspect = 'Feature/doc/evidence/staff-vfx-readability-20261111/StaffReadableInspectA/summary.json'
$inspectSha = (Get-FileHash -LiteralPath $inspect -Algorithm SHA256).Hash.ToLowerInvariant()
& $python -B $script --phase stage --label StaffReadableA --evidence-label StaffReadableStageA --lease-granted --snapshot $inspect --snapshot-sha256 $inspectSha
$stage = 'Feature/doc/evidence/staff-vfx-readability-20261111/StaffReadableStageA/summary.json'
$stageSha = (Get-FileHash -LiteralPath $stage -Algorithm SHA256).Hash.ToLowerInvariant()
& $python -B $script --phase activate --label StaffReadableA --evidence-label StaffReadableActivateA --lease-granted --snapshot $inspect --snapshot-sha256 $inspectSha --stage-report $stage --stage-report-sha256 $stageSha
```

activation은 시각 승인 상태가 아니다. 전후 전체 속성의 4-field audit와 보호 파일 hash를 기록하고, 활성/후보 DataAsset 디스크 hash를 고정한다. no-op activation도 clean saved artifact hash를 기록한다. 실패 또는 불명확한 mutation 결과는 자동 재시도·자동 rollback하지 않으며 먼저 읽기 전용으로 상태를 확인한다.

```powershell
$activation = 'Feature/doc/evidence/staff-vfx-readability-20261111/StaffReadableActivateA/summary.json'
$activationSha = (Get-FileHash -LiteralPath $activation -Algorithm SHA256).Hash.ToLowerInvariant()
& $python -B $script --phase verify --label StaffReadableA --evidence-label StaffReadableReadbackA --lease-granted --snapshot $inspect --snapshot-sha256 $inspectSha --activation-report $activation --activation-report-sha256 $activationSha
```

`verify`는 읽기 전용이다. 신규 직접 UE listener PID와 이전 PID 종료가 둘 다 증명될 때만 `cold_reload_validated=true`로 기록한다. 같은 세션은 false다. `--require-cold-reload`를 더하면 restart 증명 없는 검증은 실패한다. 이 스크립트는 Editor를 재시작하거나 빌드하지 않는다.

새 runtime launcher 후보 모드는 `-ReadabilityCandidate`를 명시해야 하며, 기존 AppliedReport/ColdReadbackReport 계보 gate와 별개다. 후보 보고서의 `readability_fingerprints(plan)`는 기존 범위에 placement helper, placement tests, runtime fixture, recipe와 launcher 소스 6개를 추가로 고정한다. 변경 후 다시 만든 보고서가 필요하다.

## 남은 실제 검증

C++ 빌드/UHT, native schema 확인, stage/save/readback, cold restart, 기본 3인칭과 진단 측면 뷰에서 다중 Niagara 나이 캡처, 손/Staff 궤적과 준비/피해/회복 증거, 물리 입력·일반 플레이는 아직 수행하지 않았다. 재촬영에서 이른 캡처와 몸체 가림을 나눠 검사해야 한다. 모든 성공 보고서에도 `runtime_gameplay_validated=false`, `visual_quality_approved=false`를 유지한다.
