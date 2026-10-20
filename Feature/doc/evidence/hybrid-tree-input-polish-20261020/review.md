# 혼합 스킬트리 표현·실입력 독립 검토

2026-10-20. 격리 작업 폴더 `tmp/hybrid-tree-input-polish`의 UI/QA 소스와 실행 증거를 검토했다. 검토자는 C++ 수정, 빌드, 에디터 실행, Git 변경을 하지 않았다. 공유 Content 사고의 추적 누락 복원과 독립 blob 검사, 독립 Content 구성, 최신 빌드·자동화·런타임·새 프로세스 읽기 검사를 완료했다. 이전 기능의 통과 기록을 이번 변경의 통과 근거로 사용하지 않았다.

## 공유 Content 삭제 사고: 추적 누락 복원 확인

격리 sparse worktree의 Content junction은 공유 root를 참조했다. 최신 main 통합 중 Git의 sparse cleanup이 junction을 따라 공유 root의 추적 Content 파일 6,423개를 삭제했다. 따라서 이 작업이 공유 root에 영향을 주지 않는다는 사전 가정은 잘못됐다. 소스 diff에 콘텐츠 변경이 없다는 아래 판단도 파일 시스템 무변경을 보증하지 않는다. `base-integration.json`의 최초 `shared_checkout_modified:false`는 잘못된 가정이었으며 현재 true와 사고·복원 기록으로 정정됐다.

`content-recovery-audit.json`은 감사 시점의 누락 6,423개 모두 status D/실제 부재였고, 그 외 Content 변경은 0개였음을 기록한다. 복원 기준 커밋은 `e12df4146affa1225acfdcaa5377a60f5670850a`, Content tree는 `561dc863470c130a9b5fe511c6f9c68dd8f74b41`이다. 광범위한 root 복원은 미감사 기존 파일 덮어쓰기 위험으로 자동 승인 검토에서 거절되었으며, 이후 실제 누락 파일만 나열한 literal manifest로 범위를 좁혀 복원했다. `content-recovery-result.json`은 기존 파일 덮어쓰기 0개와 Content diff 0개를 기록한다.

독립 검토에서 6,423개/4,480,714,917바이트를 전량 1회 읽어 기준 blob과 대조했다. 6,412개는 raw 바이트가 일치했고, 텍스트 11개는 CRLF checkout 차이만 있으며 clean-filter blob이 모두 일치했다. 누락·정규화 후 불일치·검사 중 변경은 각각 0개다. 알려진 병행 보스 Source 3개는 최신 main 코드와, Root 스킬트리 4개는 Root HEAD와 일치했다. 사전 Source 전체 해시가 없으므로 전체 Root 불변이나 모든 미추적 파일의 과거 바이트 보존은 주장하지 않는다. 근거와 한계는 [incident-review.md](incident-review.md)에 기록했다. 이후 실제 독립 Content checkout이 생성됐으며, 검토 시 Content 속성은 `Directory`, LinkType/Target 없음으로 확인했다. 최신 빌드와 런타임은 이 독립 Content 구성에서 수행됐다.

## 정적 검토 결과

UI/QA 소스 범위의 정적 검토에서는 차단할 결함을 발견하지 못했다. 검토 대상은 `Rogue10mHybridSkillTreeWidget.cpp/.h`, `Rogue10mHybridSkillTreeRuntimeTest.cpp`, `VerifyHybridSkillTree.ps1`의 기준 main 대비 변경과 HTML 정합성 증거다. 성장 포인트 지급·소비, 노드 해금 조건, 실제 투자 원장, SaveGame 스키마, 전투 수치, 콘텐츠 바이너리의 변경은 이 소스 diff에 없다. 별도로 발생한 공유 Content 삭제 사고는 위 절과 복원 검토에서 다룬다.

### 노드 표현과 입력 영역

