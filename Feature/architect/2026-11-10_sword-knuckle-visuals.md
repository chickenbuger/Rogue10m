# 대검·너클 표시 및 모션 적합성

- 작업 브랜치: Sprint#9-29-humanoid-boss-npc
- 목표: 기존 장비 상태에 연결되는 프로젝트 제작 대검과 좌우 너클을 추가하고 현재 모션/판정 적합성을 실제 근거로 구분한다.
- ULW 1: 원본 OBJ/MTL과 UE StaticMesh 제작. 수정 영역 Art/Weapons/Starter20261110, Content/Weapons/Starter20261110, Scripts/Editor/PrepareWeaponVisuals20261110.py. 완료 조건: 원본 보존, 비어 있지 않은 메시와 머티리얼 로드, 충돌 없음.
- ULW 2: 이벤트 기반 WeaponVisual ActorComponent. Character의 초기화/외형/장비 변경에만 연결, Tick·게임 피해 계산·저장 형식 변경 없음. 완료 조건: 대검 오른손 1개, 너클 좌우 2개, 다른 장비 숨김. BuildEditor 및 자동 테스트.
- ULW 3: 실제 로드되는 몽타주/손 위치/판정 시간 감사. 너클 전용 FistAuthored는 보존. 대검 맨손 대체 모션이 발견되어 전용 양손검 모션 후보를 별도 검토한다. 숫자 검사와 렌더/물리입력 검증을 구분한다.
- 자산 출처: 무기 메시/머티리얼은 이 작업의 신규 절차적 원본이며 외부 모델·텍스처를 사용하지 않는다. 기존 너클 전용 모션과 Fab Close Combat의 신규 소유·사용권 검증 결과는 별도 보고서와 연결한다.
- 롤백 경계: 신규 무기 자산/코드/원본 및 Character의 무기 표시 연결 줄. 다른 작업자의 보스/NPC/빌드 변경은 보존.
## 대검 재개 packet ResumeSword1

- 목표: 누락된 대검 작업 복제본을 저장하고 준비/방향 이동과 양손 부착을 실제 프로젝트에 적용한다. 이후 검수한 다섯 공격 컷의 연결과 피해 시작 시점을 별도 검증한다.
- 경계: 원본 FBX, 기존 여덟 source clip, 무기별 일반 retargeter, 너클 모션, 스킬 비용/피해/쿨다운/슬롯, 사용자 저장 파일을 보존한다. 1인칭 신규 개발은 보류한다.
- 이동: 원본 BlendSpace의 27개 좌표와 17개 고유 clip을 유지하면서 작업 A_Armed_ 파생본을 연결한다. 공통 AnimBP의 실제 guard Alpha/B 연결과 Walk / Run player만 수정한다.
- 그립: 실제 렌더 body의 두 palm으로 shaft 방향과 원점을 계산하고 기존 finger-Y roll을 유지한다. bone-finalized event를 사용하며 비정상 좌표는 static fallback, 외부 부착/transform 변경은 소유권을 넘긴다.
- 검증: 격리된 source pose와 실제 게임 렌더를 구분한다. 이동 20케이스와 원래 무기 10개 스킬을 검증하고, 새로운 공격 연결 후에는 새 몽타주/양수 hit delay와 첫 피해의 blade-to-capsule 간격을 별도로 확인한다. 수치/캡처 성공을 물리 충돌이나 실제 skeletal surface 접촉 승인으로 쓰지 않는다.
- 실패 보존: 기준 자세로 찍힌 초기 PNG, bone track이 비어 있던 Alt 후보와 실패 로그는 덮어쓰지 않는다. FbxFactory의 전체 import 옵션 경로에서 새 Repair1 후보를 만들고 실제 track 수와 시간별 손 움직임을 검사한다.
- 도구: 실제 UE5.8 공식 MCP의 목록/스키마/프로젝트 스킬을 확인한 후 Niagara 제작·편집·compile/readback·저장에 사용한다. MCP에 AnimSequence FBX import 경로가 없으므로 해당 부분은 승인된 Editor API를 사용한다.
- 공유 실행: 맵 작업과 Editor/build 구간을 직렬화한다. 맵 작업 중에는 파일 수준 준비만 진행한다.
- 배포: 로컬 제작/상용 게임 이용과 공개 source asset 재배포를 구별한다. Mixamo/Fab 원본 및 재사용 가능한 파생 애니메이션은 개별 권리 확인 전 자동 public commit/push 대상에서 제외한다.
