# 편집 가능한 스킬 VFX 재개 결과

공유 프로젝트는 읽기만 했다. 실제 수정은 물리적 격리 사본에 있으며 Editor/MCP/UBT/Cook/게임 실행·종료와 공유 Git 작업은 없었다. 통합은 공유 수정 사용권을 가진 담당 Work가 수행한다.

## 이번 구현

- 스킬별 VFX 사전 검사가 저장된 `{"refPath": null}` 형태의 Charge 참조를 기존 바인딩 계약과 같은 방식으로 읽도록 수정했다. 참조 포장만 비교용으로 풀고 원본 속성은 바꾸지 않는다. 실제 Charge 참조·잘못된 포장·외부 패키지는 계속 보호하거나 거부한다.
- `VfxPacketReadiness20261113.py`를 추가했다. 기준 파일과 후보 파일의 SHA256을 비교해 이미 반영, 추가 가능, 수정 가능, 충돌을 분리한다. 경로 이탈·링크·관찰된 검사 중 변경은 차단하며 프로젝트 파일을 적용하지 않는다.
- Staff 캡처의 요청 시각뿐 아니라 PNG 처리 시각도 cue age와 모션 시간창 안에 있어야 통과하도록 고쳤다. 지연된 콜백을 정상 캡처로 인정하던 검증 누락을 보완한 것이며 피해 시각·모션 재생·캡처 요청 시각은 바꾸지 않았다.
- Staff 가독성 C++ 패킷과 스킬별 연결 도구를 현재 원본 기준으로 묶는다. 이전 패킷 전체 설치자를 위한 delta도 제공한다. 원본·후보 해시와 패치 실제 적용 검사는 독립 사본에서 수행한다.

크기·색·재질·발생 위치·수명은 기존 DataAsset/Niagara/MIC에서 조정한다. 임의의 0.5초 상한이나 고정 재질 수는 도입하지 않는다. 기존 피해 타이밍·이동·카메라·피격 취소·catalog를 보존하며 면역은 추가하지 않는다. 이미 생성된 Cast/Impact는 현재 타이머로 정리되므로 피격 즉시 모든 VFX가 사라진다고 보장하지 않는다.

## 현재 연결 상태와 근거

현재 공유 소스 4개와 기존 도우미 4개의 해시는 이전 전달 기준과 같다. 새 스킬별/Staff 가독성 도구 9개는 공유에 없다. Staff Primary01의 Telegraph/Impact 연결은 보관된 저장·cold 증거가 있지만 이번 후보의 3인칭 가독성 완료 증거는 아니다. 다른 Staff 4개에는 legacy Cast/Impact가 있고 Charged에는 Charge 참조가 있으므로 비어 있는 스킬로 취급하지 않는다.

GreatSword Charged와 Knuckle Primary03은 기존 pilot로 보호한다. 남은 legacy 8개만 한 번에 하나씩 staged Cast/Impact 연결 대상으로 준비했다. 신규 selectable technique catalog를 채우는 구현은 아니다. Trajectory는 연속 리본·접촉 이벤트가 아니므로 연결하지 않는다. Magic Circle/Release/지속 Charge의 새 이벤트 권한은 만들지 않았다.

Fab 소유팩 확인, 로컬 파일 존재, 보관된 실행 증거, 현재 실행 확인을 분리했다. Wizard for Battle Polyart와 Combat Magic Animations의 소유·호환성은 과거 기록이며 현재 계정 재확인은 하지 않았다. 구매·다운로드·외부 전송은 없었다.

## 검증 범위와 통합 순서

새로 실행한 Python 계약 검사, 새 통합 검사, 정적 C++ 보존 검토 결과는 전달 폴더 `validation-summary.json`과 해당 로그에 기록한다. 기존에 통과했던 51/16/36개 검사는 과거 결과이며 이번 실행과 따로 적는다. fake MCP 검사는 실제 UE reflection·저장·렌더 증거가 아니다.

1. 통합 담당자는 다른 Codex의 변경과 공유 Editor/빌드 사용권을 조율한다.
2. 현재 파일에 대해 full 또는 정확한 이전 패킷 기준 delta manifest를 다시 검사한다. 서로 다른 기준의 패치를 함께 적용하지 않는다. 충돌하면 최신 원본을 유지하고 패킷을 재기준화한다.
3. Staff reflected 속성 변경을 UHT/Editor 빌드로 반영한 뒤 fresh inspect·unbound stage·save/readback·단일 활성화·cold reload를 수행한다. 현재 CIM listener/project identity 검사는 이 Work에서 권한 때문에 실행할 수 없었고 실제 live 단계에서도 확인이 필요하다.
4. Staff 31장 캡처의 요청/처리 시각·나중 age·실제 active montage 회복 구간을 확인하고 기본 3인칭과 보조 시점에서 모션/가림/효과를 검수한다. 피해 시각은 그대로 둔다.
5. 실제 입력·피격 취소·전체 전투·Cook/Windows 패키징을 검수한다. 독립 연결 도구만 통합하면 Staff 배치 C++ 빌드는 필요 없지만 이 full 패킷에는 C++ 변경이 포함된다.

실제 에디터 연결·새 저장·cold reload·3인칭 cue·Cook·Windows 패키징은 이번 Work에서 실행하지 않았다. 시각 완료 승인은 보류다.
