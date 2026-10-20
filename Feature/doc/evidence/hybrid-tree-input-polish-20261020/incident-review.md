# Content junction 삭제 사고와 복원 독립 검토

2026-10-20. 검토자는 복원 완료 후 파일을 읽고 해시를 계산했다. 이 검토에서는 Git 상태, Source, Content를 변경하거나 에디터·빌드를 실행하지 않았다.

## 확인 결과

삭제 manifest의 추적 Content 6,423개가 모두 다시 존재하며, 원본 커밋의 blob과 일치한다. 전량 1회 검사한 데이터는 4,480,714,917바이트다. 검사 중 파일 크기/수정 시각 변경은 0개였다. 이 결과는 사고로 누락된 추적 파일의 복원을 확인하며, 사고 전 모든 작업 파일의 완전한 바이트 보존을 소급해서 증명하는 것은 아니다.

복원 기준은 커밋 `e12df4146affa1225acfdcaa5377a60f5670850a`, Content tree `561dc863470c130a9b5fe511c6f9c68dd8f74b41`이다. 검토 시점 공유 root HEAD `b11aefe04105c50fb2ca79bdbae24c760a93da20`의 Content tree도 같은 값이다.

## 사고와 복원 범위

격리 sparse worktree의 Content가 공유 root로 향하는 junction이었다. 최신 main 통합 중 Git의 sparse cleanup이 이 junction을 따라 공유 root의 추적 파일 6,423개를 삭제했다. 공유 폴더를 읽기 용도로만 사용할 것이라는 운영 의도가 파일 시스템 쓰기 격리를 보장하지 못했다. 실제 파일을 삭제한 원인은 DevLog의 줄바꿈 정규화 자체가 아니라, Git이 관리하는 작업 경로 안에 공유 쓰기 대상 junction을 둔 구성이다.

최초의 광범위한 root 복원은 미감사 기존 파일 덮어쓰기 위험으로 자동 승인 검토에서 거절됐다. 이후 담당자가 실제 감사에서 status D이면서 파일이 존재하지 않는 경로만 literal manifest로 좁혀 복원했다. `content-recovery-audit.json`은 누락 6,423개와 그 외 Content 변경 0개를 기록하며, `content-recovery-result.json`은 누락 파일만 복원했고 기존 존재 파일 덮어쓰기 0개, Git Content diff 0개임을 기록한다. 검토자는 이 과거 상태를 독립 재현한 것이 아니라 해당 기록을 확인했다.

사고 전 `shared_checkout_modified:false`는 사실 확인이 충분하지 않았던 가정이다. 격리 작업의 소스 diff가 제한되어 있다는 점만으로 공유 root 파일 시스템이 바뀌지 않았다고 판단할 수 없다. 이 가정은 최종 결과 문서에서 유지해서는 안 된다.

## 독립 파일 검사

1. `content-recovery-missing.paths`의 고유 경로 수가 6,423개이고 감사 JSON의 누락 집합과 정확히 같은지 확인했다. 모든 경로는 `Content/` 아래이며 기준 커밋의 추적 blob으로 존재한다.
2. 기준 커밋의 `git ls-tree` 결과에서 경로별 기대 blob ID를 읽었다. 실제 파일은 이진 스트림으로 읽고 `blob <길이>\0` 헤더를 붙인 SHA-1을 계산했다. 전체 Content diff나 복원을 다시 실행하지 않았다.
3. 6,412개는 실제 바이트 그대로 원본 blob과 일치했다. 나머지 11개는 `.gitkeep` 10개와 `Content/UI/HUD/ReferenceMetal/generation-prompt.txt`였으며, Git checkout의 CRLF 차이였다.
4. 위 11개만 `git hash-object --path=<경로>`로 clean-filter blob을 대조했다. 11개 모두 원본 blob과 일치하며, 실제 바이트에서 CRLF를 LF로 바꾸는 것 외에는 원본과 차이가 없음을 추가 확인했다. 전량 해시 검사를 반복하지 않았다.

