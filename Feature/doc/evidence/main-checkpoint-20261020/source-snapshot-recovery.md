# 2026-10-20 게시용 Source 스냅샷 복구 근거

사용자는 빌드·QA를 통과한 최초 상태만 게시하고, 이후 병행 개발 내용은 로컬에 보존하도록 선택했다. 이 보고서는 최초 Git blob을 식별한 읽기 전용 조사 결과다. 조사 중 index·Source 작업 파일을 수정하지 않았으며, 이 보고서만 새로 작성했다. 실제 게시용 index 구성은 주 작업자가 수행한다.

## 조사 기준

- 최초 Source 범위: 기존 수정 38개 + 신규 65개 = 103개.
- 최초 파일 목록 blob: `db54e05dfd3b0b2aa207d433871baa808a497e86`.
- 해당 목록은 머리말 2줄 + 경로 6,075개다. blob의 loose object 수정 시각은 2026-10-20 11:19:33 UTC다.
- 병행 작업의 `Feature/doc/evidence/hybrid-skill-tree-runtime-20261020/baseline-tracked.json`은 11:17:59 UTC에 기록되었으며, 당시 기존 수정 Source 38개의 원시 파일 SHA256을 제공한다. 검증 입력으로만 읽었으며 이번 게시 범위에 추가하지 않는다.
- 최초 Source blob은 11:18:50 UTC, 최초 Town/Save 신규 blob은 11:19:54 UTC에 생성되었다. 병행 하이브리드 스킬트리 Source 수정은 11:21 UTC 이후 시작했다.

## 복구할 기존 파일 4개

아래는 최초 staging 때 저장된 원본 blob이며, 이후 index에 들어간 하이브리드 스킬트리 변경을 포함하지 않는다. 모두 아래 6개 파일 검증의 SHA256과 정확히 일치한다.

| 파일 | 게시할 최초 blob OID |
|---|---|
| `Source/Rogue10m/Character/Rogue10mCharacter.cpp` | `e8b536ebbf75ac35e460c5576768f8a18ebeaca7` |
| `Source/Rogue10m/Character/Rogue10mCharacter.h` | `031c268ddbe659be05ff81bfef5febfdefbcc113` |
| `Source/Rogue10m/Components/Rogue10mCombatComponent.cpp` | `9ff9c1a8aa77b62e2c635595e9abc304a93f1c4f` |
| `Source/Rogue10m/Components/Rogue10mCombatComponent.h` | `ec92fd658ac0e30e0253739628080b268f265817` |

## 기존 Source 6개 원본의 해시 검증

Git의 LF 정규화 이전 원시 파일 SHA256을 baseline과 비교했다. 원본 코드 내용은 전부 기존 Git blob에서 읽었다. CRLF/LF 복원은 메모리에서만 수행했다.

| 파일 | 최초 blob OID | baseline 원시 SHA256 |
|---|---|---|
| `Character/Rogue10mCharacter.cpp` | `e8b536ebbf75ac35e460c5576768f8a18ebeaca7` | `3938C91B27ADCCEE60C9F4CE40C5464323DD89020C49221644DDCFAB9CFE32FB` |
| `Character/Rogue10mCharacter.h` | `031c268ddbe659be05ff81bfef5febfdefbcc113` | `7053280497A3799BD942F828A84C781835368520D0B42E9677899A84575B8F64` |
| `Components/Rogue10mCombatComponent.cpp` | `9ff9c1a8aa77b62e2c635595e9abc304a93f1c4f` | `EA32FB1BC27794CC98815DAE39F2C52457423CED363A63267F190B596868367E` |
| `Components/Rogue10mCombatComponent.h` | `ec92fd658ac0e30e0253739628080b268f265817` | `DA48D20BBADB29545CE8E6B39655069AE86987256D8E61DCE60353C645C8D3B7` |
| `Core/Rogue10mPlayerState.cpp` | `c4bdf992c7bb5d1df3bb572184459648ee23eedb` | `DCE58E3FEC593CCEE2B014C55B5DB24A197BEF5EF78D59518138CAA120FA1D4C` |
| `Core/Rogue10mPlayerState.h` | `27de2da0a97a2253362d1d764cd03fa3bfc32c9f` | `5C3BDC3BC2712D9F1904A029D2421F427555C9FF383294326A5E4004372024FF` |

