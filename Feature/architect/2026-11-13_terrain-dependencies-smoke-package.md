# 비GUI 지형 보존 조사와 내부 Development smoke package

체크포인트 C 이후 부모의 명시적 후속 지시에 따른다. Computer Use, 화면·마우스·키보드 조작 및 같은 GUI 입력을 다른 API로 우회하는 행위는 계속 금지다. 이 Work만 공유 원본·Editor/UBT/Cook 실행권을 보유한다. 외부 Codex의 작업 여부와 변경은 매 실행 전 재확인한다.

## Ultrawork Packets

| Packet | 목표·수정 범위 | 완료 조건과 검증 | 복구 경계 |
| --- | --- | --- | --- |
| D0 재개 확인 | 현재366 Source/Config·DLL·branch·프로세스, 기존 Content/후보 기준 | checkpoint C와 일치 또는 차이를 보존·설명 | 새 evidence만 |
| D1 읽기 전용 조사 API | 소유 CompactWorldAuthoring2파일 v3, 별도 staged 검토 | 기존 RG/paint/metadata digest 유지, raw EditLayer B/A 값·의미/범위, layer GUID·brush·PCG·spline·foliage·외부/actor 참조와 관찰 한계 보고; UHT/Editor·commandlet 검증 | 소유 helper2파일 v2의 사전 SHA와 물리 백업. Content 변경 없음 |
| D2 공개 crop 매핑 | 보존 가능한 공개 데이터 API의 좌표·레이어·의존 객체 매핑 | 실제로 보존이 입증된 경우에만 독립 새 후보에서 저장/재로드·비GUI 충돌 검사. 아니면 최소 차단과 다음 인벤토리 명시 | 새 후보만, 기존 source/candidate A 보존 |
| D3 내부 Windows smoke 준비 | 격리 Windows Work의 별도 runner·manifest 검토 | release/B/B2 봉인·strict runner 변경0. map1800m/미축소·NPC미배치·UI미검증을 명시한 새 물리 출력/이름, 원본/선택맵+외부패키지 입력 동결·동일DLL회귀·저장보호·소유PID 기록 | 새 internal 후보·출력만 |
| D4 내부 빌드 | 새 Developer 전용 Game Win64/UAT cook·IoStore·archive | 실제 종료값과 로그/입력·전후 SHA·archive contents 기록. 실패를 ignore하지 않고 미검증 내부 빌드로만 전달 | 새 출력만, 기존 ArchiveE/B/B2/release 원본 미변경 |

현재 후보 A는1800×1800m·1360개 editor actor·256 render/collision·16 proxy다. 소스 통합된 NPC 의뢰의 Director/DataAsset은 미배치다. 미래 목표1012.5×562.5m 9×5 grid는 아직 계획이며 현재 실제 크기로 표시하지 않는다. 현재 native43은 숫자/타이머/물리 질의 모델이며 실제 애니메이션/GUI/입력/완주 인증이 아니다.

public Import 검토상 raw B/A alpha/flags, 새 layer GUID, brush 소유권 이동, PCG/foliage/spline 및 외부 component reference 보존이 빠져 있다. 이번 D1은 이 간극을 읽기 전용으로 줄이며, 누락이 없다는 추정을 하지 않는다. live UObject hard reference와 serialized package/soft reference는 별도 관찰 범위로 표시한다. null/로드되지 않은 자산·참조도 누락하지 않고 unknown으로 남긴다. native GUI Manage/Delete는 금지 상태로 유지한다.

D4는 사용자가 요청한 실제 개발 빌드를 진전하기 위해 새로 명시 허용된 **미검증 내부 Development smoke package**다. strict release gate와 UI fixture를 완화하거나 대체해 통과했다고 주장하지 않는다. 실제 Windows UI·DX12·저장·재시작·Compact 이동 검증은 별도 미완료로 유지한다. 최종 source/assets가 바뀌면 이전 DLL·회귀 결과를 새 입력 증거로 재사용하지 않는다.


## 2026-11-13 추가 패킷: Game 테스트 guard 및 내부 빌드 재검증

- 목표: 실제 Game Win64 Development 빌드에서 발견한 Editor 전용 montage fixture의 컴파일 범위를 바로잡고, 새 DLL 회귀 후 별도 내부 빌드를 재시도한다.
- 변경 범위: `Source/Rogue10m/Tests/Rogue10mAuthoredContactSequenceTests.cpp` 첫 줄의 `WITH_EDITOR` 조건 추가, native 검사 실행기의 `NativeTestsD` 출력 이름 추가. 기존 23개 Editor 테스트와 assertion은 유지한다.
- 완료 조건: 정규 Editor 빌드 성공, 새 DLL의 정확한 native 43개 및 전체 로그 RED 0 확인, 새 입력 봉인, 정규 Game/cook/package 결과 기록. 실패 시 완료로 주장하지 않는다.
- 검증: `Scripts/BuildEditor.ps1 -NoHotReload`, `Scripts/RunReviewedNativeTests20261113.py --execute --label NativeTestsD`, 별도 UTC 수정 wrapper의 strict 내부 smoke 절차.
- 복구 경계: `GameGuardFixD/`에 수정 전 두 파일과 SHA를 보관한다. 사용자 WIP나 원본/후보 Content를 되돌리지 않는다. 기존 실패 출력과 seal도 보존한다.
- 현재 사실: run02 UAT exit 6, Game 컴파일 오류 1개, Cook 미시작. 실패 후 입력 10,456개·Editor 6제품·저장 2root 불변. guard 수정 뒤 Editor exit 0, 12.2146초. NativeTestsD는 실행 중이다.
- 지형: 1800m 후보이며 축소 미실행. 단일 Edit Layer raw와 혼합 결과를 구분하고 Collapse All Layers는 사용하지 않는다. height/paint/B/A export만으로 편집 구조·브러시·참조 보존 완료를 선언하지 않는다.
- 도구 제한: Computer Use 및 GUI 입력·우회 0회 유지. UI/시각/물리 입력과 완주 검증은 미검증이다.
