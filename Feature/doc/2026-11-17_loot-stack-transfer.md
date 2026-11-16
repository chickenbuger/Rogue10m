# 전리품 스택 수량 계산 검증 결과

2026-11-17.

독립 게시용 물리 checkout에서 실제 수량 계산 헤더를 컴파일해 **명명 사례9개·무작위 사례50,000개·검사2,103,255개 통과**, 종료코드0을 확인했다. 완료 범위는 엔진과 분리된 수량 계산이며 자동 입고의 게임 연결은 포함하지 않는다.

## 구현

- `Source/Rogue10m/Components/Rogue10mLootStackTransfer.h`: `PlanTransfer`가 기존 스택별 증가량, 새 항목 수량, 수령량과 잔량을 반환한다.
- `Scripts/Tests/LootStackTransferTests.cpp`: 실제 헤더를 직접 검사하고 독립된 64비트 용량 계산과 결과를 대조한다.
- `Scripts/TestLootStackTransfer.ps1`: 새 label의 임시 폴더에서 컴파일·실행하고 컴파일러·출력·해시를 기록한다.

예를 들어 최대20개인 기존 스택18개에5개가 들어오고 빈 공간이 없으면 기존 스택에2개를 합치고3개를 남긴다. 빈 공간이 있으면 남은3개를 새 항목에 배정한다. 잘못된 기존 수량은 변경하지 않으며, 잘못된 입력은 유효한 계획으로 반환하지 않는다.

## 검증과 검토

`Scripts/TestLootStackTransfer.ps1 -RunLabel LootPub1117B`를 실행했다. clang++19.1.5·C++17·경고 오류 처리·최적화를 사용했다. 전체 수령, 부분 병합, 새 항목, 다중 스택, 잘못된 수량, 최댓값과 오버플로, 수량 보존을 검사했다.

세 파일은 원래 checkout에서 바이트 그대로 복사해 SHA256 일치를 확인했다. 코드 검토에서 기존 수량의 범위 검사 후 뺄셈하는 순서, 입력 이하의 잔량, signed 합산을 피한 수령량 계산을 확인했으며 추가 결함은 찾지 못했다. 엔진·UBT·에디터는 실행하지 않았다. 컴파일러 진단이 없어 `compile.log`는 생성되지 않았다.

근거: [실행 요약](evidence/loot-stack-transfer-20261117/LootPub1117B/summary.json), [검사 출력](evidence/loot-stack-transfer-20261117/LootPub1117B/tests.log), [컴파일러](evidence/loot-stack-transfer-20261117/LootPub1117B/compiler.txt).

## 완료 경계

게시 대상에는 이 헤더를 호출하는 인벤토리 어댑터가 없다. 호환 항목 선정·실제 공간 판정·GUID·알림·전리품 생성·재시도·월드 수명·저장은 별도 구현이다. 따라서 이번 결과를 게임 내 자동 입고·충돌·저장·전체 런 성공으로 표현하지 않는다. 실행 파일은 임시 폴더에만 두며, 게시 담당자가 별도 기능 커밋과 통합을 진행한다.

관련: [계획](../architect/2026-11-17_loot-stack-transfer.md).