표의 짧은 파일 경로는 모두 `Source/Rogue10m/` 아래다. PlayerState 두 파일은 조사 당시 index에 위 최초 blob이 그대로 있었고, 새 변경은 unstaged 상태였다.

재현 방법:

1. `git cat-file blob <OID>`를 바이너리로 읽거나 `.git/objects/<앞 2글자>/<나머지>`를 zlib으로 해제하고 NUL 뒤의 blob 바이트를 읽는다. 파일로 덮어쓰지 않는다.
2. Character.h와 PlayerState 두 파일은 LF를 CRLF로 변환한 메모리 바이트의 SHA256이 baseline과 직접 일치한다.
3. 혼합 줄바꿈 파일은 원본 blob과 현재 파일의 줄 내용을 `difflib.SequenceMatcher(..., autojunk=False)`로 대응시킨다. 내용이 같은 줄은 현재 파일에 보존된 CRLF/LF를 적용한다. 현재 파일의 새 코드 내용은 가져오지 않는다.
4. Character.cpp는 원본 줄 모두가 대응되어 추가 추론 없이 정확히 일치했다. Combat.h의 원본 2줄 한 구간은 CRLF로 복원했을 때 정확히 일치했다.
5. Combat.cpp의 교체된 원본 26줄·25구간은 각 구간 양옆의 보존된 줄바꿈이 모두 같았다. 그 줄바꿈을 해당 원본 구간에 적용한 결과 SHA256이 정확히 일치했다. 비교 불일치나 미확정 구간은 남지 않았다.

## 최초 신규 파일 2개

아래 파일은 작업 시작 전에 이미 존재하던 신규 Source이며, 조사 당시 최초 blob이 index에 그대로 있었다. 이후 작업 파일 변경은 unstaged 상태였다.

| 파일 | 유지할 최초 blob OID | 원본 확인 |
|---|---|---|
| `Source/Rogue10m/Components/Rogue10mTownProgressionComponent.cpp` | `52c9c40c85fb2ec6f0ba262afdca163329a02512` | 최초 11,483바이트, 하이브리드 스킬트리 참조 없음 |
| `Source/Rogue10m/Core/Rogue10mProgressionSaveGame.h` | `fea57d2d78a3f540b00d32f0d761f882bc866e7b` | 최초 2,958바이트, `CurrentVersion = 2`, 스킬트리 저장 필드 없음 |

## 병행 작업 보존 경계

새 Source인 `Rogue10mSkillTreeCombatComponent.cpp/.h`, `Rogue10mSkillTreeProgressionComponent.cpp/.h`, `Rogue10mSkillTreeCatalog.cpp`, `Rogue10mSkillTreeData.cpp/.h`, `Rogue10mHybridSkillTreeWidget.h`와 이후 추가 구현은 이번 최초 상태 게시에서 제외한다. 기존 파일의 새 스킬트리 변경과 `UI/Rogue10mRunHUD.cpp` 변경도 작업 파일에 보존한다. 진행 중 하이브리드 스킬트리 계획·결과·증거는 이번 게시 범위로 확장하지 않는다.

최초 목록과 조사 당시 index의 경로 차이는 16개였다. 그중 7개는 새 스킬트리 Source이고, 나머지 9개는 주 작업자가 이번 게시 검증에서 생성한 whitespace/generated 경로 검사 및 Town QA 증거다. 최초 목록에서 사라진 경로는 없었다. 게시용 index는 최초 103개 Source와 주 작업자의 검증·게시 문서만 명시적으로 구성해야 한다. 현재 작업 파일을 일괄 재추가하면 검증 이후의 병행 구현이 다시 포함될 수 있다.
