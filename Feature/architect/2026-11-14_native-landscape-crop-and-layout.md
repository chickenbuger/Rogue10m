# 실제 Landscape 축소와 편집형 배치

사용자의 최신 지시인 실제 맵 축소·배치 준비를 적용한다. 기존 문서의 전체 필드 배율 축소 금지는 지형 컴포넌트의 안전한 외곽 정리를 막는 것으로 확대하지 않는다. 이동속도나 전체 액터 배율로 직행 시간을 맞추지 않고 콘텐츠 밀도·실제 동선을 검증한다.

## Ultrawork Packet

- 목표: 최신 Compact 후보를 원본으로 보존하고, 별도 정상 제작 후보에서 native Landscape Manage/Delete로 불필요한 외곽을 정리한다. 최신3큰거점+5초소/HMQTF·양방향 갈림길·우회/재방문과 탐험 비급 계약을 유지한다.
- 입력: 최신 전체10472 입력 및 Editor6, Compact의 NPC/Director/의뢰DA, StartHub와 새 포털, 완성된 축소전 WindowsHubA와 기존B/E5/D, 지형 raw/semantic·층/GUID·참조 자료.
- 수정 위치: 새 독립 제작 후보와 프로젝트 소유 파생 데이터·전용 증거/도구. WorldLayout20261112·기존CompactA·StartHub·사용자Source/Config·vendor원본·기존패키지는 덮어쓰지 않는다.
- 후보 경계: 우선9×5컴포넌트(1012.5×562.5m), ROI X315..882/Y630..945를 검토한다. 900×562.5m 또는 다른 경계와 비교하며 actual actor/보스장 여유·접근로·편집층 보존을 우선한다. 치수 산술로 채택·완료를 선언하지 않는다.
- 완료 조건: 최신 물리 백업/SHA → 정상 에디터 연결/소유/dirty 확인 → 별도 후보의 층·raw·paint·참조/Actor 기준 → 정확한 선택 preview → 외곽 삭제 → retained 데이터·층/GUID·Actor 및 bounds/컴포넌트·충돌/프록시 대조 → 정상 Undo/Redo → 저장과 독립 재로드·통행 검사. 완료되지 않은 항목은 분리한다.
- 검증: 정식 Unreal MCP/공개 API로 읽기·저장·인벤토리, native Landscape UI의 명시적 후보 조작, 전후 해시/공간/데이터/참조 기록. 단순 heightmap round-trip이나 AssetRegistry 조회만으로 보존을 인증하지 않는다.
- 복구 경계: 이번 최신 후보 전체와 관련 DA/Hub/사용자SourceConfig를 새 물리 백업으로 보존한다. 원본을 복구 대상으로 덮어쓰지 않고 실패 후보·로그를 유지한다. Import/CollapseAllLayers/private EdMode/임의DestroyComponent는 사용하지 않는다.

## 작업 경계

이 Work만 공유 에디터·빌드를 다룬다. 병렬 Work는 배치JSON/검사기·NPC 파생데이터를 격리 준비한다. 일반 CU는 허용됐지만 만료된 D 게임창 접근 재요청·대체 입력/창/런처 우회는 하지 않는다. root-startup-approval-A.json 생성과 의존 metadata-only Temp clone MapMaker QA는 자동심사 거절 후 별도 승인 대기이며 이번 제작으로 대신 수행하지 않는다.

새 WindowsHubA는50파일·SHA B9C2ED1B46F045C2CA80A2D6844EED5C1AA023B1AE9F47F2010311EB8A80A60B,1800m·미실행 버전으로 보존한다. 실제 신규 포털 여행/RunId/시계/NPC/F/saveIO·종료재시작·모션VFX·직행/15분 성장/20분 완주는 별도다.
