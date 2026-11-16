# 마법 명령 입력 버퍼 독립 게시 계획

2026-11-17 · `Sprint#10-1-magic-command-buffer` · 기준 `fdbe8c8c`.

## 범위

이미 작성된 C++17 명령 버퍼를 별도 물리 checkout에서 검증하고 기능 단위로 게시한다. 대상은 `Rogue10mMagicCommandBuffer.h`, 이를 직접 사용하는 `MagicCommandBufferTests.cpp`, `TestMagicCommandBuffer.ps1` 세 파일이다. 기존 Character·Combat·Config·Content는 이 패킷의 대상이 아니다.

버퍼는 Q/E/R 입력 순서와 현재 누른 키, 세션 epoch, 용량 초과 소비, 취소와 값 복사를 관리한다. Ctrl → 첫 Q 기한 → QQQ 자동 시전의 최신 게임 규칙을 대체하지 않으며, 해당 입력 라우팅과 주문 판정은 후속 구현이다. 저장 용량은 호출자가 정한다.

## 작업 패킷

| 패킷 | 목표·대상 | 완료 조건 | 검증 | 복구 경계 |
| --- | --- | --- | --- | --- |
| M1 Architect | 세 파일과 기존 호출 관계 확인 | 외부 런타임 의존성과 게임 연결 여부 구분 | 좁은 코드 읽기·검색 | 읽기 전용 |
| M2 Implementer·Builder | 동일 바이트 복사와 독립 컴파일 | 양쪽 checkout에서 6사례·1,701검사 통과 | `Scripts/TestMagicCommandBuffer.ps1 -RunLabel Pub1117B` | 신규 세 파일과 해당 증거 |
| M3 Reviewer·Doc Writer | API·수명 검토와 결과 기록 | 누락 의존성 없음, 코드/증거 해시 일치, 범위·공백 검사 통과 | SHA256·PowerShell parser·generated 검사 | 본 문서·결과서·공용 기록의 추가분 |

## 검증 기준

clang++의 C++17·`-Wall -Wextra -Werror -pedantic -O2`로 실제 헤더를 컴파일한다. 순서/비활성, 진입 시 누른 키의 모든 조합, 반복 Down과 Up, 오래된 epoch, 용량 초과, 취소/스냅샷, 잘못된 enum을 검사한다. UObject·UE 모듈·애셋을 사용하지 않는 세 파일이므로 엔진 빌드와 에디터 실행은 필요하지 않다.

게임 연결 완료나 시각 검증으로 범위를 넓히지 않는다. 기존 미완료 지형·전투 변경은 원래 checkout에 유지한다. 검증된 파일만 부모 담당자가 commit하고 작업 브랜치 push 후 `develop → test → main` 순서로 반영한다.