- 아이덴티티 입문·심화·성질은 닫힌 5점 마름모, 공명은 기존 내부 원과 닫힌 49점 외부 원으로 표시한다. QA용 읽기 getter도 실제 페인트에 사용하는 `NodeFrame` 계산을 사용한다.
- 장식은 기존 연결선 위젯의 `HitTestInvisible` 페인트에 추가된다. 별도 장식 위젯이 버튼이나 드래그 입력을 가로채는 구조는 없다.
- 초기 구현에서 마름모가 기존 44 단위 버튼 밖으로 약 12 단위 돌출하는 점과 라벨 간격을 지적했다. 최종 구현은 아이덴티티 약 72.23, 공명 54 단위의 entry 영역에 원래 44/40 단위 버튼을 중앙 배치한다. 바깥 영역의 왼쪽 클릭은 entry에서 선택으로 처리하며, 원래 기술의 preview/drag 처리는 유지한다.
- 라벨은 외곽 extent에서 9 단위 떨어져 배치한다. 공명 라벨은 높이 46으로 조정되어 기본 카탈로그에서 y=741~787이며 그래프 높이 790 안에 있다. 최종 1080p/720p PNG에서 새 외곽선과 라벨 간격, 하단 공명 라벨이 화면 안에 표시되는 것을 확인했다.
- 잠금/학습/활성/선택 상태의 색상과 중앙 버튼의 hover/pressed 스타일을 유지한다. 바깥 장식 영역의 실제 선택은 QA의 별도 pointer 검사로 확인하도록 했다.

### 드래그·키 검증 경로

- `HitPath`는 실제 Slate hit path에 대상이 포함되는지 확인한다. 클릭은 pointer move/down/up, 드래그는 임계 거리 이동부터 실제 `FUMGDragDropOp` payload와 HUD 위 pointer release까지 사용한다. 새 입력 검증에 `OnDrop` 또는 `OnClicked` 델리게이트 직접 호출은 없다.
- 격투가 `F.DiamondStrike`와 대검 `G.CrushingSlash`를 실제 학습 상태에서 Primary HUD 슬롯으로 끌어 놓고 장착 결과와 HUD 표시를 함께 확인한다. 회피 E 슬롯 거절은 바인딩과 HUD의 전후 동일성을 확인하며, 잠긴 `F.Ultimate`는 drag operation 생성 거절을 확인한다.
- 마름모와 공명 entry의 정규화 좌표 `(0.5, 0.06)`을 실제 pointer로 클릭하고 선택 ID를 확인한다. 이 점은 기존 중앙 버튼 바깥에 있어 외곽 입력 영역 회귀를 검출할 수 있다.
- 키 선택은 실제 selector 클릭 후 Slate key down/up으로 F7/F8, Esc, 예약 키 E를 전달한다. 해제 버튼과 재지정도 실제 UI 경로를 사용한다. 메뉴를 닫은 뒤 `PlayerController::InputKey`에 press/release를 넣고 정확한 기술, 사용 횟수 증가 1회, 쿨다운 및 release 후 중복 실행 부재를 확인한다.
- 검증 이벤트는 QA가 생성한 엔진 입력 이벤트다. OS 장치 자체의 입력 자동화나 모든 입력 장치·충전/홀드 기술 조합의 검증으로 확대 해석하지 않는다. 기존 성장·전투 fixture 설정에 쓰던 API 호출과 새 사용자 입력 경로 검사는 구분되어 있다.

### QA 저장 격리와 판정

- 실행 스크립트는 프로필·진행·키 저장의 세 QA 접두사를 항상 전달한다. fixture도 세 값의 접두사·길이·허용 문자를 확인한 뒤 작업을 시작하며, `RogueHybridInputQA` 하나만으로 일반 사용자 저장을 대상으로 fixture를 진행하지 않는다. 기존 production 저장 경로 구현은 바꾸지 않았다.
- 새 `-InputChecks` 옵션은 별도 evidence 폴더와 입력 검사 분기를 선택한다. 런타임/읽기 모두 기존 baseline 성공 marker, 새 input 성공 marker, 프로세스 exit 0을 함께 요구한다. 자동화 검사 3개의 Success/errors=0 판정도 유지한다.
- 새 프로세스 읽기 단계는 일반 spawn/Possess/AttachCharacter 이후 디스크의 기존 성장·장비·특성·숙련·투자·아이덴티티 복원을 먼저 확인한다. 이어 Primary에 끌어 놓은 대검 기술과 F8 저장을 비교하고, 대검 실제 UI/입력을 다시 실행하도록 구성했다.

## HTML 대표 경로도

외부 초안과 저장소 사본에서 격투가 마나 3노드·데이터·간선·CSS·활성 설명을 제거하고 내공/투지 2열로 정리했다. 제목에 격투가 대표 경로와 개념도임을 명시했다. 수정 전 사본은 `prototype-before.html`, 실제 검사 결과와 SHA-256은 `prototype-validation.json`에 있다.

