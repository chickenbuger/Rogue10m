# 마법 Q/E/R 입력 버퍼 구현 결과

작성일: 2026-11-10 UTC. 상태: **독립 구현·standalone 검증 완료 / 공유 소스 반영·게임 입력 연결 전**.

사용자의 '바로바로 기능 적용', '검토 받아야 하는 사항은 알림' 후속 요청에 따라 [마법진 설계](../architect/2026-11-10_magic-circle-niagara-integration-plan.md)의 입력 수명 관리부터 구현했다. Ctrl 방식·주문 문법·마나·실패 수치를 결정하지 않는 작은 단위다.

## 1. 구현한 코드

독립 물리 작업공간은 `tmp/magic-command-20261110/candidate/`다. 공유 Content 링크·Git 브랜치 전환·기존 소스 편집 없이 아래 세 파일을 작성했다.

| 실제 후보 파일 / 반영할 상대 경로 | 내용 |
| --- | --- |
| `Source/Rogue10m/Magic/Rogue10mMagicCommandBuffer.h` | C++17 header-only `Rogue10m::Magic::TCommandBuffer<Capacity>` |
| `Scripts/Tests/MagicCommandBufferTests.cpp` | 실제 후보 헤더를 include하는 독립 C++ 시험 |
| `Scripts/TestMagicCommandBuffer.ps1` | Windows clang++ 빌드·실행·해시 기록. UE/UBT 호출 없음 |

기능은 명시적인 BeginSession/KeyDown/KeyUp/CancelSession/ReadSnapshot API다. Q/E/R 순서를 보존하고 같은 키 Down 반복을 한 번만 추가한다. 세션 진입 시 현재 누른 키 마스크를 받아 진입 전부터 누르던 키는 놓고 다시 눌러야 추가된다. 이전 세션 epoch의 Down/Up은 상태를 바꾸지 않고 소비하여 일반 즉발 입력으로 새지 않게 한다. 용량 초과도 소비하며 자동 발사·실패 판정으로 바꾸지 않는다. 스냅샷은 값 복사이며 취소는 입력만 폐기한다.

용량은 호출자가 지정하는 저장공간이며 게임의 최대 주문 길이를 정한 것이 아니다. 조합 인식·확정키·타임아웃·속성·마나·패널티·Niagara·UObject·Tick·저장은 포함하지 않는다. 이 코드를 추가해도 기존 Ctrl/QER 바인딩에 자동 연결되지는 않는다.

## 2. 검증과 수정

실행한 명령(후보 작업공간 기준):

```powershell
.\Scripts\TestMagicCommandBuffer.ps1 -RunLabel Candidate02
```

실제 결과는 `RESULT=MAGIC_COMMAND_BUFFER_PASSED cases=6 checks=1701`이다. Visual Studio 2022의 clang++ 19.1.5로 C++17, `-Wall -Wextra -Werror -pedantic -O2`를 적용해 컴파일했다. 엔진 빌드·에디터·PIE를 실행하지 않았다.

시험은 순서/비활성, 세 키의 모든 진입 held 조합, Down 중복과 Up 후 재입력, 이전 epoch가 새 latch를 풀지 않기, 용량 초과 소비·자동 확정 없음, 취소/스냅샷 분리, enum 범위 밖 3..255의 안전한 거부를 포함한다.

독립 검토에서 취소 후 이전 epoch의 잘못된 enum이 `InvalidToken`과 미소비로 반환될 수 있는 결함을 발견했다. epoch를 토큰 검증보다 먼저 검사하도록 수정했고 stale+invalid Down/Up 전체를 회귀 시험에 추가했다. 재검토에서 추가 중대 결함은 발견하지 못했다. 첫 Candidate01의 1,194검사는 수정 전 후보 기록이며 최종 결과로 대신 쓰지 않는다.

공유 프로젝트에서 [integration.patch](evidence/magic-command-buffer-20261110/integration.patch)의 `git apply --check --whitespace=error-all`을 실행해 통과했다. 이는 패치 적용 가능성과 공백 검사이며 실제 소스 반영이 아니다.

증거: [최종 summary.json](evidence/magic-command-buffer-20261110/summary.json), [시험 출력](evidence/magic-command-buffer-20261110/tests.log), [컴파일러](evidence/magic-command-buffer-20261110/compiler.txt). 실행 파일·중간 출력은 ignored tmp에만 있다.

## 3. 공유 소스 반영을 코디네이터에게 남긴 이유

대검 ResumeSword1은 전용 BlendSpace·몽타주·QA를 사용 중이다. 기존 Character/Combat·Build.cs·기존 테스트는 건드리지 않았다. 새 파일명은 현재 Source에 없고 모듈 root include 경로가 이미 등록되어 Build.cs 수정도 필요하지 않다.

