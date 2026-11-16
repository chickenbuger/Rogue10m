# 마법 명령 입력 버퍼 검증 결과

2026-11-17 · `Sprint#10-1-magic-command-buffer`.

Q/E/R 입력 수명을 관리하는 독립 버퍼를 원래 작업 폴더와 게시용 물리 checkout에서 각각 컴파일해 **6사례·1,701검사 통과**를 확인했다. 기존 게임 입력에 연결한 기능은 아니며, 이번 완료 범위는 다른 코드 없이 사용할 수 있는 버퍼와 회귀 검사다.

## 구현과 검토

- `Source/Rogue10m/Magic/Rogue10mMagicCommandBuffer.h`: `BeginSession`, `KeyDown`, `KeyUp`, `CancelSession`, `ReadSnapshot`을 제공한다. 진입 전에 누른 키는 놓고 다시 눌러야 기록되며, 이전 epoch의 이벤트는 소비해 일반 기술 입력으로 새지 않게 한다.
- `Scripts/Tests/MagicCommandBufferTests.cpp`: 위 헤더를 직접 검사한다. 용량 초과에도 세션을 유지하고 입력을 소비하며, 취소 뒤에도 이전 스냅샷은 독립된 값으로 남는지 확인한다.
- `Scripts/TestMagicCommandBuffer.ps1`: 새 label의 임시 디렉터리에 컴파일·실행 결과와 해시를 남긴다. 엔진이나 UBT를 호출하지 않는다.

독립 코드 검토에서 키 마스크의 범위 검사, stale epoch 우선 처리, 취소 뒤 소비, 용량 초과 latch, 스냅샷 복사를 확인했다. 원래 구현에 대한 과거 검토에서 발견했던 stale+invalid token 문제는 현재 코드와 회귀에 반영돼 있다. 이번 검토에서 추가 결함은 찾지 못했다.

## 실제 검증

| 실행 | 결과 | 증거 |
| --- | --- | --- |
| 원래 checkout `Publish1117A` | 6사례·1,701검사, exit0 | [요약](evidence/magic-command-buffer-20261117/RootPublish1117A/summary.json), [출력](evidence/magic-command-buffer-20261117/RootPublish1117A/tests.log) |
| 게시 checkout `Pub1117B` | 6사례·1,701검사, exit0 | [요약](evidence/magic-command-buffer-20261117/PublicationPub1117B/summary.json), [출력](evidence/magic-command-buffer-20261117/PublicationPub1117B/tests.log) |

두 실행 모두 clang++ 19.1.5와 C++17·경고 오류 처리·최적화 옵션을 사용했다. 세 파일은 원래 checkout에서 바이트 그대로 복사했으며, 두 실행의 헤더와 테스트 SHA256이 같다. 컴파일러가 진단을 출력하지 않아 `compile.log`는 생성되지 않았다. 실행 파일은 임시 디렉터리에만 둔다.

## 완료 경계

이 헤더는 현재 다른 런타임 파일에서 사용하지 않는다. Ctrl 진입, 첫 Q의 고정 기한, QQQ 자동 시전, 마나·피해·마법진·투사체·저장·실제 입력 연결은 이 버퍼에 들어 있지 않다. 최신 게임 규칙은 그대로 후속 연결 대상이며 별도 수치를 정하지 않았다.

이번에는 UE·에디터·플레이를 실행하지 않았다. 게임 연결이나 시전, 물리 입력, 렌더링 검증이 완료됐다고 보고하지 않는다. 기능 파일과 증거를 준비했으며 실제 commit·push와 통합 결과는 부모 담당자가 성공 후 기록한다.

관련: [계획](../architect/2026-11-17_magic-command-buffer.md).