실행한 검사는 JavaScript 구문, DOM/JS 노드 ID 16개 일치, 고유 간선 17개, 미존재 끝점·순환 0개, 중앙에서 16개 도달, 16개 선택 handler의 상세/aria 상태, 오래된 `mana3` 선택의 중앙 fallback, 외부 원본과 저장소 사본의 바이트 일치다. 브라우저 렌더링과 실제 브라우저 pointer/key 입력은 실행하지 않았다.

## 최종 실행 결과와 범위

| 검증 | 확인 결과 | 근거 |
| --- | --- | --- |
| 최신 Editor 빌드 | `Succeeded`, 103.04초 | `build-02.log` |
| 자동화 | Success 3개, errors=0, 나머지 집계 0개 | `automation-PolishAuto02.log`, `automation-report/index.json` |
| 실제 UI/입력 런타임 | PASS 176개, FAIL 0개, baseline/input 두 marker 모두 `PASSED failures=0`, 프로세스 exit 0 | `runtime-PolishRun02.log`, `runtime-results.txt` |
| 새 프로세스 읽기 | PASS 77개, FAIL 0개, baseline/input 두 marker 모두 `PASSED failures=0`, 프로세스 exit 0 | `read-PolishRun02.log`, `runtime-read-results.txt` |
| PNG | 1080p/720p 새 도형·라벨 및 active-drag의 노출 HUD 확인 | `brawler-1080p.png`, `greatsword-720p.png`, `input-brawler-drag-active.png` |

런타임 결과에서 실제 MainHUD Primary 장착 2회, E 회피 슬롯 거절 2회, 메뉴 닫기 시 pointer release 이전의 owned drag 취소 2회, drop/cancel 후 표현 복원 7회, 잠긴 격투가 노드의 drag 생성 거절 1회가 각각 PASS다. 각 직업의 실제 키 선택·해제·재지정·Esc/예약 E 거절 및 대표 기술의 정확한 1회 실행도 PASS다. 실제 HUD 경로는 표시된 MainHUD 아래 `BottomHUDWidget`의 슬롯으로 기록되어 숨은 장비 preview와 구분된다.

메뉴 닫기 중 취소 검사는 실제 learned drag를 시작한 뒤 controller의 메뉴 상태 전환을 실행한다. pointer up 전에 Slate operation 제거·표현 복원·메뉴 닫힘·loadout 불변을 확인하고, 다시 연 메뉴의 선택도 확인한다. 이 사례의 controller 전환은 직접 호출이며 물리 K 키 이벤트를 검증했다고 주장하지 않는다. 드래그·drop 및 키 선택/발동 검사의 엔진 입력 경로와는 별도 lifecycle 검사다.

새 프로세스 읽기는 일반 AttachCharacter 경로로 같은 QA 프로필과 v3 파일을 복원한다. LV15/대검, 인벤토리·장비/재화, 특성 실제 지출 1P, 격투 숙련 M12/XP385와 대검 M20/XP1045, 트리 실제 지출 8P와 공용 잔액 5P, 마나 확산/공명, 격투가 F7과 실제 HUD에 배치한 대검 Primary/F8의 저장을 확인했다. 임시 아이덴티티 게이지·쿨다운은 0에서 시작한다. 이후 대검의 실제 drag/drop·E 거절·키 선택과 발동·메뉴 비활성 취소·복원을 다시 실행해 통과했다.

PNG에서는 정상 창의 마름모·공명 이중 원과 상태/라벨이 표시되고, active-drag 중에는 창이 흐려지며 기술명/기존 아이콘 drag visual과 실제 HUD가 보인다. 이미지 확인은 위 세 장에 한정하며 모든 해상도나 입력 장치를 포괄하지 않는다.

이번 검토는 해당 스킬트리 표현과 대표 기술의 입력 경로 보완에 한정한다. 긴 플레이 세션, 패키징/Shipping, 멀티플레이, 전체 무기·애니메이션 및 별도 archive-v2 후속 통합은 범위 밖이다.

## Run01 이후 추가 검토

