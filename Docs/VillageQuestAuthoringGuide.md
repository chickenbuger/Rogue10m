# NPC·의뢰 값과 콘텐츠 편집

이 안내는 격리 구현 코드 기준이다. 통합 담당자의 UHT/Editor 빌드 이후 실제 에셋을 만들어 사용한다. 현재 원본 프로젝트에 이 에셋을 만들거나 맵에 배치한 것은 아니다.

## 기본 작업 흐름

1. 프로젝트 소유 폴더에 `Rogue10mVillageQuestDataAsset`을 만든다. 장소나 보상안을 비교할 때는 에셋을 복제해 별도 버전으로 둔다.
2. `SupportedWorld`에서 사용할 맵을 고른다. `LayoutOriginCm`과 `LayoutRotation`으로 전체 의뢰 배치의 기준점과 방향을 정한다. 좌표는 로컬 cm이며 원본 지형 크기나 캐릭터 속도를 바꾸지 않는다.
3. `Quests`를 펼쳐 아래 항목을 조정한다. NPC 위치와 목표 위치는 같은 기준점을 사용하고 거리 안내도 변환된 위치를 읽는다.
4. 맵의 `VillageQuestDirector.QuestDefinition`에 만든 에셋을 연결한다. 새 월드의 정상 런 진입·GameMode/GameState는 맵 담당자의 기존 경로를 사용한다.
5. `Validate Configuration`을 실행해 `LastValidationResult`를 확인한다. 형식 검사는 실제 자산 존재·충돌·동선·렌더 확인과 별개다.
6. 새 PIE/런에서 실제 F 선택, 목표, 보상, 재방문을 검증한다. 진행 중인 런의 ID·목표 개수·세계 스코프를 변경하는 상태 이행은 지원하지 않는다.

| 항목 | 바꾸는 곳 | 반영되는 동작 |
| --- | --- | --- |
| 대상 맵 | SupportedWorld | Director·컴포넌트의 허용 맵 검사 |
| 전체 배치 기준 | LayoutOriginCm / LayoutRotation | NPC·목표 위치와 방향, 화면의 표식 거리 |
| NPC 이름·소개·진행·완료 후 대사 | Quests의 Dialogue | F 대화 상태별 문구 |
| NPC 외형과 모션 | NpcPresentation | 기존 TutorialData의 mesh·idle/talk/walk 설정 재사용 |
| NPC 대화·탐지 거리 | NpcInteractionRadiusCm / NpcDetectionRadiusCm | 기존 F 입력/거리 이탈 수명 관리 |
| 목표 문구·위치·종류 | ObjectiveLabels / ObjectiveLocations / CombatObjectiveIndex | 탐색 또는 실제 귀속 처치 목표 |
| 목표 모양·표시·반경 | ObjectivePresentation | mesh·scale·표시 여부·글자 offset/회전/크기/색, 수집 반경 |
| 일반 전투 목표 | CombatMonsterData | 해당 일반 Monster Data의 외형·스탯·공격·드롭을 사용 |
| 보상 | RewardGold 또는 RewardItem/RewardQuantity | 아이템·재화 중 한 종류를 보고 때1회 지급 |
| 선택 이야기 역할 | SupplyClueObjectiveIndex / bOffersClueAnalysis / bAcceptsClueReport | 단서 발견→분석→원래 장소 보고 |
| 선택 이야기 문구8개 | OptionalStoryText | 추가 선택지·안내·성공 반응 |

목표2개의 표현 설정은 의뢰 행의 `ObjectivePresentation`을 공유한다. 독립 배치한 목표 액터는 같은 `Presentation` 구조를 에디터에서 조절할 수 있다. custom marker mesh를 해제하면 기본 Cube로 돌아가며, 표시를 꺼도 수집 반경은 별도로 동작한다.

## 데이터 검증과 적용 경계

상호작용 반경은100cm 이상이며 탐지 반경은 그보다50cm 이상 크다. 목표 반경·텍스트 크기·mesh scale은 양수의 유한 값이어야 한다. 탐색 목표에는 몬스터 데이터가 필요 없고, 전투 목표에는 로드 가능한 일반 몬스터 데이터가 필요하다. Director는 필수 전투 데이터를 먼저 확인한 뒤 NPC/목표를 만든다. 중간·최종보스는 기존 보스 배치/정산 시스템에서 별도로 다룬다.

보상은 현재 골드 전용 또는 아이템 전용이다. 섞인 보상은 부분 지급을 막기 위해 거부한다. 가방이 가득 차면 보고 가능 상태를 유지한다. 선택 이야기는 추가 보상이나 통행 조건을 만들지 않는다. 선택 이야기 역할을 모두 끄면 문구 없이 기본 의뢰만 사용할 수 있다.

현재 모델의 논리 의뢰5개·각 목표2개·싱글플레이·런 수명은 코드 계약이다. 실제 맵에 큰 마을5개를 강제하는 의미가 아니다. 의뢰/목표 개수의 확장, 저장 중인 런의 실시간 스키마 변경, 혼합 보상 거래는 별도 구현·검증 대상이다.

## 최신 월드 기준에 맞추기

현재 원본의 `Docs/WorldMapProductionGuide.md`, `Docs/References/WorldMap/README.md`, 배치/획득처 JSON과 지도 PNG를 우선 참조한다. 큰 거점3개와 전문 초소5개·일반15종/중간4/최종1의 최신 계획을 과거 동급5마을 배치로 바꾸지 않는다.

C++ 기본 데이터는 이전 FiveVillageGraybox 시험 맵을 위한 회귀 기본값이다. 새 Landscape 월드에는 새 Data Asset을 만들고 최신 노드·실제 로컬→월드 앵커·동선과 맞춰 작성한다. 이 코드가 새 월드에 NPC·몬스터·비급을 자동 배치하지 않는다.

일반 의뢰의 보상에는 최신 획득처 정책을 따른다. 중·상위 비급을 확정 지급하는 보상창으로 바꾸지 않는다. 입문/하위 보상의 예외는 현재 제작 가이드와 실제 아이템 매핑을 확인한다. 이 형식 검사는 비급 등급·자산 소유·학습 조건을 대신 검증하지 않는다. vendor 원본은 보존하고 필요한 프로젝트 소유 파생 에셋을 연결한다.
