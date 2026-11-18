# 가방 공간 확보 후 드롭 자동 재획득

## 범위와 변경 경계
기존 처치→드롭→획득·저장을 유지하면서, 플레이어가 드롭 위에 머물고 가방이 꽉 찬 경우 인벤토리 변경 다음 tick에 재시도한다. DroppedItem cpp/h와 테스트·runner만 수정하며 Inventory/SaveGame 형식과 확률·밸런스는 바꾸지 않는다.

## 수명 및 거래 안전
마지막 실패 플레이어/인벤토리를 weak reference로 관측하고 같은 알림은 하나의 next-tick 작업으로 합친다. 인벤토리 거래 안에서 재진입하지 않는다. 생존·거리·overlap을 다시 검사하고 성공·이탈·EndPlay에서 delegate/timer를 정리한다. 단일플레이 범위이며 다중 플레이어 관측 확장은 별도다.

## 검증
신규 stationary capacity/transaction, bag expansion, leave/destroy 3개와 기존5개 자동화 전체를 strict8개로 검증한다. 별도 격리 슬롯의 실제 Write26/Read9 검사로 처치·충돌·획득·재로드를 확인하고 전체 기존 저장 SHA256 및 신규 namespace를 대조한다.