다음 내용은 위 초기 정적 검토 이후의 기록이다. `build-01.log`에서 Editor 빌드 `Result: Succeeded`, 실행 시간 293.46초를 확인했다. `automation-report/index.json`은 성공 3개, 나머지 집계 0개이며 세 테스트 모두 `Success`, errors=0이다. 이 결과는 이후 drag 보완 전 소스의 결과다.

`runtime-PolishRun01.log`와 `runtime-results-attempt-01.txt`는 `failures=5`인 실패 실행이다. 외곽 노드 선택, 키 선택/취소/예약 E 거절/해제/재지정 및 대표 기술 1회 발동 검사는 성공했지만, drag의 실제 hit path 검사 다섯 개가 실패했다. 전체 런타임 통과로 보지 않는다.

### 숨은 preview와 실제 HUD 가림

QA의 슬롯 탐색은 이제 표시된 `MainHUD`의 소유 관계·viewport·동일 플레이어와 실제 Slate ancestry를 요구한다. 숨은 장비 preview의 남은 geometry로 drop 대상을 고를 수 있던 모호함을 줄이며, 마지막 drop 지점은 계속 `LocateWindowUnderMouse`로 다시 검사한다.

실제 화면에서 K 창이 HUD를 덮는 문제가 확인되어 제품 UI도 보완했다. 학습 기술의 drag가 시작되면 기존 visibility/opacity를 저장하고 창을 `HitTestInvisible`, 기존 불투명도의 12%로 바꾼다. 메뉴의 열림·게임 입력 차단 상태는 유지한다. 실제 operation의 drop/cancel 뒤에는 저장한 표현 상태를 복원한다. 기술명과 기존 아이콘으로 만든 drag visual은 입력을 받지 않는다.

QA는 이제 drag 시작과 HUD drop 사이에 각각 다음 프레임을 기다린다. dim/히트 테스트 상태와 active-drag PNG를 확인한 뒤 실제 HUD hit path로 이동/해제하고, 다시 다음 프레임에서 메뉴 표현·선택·바인딩 복원을 확인한다. 같은 프레임의 오래된 hit grid나 숨은 preview로 통과시키는 방식이 아니다. E 거절은 실제 취소 경로, Primary는 실제 수락 경로를 검증하며 최종 Run02에서 두 직업 모두 통과했다.

### 수명·GC·delegate 확인과 추가 보완 요청

- 로컬 UE 5.8의 `FUMGDragDropOp::AddReferencedObjects`가 `UDragDropOperation`을 유지하고, operation의 `DefaultDragVisual`과 프로젝트 payload의 `SkillData`는 `UPROPERTY`다. 패널의 operation 캐시는 비소유 `TWeakObjectPtr`이며 해당 캐시가 GC 소유권을 대신할 필요가 없다.
- 종료 callback은 `UFUNCTION`이고, 현재 operation과 일치할 때만 처리한다. 복원 함수는 drop/cancel의 두 delegate를 모두 제거하고 캐시를 비운다. 표현 복원은 boolean guard로 중복 호출에 안전하도록 작성됐다.
- 정적 검토 중 추가 종료 조건을 발견했다. 비활성/`ReleaseContext`가 표현만 복원하고 delegate를 제거하면 Slate가 보유한 drag operation은 남을 수 있었다. K 닫기나 프로필 전환 뒤에도 이전 source/skill payload가 유지될 수 있으므로 소유 operation의 운영 취소를 요청했다.
- 최종 소스의 `CancelOwnedSkillDrag` 반영을 다시 확인했다. 이 helper는 Slate 초기화, `FUMGDragDropOp` 타입, `GetOperation()==ActiveSkillDrag.Get()`를 모두 확인한 경우만 `CancelDragDrop()`을 호출한 뒤 표현을 복원한다. 비활성/`ReleaseContext`가 이 helper를 사용하며, 일반 복원 함수와 drop/cancel callback에는 취소 호출이 없다. 다른 화면의 drag를 취소하지 않고, 동기 cancel callback 후 중복 복원도 guard로 처리한다.
- 위 종료 조건의 소스 보완을 확인했으며 추가 정적 검토에서 미해결 차단 결함은 발견하지 못했다. 최신 Run02에서 실제 정상 drop·E 거절/cancel·메뉴 복원과 pointer release 이전의 메뉴 비활성 취소도 통과했다. 별도 새 프로세스 읽기에서도 대검의 같은 입력·수명 경로와 디스크 저장 복원이 통과했다.