현재 팀의 소스 소유/에디터 lease 파일은 찾지 못했으며, 프로세스 조회에 UE/UBT가 없다는 것만으로 사용권이 해제됐다고 판단하지 않았다. [협업 원칙](../architect/2026-09-28_studio-agent-workflow.md)의 '빌드/QA 담당은 하나', '공유 DevLog/Sprint/브랜치는 코디네이터만 소유'와 이번 위임의 '공유 소스 적용 전 다른 작업과 조율'에 따라, 검증된 새 파일 패치를 부모 통합 담당에게 인계한다. 자동 승인 거절은 없으며 **공유 소스 적용 작업은 조율 대기**다.

코디네이터는 대검 담당에게 새 세 파일의 소유 범위를 알린 뒤 프로젝트 root에서 다음 두 단계를 실행할 수 있다.

```powershell
git apply --check --whitespace=error-all Feature/doc/evidence/magic-command-buffer-20261110/integration.patch
git apply --whitespace=error-all Feature/doc/evidence/magic-command-buffer-20261110/integration.patch
```

적용 후 새 RunLabel로 `Scripts/TestMagicCommandBuffer.ps1`를 실행하고, [summary.json](evidence/magic-command-buffer-20261110/summary.json)의 후보 SHA-256과 새 파일 해시를 대조한다. 새 파일만 추가하는 패치라 기존 미커밋 파일을 덮어쓰지 않는다. 파일이 이미 있다면 패치 실패 원인을 확인하고 강제 적용하지 않는다.

## 4. 실제 게임 연결과 사용자 검토 사항

현재 `Rogue10mCharacter.cpp`는 LeftControl에 무기 아이덴티티가 연결되어 있고, Q/E/R pressed는 modifier를 거부하며 released는 일반 Combat에 전달된다. 기존 경로보다 먼저 단일 gate가 Ctrl 및 Q/E/R의 press/release를 소유하도록 통합해야 게임에서 쓸 수 있다. Native/Enhanced Input의 이중 경로를 모두 통과시켜야 한다.

라우터는 원래 물리 이벤트 순서대로 한 스레드에서 호출하고, 큐에 넣을 때 epoch를 캡처하며, BeginSession에는 실제 held 키를 전달해야 한다. `bConsumed=true`는 토큰 추가 성공과 무관하게 일반 즉발 호출을 차단한다. 버퍼의 epoch와 latch는 모든 물리 입력 경로를 자동으로 중재하지 않는다.

사용자 검토가 필요한 것은 Ctrl hold/toggle, 문자열 길이·문법·완료/취소 키·타임아웃, 미해금/오주문의 실패 구분, 마나 초과·체력 손실 공식, 서클/즉발 해금 기준이다. 이번 구현에서 임의 수치나 키를 넣지 않았다. 해당 결정 전에도 이 버퍼의 소스 반영·회귀 검증은 가능하다.

게임 내 마법 시전·마법진·투사체·피해·실제 입력 소비·3인칭 렌더·성능 검증은 미수행이다. 이번 결과를 '마법 기능이 게임에 적용됨'으로 보고하지 않는다. 기존 대검·저장·카메라·피해·비용·타수·VFX 계약은 이 작업에서 변경하지 않았다.

## 5. 부모 통합용 일지·Sprint 기록안

코디네이터가 `DevLog/20261110.txt`에 추가할 내용:

> [마법 Q/E/R 입력 기반 구현 / UTC 2026-11-10]
>
> 미정인 주문·마나 수치를 정하지 않고 Q/E/R 입력 버퍼와 세션 epoch·held 키·중복 소비·취소 수명을 C++17로 구현했다. 별도 물리 작업공간에서 최종 6개 시나리오·1,701개 검사를 통과했고, 취소 후 오래된 잘못된 토큰이 일반 스킬로 새는 경로를 독립 검토로 찾아 교정했다.
>
> 공유 소스 반영 전 담당자 조율 규칙에 따라 새 파일 3개만 추가하는 integration.patch를 준비하고 적용 전 검사를 통과했다. 실제 공유 소스 반영·Ctrl/QER 게임 연결은 통합 담당 대기이며, UE/UBT·에디터·에셋·커밋·푸시는 실행하지 않았다. Feature/doc/2026-11-10_magic-command-buffer.md와 전용 증거를 참고한다.
>
> [Notion 요약 후보] 마법 입력의 독립 코드와 회귀 검증을 완료했다. 기획 문서만 추가한 단계는 넘어섰으나 게임 내 시전 적용과는 구분한다. 다음 단계는 새 소스 통합과 단일 입력 gate 연결이며 Ctrl 정책 등 사용자 선택만 남겨 둔다.

Sprint 기록안: 목표=확정된 QER 입력 수명 기반 구현, 주요 변경=header-only 명령 버퍼/standalone 시험/실행기, 검증=clang++ 경고오류0·6사례1701검사·패치 적용 전 검사·독립 재검토, 상태=독립 구현 완료·공유 소스/게임 연결 전, 관련 문서=본 결과와 마법진 연계 설계. 공유 기록·브랜치·커밋·푸시는 부모 통합 담당 범위다.