최종 누락 0개, 정규화 후 blob 불일치 0개다. 최초 raw 검사에서 나온 11개의 차이를 숨기거나 이진 에셋 손상으로 간주하지 않고 줄바꿈 변환 여부를 별도로 확인했다. 검사 대상 확장자는 `.uasset`, `.umap`, `.png`, `.wav`, `.txt`, 확장자 없는 `.gitkeep`였다.

## 공유 Root Source 확인

Root Source를 한 번 비교했을 때 변경은 병행 보스 작업의 다음 세 파일이었다. 이 파일들을 되돌리지 않았으며, 현재 clean-filter blob이 모두 최신 main `e12df414`의 완료 코드와 같은지 확인했다.

| 파일 | 현재/기준 일치 blob |
| --- | --- |
| `Rogue10mRunCompletionComponent.cpp` | `b670ff34bcac5c39e09dca2f458c26d849034604` |
| `Rogue10mRunCompletionComponent.h` | `391ef9decf6baf42d1ee3e314d2769a41d9dbe85` |
| `Rogue10mRunCompletionRuntimeTest.cpp` | `70c62a314a1d5d77db5e9ffcc0c14ed68568e3d1` |

이번 스킬트리의 Root 네 파일도 각각 Root HEAD `b11aefe0`의 blob과 일치했다. 따라서 검사 시점에는 격리 스킬트리 수정이 아래 Root 소스에 주입되어 있지 않았다.

| 파일 | 현재/Root HEAD 일치 blob |
| --- | --- |
| `Rogue10mHybridSkillTreeWidget.cpp` | `0f9169068b3e2e8ca0ae43c8128c66ccfc07bfc8` |
| `Rogue10mHybridSkillTreeWidget.h` | `cce2958bd1cf56629714663f0c5c062b27549d2b` |
| `Rogue10mHybridSkillTreeRuntimeTest.cpp` | `5fc84efe5d46f29529959619258c1abaf5f767be` |
| `VerifyHybridSkillTree.ps1` | `0f830745f5e3d10c34140e951d098e406bc78ff8` |

사고 전 Root Source 전체 SHA baseline은 없었다. 위 확인은 알려진 병행 완료 코드와 스킬트리 네 파일의 현재 상태를 확인한 것이며, Root 전체가 작업 내내 불변이었다는 주장은 하지 않는다. 추적하지 않은 모든 파일의 사고 전 바이트까지 확인할 근거도 없다.

## 재발 방지와 남은 확인

- Git checkout/merge/sparse 관리 대상 경로에 다른 작업 폴더로 향하는 Content junction을 두지 않는다. 읽기 의도나 폴더 이름 대신 실제 reparse point·해결 경로를 확인한다.
- 이 작업의 Content는 실제 독립 checkout으로 구성한다. Git 작업 전에 Content가 junction이 아니고 실제 경로가 격리 worktree 내부에 있는지 확인한다. 후속 확인에서 독립 Content의 속성은 `Directory`, LinkType/Target 없음이었다. 이 구성에서 최신 빌드·자동화·실입력 런타임·새 프로세스 읽기가 수행됐으며 결과는 [review.md](review.md)에 기록했다.
- 병행 작업이 있는 공유 root에 작업 전 경로별 상태와 해시 기준을 남긴다. 복원은 감사한 실제 부재 경로만 대상으로 하며 기존 파일을 포괄적으로 덮어쓰지 않는다.
- 복원 확인과 스킬트리 동작 검증은 별도다. 복원 후 독립 Content에서 빌드02 `Succeeded`/103.04초, 자동화 3개 Success/errors=0, 런타임 176 PASS/0 FAIL, 새 프로세스 읽기 77 PASS/0 FAIL과 각 실행의 두 성공 marker·exit 0을 확인했다. 긴 플레이·패키징·멀티플레이 등의 범위 밖 검증으로 확대하지 않는다.
