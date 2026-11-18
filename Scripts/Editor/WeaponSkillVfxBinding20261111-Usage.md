# 기존 무기 스킬별 VFX 연결 사용법

이번 도구는 기존 대검/너클 각5경로 중 이미 활성인2개를 제외한8개를 한 번에1개씩 다룬다. 새10개 selectable catalog나 스킬 input/unlock/profile binding을 변경하지 않는다. 기본 prepare는 오프라인이고 live 단계는 공유 담당자 사용권 인계 뒤 실행한다.

피해·전진·montage·Charge·1인칭·카메라·면역 필드는 변경하지 않는다. 꺼진 효과를 켜면 기존 Charge/legacy 효과까지 활성화될 수 있으므로 둘 다 null이어야 한다. 수명 잘림·스냅샷 누락을 타이머 수정이나 기본값 대입으로 우회하지 않는다.

효과·크기·수명·배속·local/world-space·색은 기존 DataAsset/Niagara/MIC에서 편집한다. 검토한 현재 값과 hash를 다시 제공하면 도구 코드를 바꾸지 않고 검사할 수 있다. 수명에 임의0.5초 상한이나 phase별 공간값 고정을 두지 않는다. 재질도 고정4개가 아닌 저장 readback의 참조와 parent chain을 수집해 스킬별 MaterialInstance를 지원한다. 프로젝트 외 parent 및 전체 material function/texture 의존성은 현재 인증 범위 밖이다. 수정 위치는 `Docs/EditableCombatVfxGuide.md`를 따른다.

## 새 파일 통합

별도 C++ 변경 없이 Scripts 도구를 추가할 수 있다. 원래 MCP helper3개(`BindMagicSkillVfxMCP20261111.py`, `InspectWeaponFantasyVFX20261110.py`, `AuthorWeaponSkillParticles20261110.py`) 및 `MagicSkillVfxBinding20261111.py`와 새 preflight/Staff owner helper가 필요하다. standalone bundle은 이 Work가 작성한 새 의존 파일도 포함한다. 이미 존재하는 파일은 덮어쓰지 말고 버전·변경을 비교한다.

현재 Source와 일치하는 UE reflection이 필요하다. 특히 profile의 `activeTechniqueCatalog`와 AttackSkillData 전체 필드가 없으면 current build/schema가 준비되지 않은 상태로 중단한다. 직접 local UE listener 프로세스의 절대 project path를 검증하며, CIM/프로세스 정보가 거부되면 ownership을 추정하지 않는다.

## 한 스킬 처리 예시

아래 live 명령들은 이번 작업에서 실행하지 않았다. `<sha256>`는 실제 파일에서 계산한 소문자 SHA이고, label은 모든 단계에서 유지하며 evidence-label은 단계마다 새 값을 쓴다.

```powershell
$python = 'D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$script = 'Scripts/Editor/StageWeaponSkillVfxBinding20261111.py'
& $python -B $script --phase prepare --weapon GreatSword --variant Primary01 --label GSPrimary01BindB --evidence-label GSPrimary01PrepB
```

공유 사용권을 인계받은 뒤 pinned 원본 후보 manifest/capture evidence로 fresh inspect를 수행한다. `--manifest`는 `SkillParticlesA/manifest.json`, `--capture-evidence`는 `DevLog/20261110_SwordKnuckle/SkillParticlesReviewG/captures.json`이다. 과거 스냅샷은 이 도구가 실제 조회한 현재 전체 reflection을 대체하지 않는다.

```powershell
$base = @('--weapon','GreatSword','--variant','Primary01','--label','GSPrimary01BindB','--lease-granted',
  '--manifest','<manifest.json>','--manifest-sha256','<sha256>',
  '--capture-evidence','<captures.json>','--capture-evidence-sha256','<sha256>')
& $python -B $script @base --phase inspect --evidence-label GSPrimary01InspectB
```

inspect 보고서의 프로젝트 skills·전체13asset 속성·카탈로그 범위·preflight를 검토한다. 필요한 자료·수명 조건이 맞지 않으면 중단한다. 반영 전후 Source/Config/Scripts·관련 asset hash를 고정하므로 다른 Codex 변경이 있으면 새 inspect부터 다시 조율한다.

```powershell
$snapshot = '<inspect-summary.json>'
$snapshotSha = (Get-FileHash -LiteralPath $snapshot -Algorithm SHA256).Hash.ToLowerInvariant()
& $python -B $script @base --phase stage --evidence-label GSPrimary01StageB --snapshot $snapshot --snapshot-sha256 $snapshotSha
$stage = '<stage-summary.json>'
$stageSha = (Get-FileHash -LiteralPath $stage -Algorithm SHA256).Hash.ToLowerInvariant()
& $python -B $script @base --phase activate --evidence-label GSPrimary01ActivateB --snapshot $snapshot --snapshot-sha256 $snapshotSha --stage-report $stage --stage-report-sha256 $stageSha
```

stage는 선택한 DA만 복제한 unbound 후보이고 source/profile은 변경하지 않는다. activate는 원본의 enable/Cast/Impact 최대3필드만 복사한다. no-op도 clean saved hash를 남기며 partial/false/timeout은 자동 재시도하지 않는다. 보호 필드 차이가 발견된 dirty 상태를 자동으로 되돌리거나 저장하지 않는다.

```powershell
$activation = '<activate-summary.json>'
$activationSha = (Get-FileHash -LiteralPath $activation -Algorithm SHA256).Hash.ToLowerInvariant()
& $python -B $script @base --phase verify --evidence-label GSPrimary01VerifyB --snapshot $snapshot --snapshot-sha256 $snapshotSha --activation-report $activation --activation-report-sha256 $activationSha --require-cold-reload
```

`--require-cold-reload`는 담당자가 clean 종료/재실행을 조율한 뒤 사용한다. 스크립트가 UE를 시작하거나 종료하지 않는다. saved readback 성공은 실제3인칭 렌더·피격취소·cook·물리입력 검증이 아니며, `visual_quality_approved=false`를 유지한다. 기존 Cast/Impact 잔상은 기존 timer를 따르므로 취소 시 모든 효과의 즉시 제거를 보장하지 않는다.
