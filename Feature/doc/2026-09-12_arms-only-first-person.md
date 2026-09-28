# 팔 전용 1인칭 격투 시야

- 날짜: 2026-09-12
- 브랜치: Sprint#4-27-arms-only-first-person
- 엔진: UE 5.8.2
- 상태: 팔 전용 자산·런타임 연결·빌드·시야·전투 회귀 검증 완료. 원본 영상 직접 비교는 도구 오류로 미완료

## 변경 결과

기본 격투와 너클의 로컬 화면에는 팔 표면만 표시한다. 원래 1인칭 전신 Manny에서 목·다리만 숨겨 몸통·골반이 남았던 구조를 별도 팔 메시로 바꿨다. 마우스 시야 제한을 좁히거나 팔을 멀리 이동해서 가리지 않았다.

새 SK_FirstPersonArms는 Unreal Editor의 MeshDescription API로 제작했다. 89개 전체 뼈의 이름·부모·기준 자세와 재질 2개를 보존하고, upperarm 자손의 가중치가 절반 이상인 정점으로 구성된 면만 남긴다. 세 LOD의 생성 후 렌더 데이터도 검사했다. 원본 Manny의 SHA256은 유지됐다.

머리·척추 뼈는 계속 포즈를 평가하므로 기존 상체 선행·머리 후행과 표시 카메라 동조를 유지한다. 기본 공격·차징·타격 판정·점프 수치는 변경하지 않았다. 다른 무기로 전환하면 메시·재질 override·애니메이션 설정·단일 노드 재생 상태·명시 숨김 본·부착 위치·렌더링 설정을 복구한다. 소유자 시야에서 전신 본체는 숨기되 월드 그림자는 기존 설정을 따른다.

## 변경 파일

- Source/Rogue10m/Components/Rogue10mFirstPersonPresentationComponent.h/.cpp: 팔 메시 선택과 원래 표시 상태 보존·복구.
- Source/Rogue10m/Editor/Rogue10mFirstPersonArmsAuthoring.cpp: Editor 전용 형상 분리와 모든 LOD 검증.
- Source/Rogue10m/Rogue10m.Build.cs: Editor에서만 MeshDescription·SkeletalMeshDescription·UnrealEd 의존성.
- Scripts/Editor/CreateFirstPersonArms.py: 실제 플레이어 CDO의 원본 메시 확인, 복제·검증·신규 팔 자산만 저장.
- Content/Rogue10m/Character/SK_FirstPersonArms.uasset: 신규 팔 전용 자산.
- Source/Rogue10m/Tests/Rogue10mArmsOnlyRuntimeTest.cpp: 시야·공격·점프·메시 복원 검사.
- 기존 BasicBrawler/FirstPersonFist 테스트: 실제 마우스 시점 입력 분리. 기존 조준 불변과 POV 기준 유지.

## 검증

- 통합 빌드 118.35초, 신규 테스트 포함 21.66초, 극단 각도 fixture 보완 후 10.22초 성공.
- Editor 자산 저장 성공. LOD0/1/2 렌더 정점 20506/14757/7331, 팔 이외 정점 모두 0. 원본 LOD0 92178면에서 팔 38134면 유지.
- 실제 팔 메시 경로를 새 게임 프로세스에서 확인. 최초 아래보기 시험의 -89도 요청이 기존 최소 -70도로 제한된 것을 assertion으로 발견했다. 테스트에서만 제한을 잠시 확장하고 복원해 실제 -89도까지 추가 검증했다. 프로덕션 제한 -70도 유지.
- 최종 시야 검사 RESULT=ARMS_ONLY_PASSED frames=390 failures=0. 1280×720·1720×720에서 -89/-70/0/+70도, 아래보기 좌우 연타·차징·어퍼컷·점프, Staff 복구 및 Knuckle/Unarmed 재진입.
- 머리 신호 1.267도, 점프 높이 89.989cm, 최대 손 프레임 간 이동 11.144cm.
- 독립 소스 검증과 실제 대표 14장 시각 검증 통과. 몸통·골반·다리 표면, 어깨 절단 구멍, 새 손목 뒤집힘을 발견하지 못했다.

첫 기본 전투 회귀는 실행 중 조준 방향이 바뀌어 실패했다. 외부 시야 입력 간섭 가능성을 분리하기 위해 테스트에서만 IgnoreLookInput을 사용하고 종료 시 해당 스택을 해제한다. 직접 ControlRotation 변경·카메라 불변 검사와 실제 공격 호출은 그대로 유지한다. 프로세스 종료 코드만으로 통과를 판정하지 않고 RESULT 로그를 대조한다.

## 레퍼런스 비교와 한계

고릴라 암즈에 대한 공식 자료와 현재 구현의 비교·적용 방향은 evidence/arms-only-20260912/reference-comparison.md에 정리했다. 현재 몸·머리·팔의 연결과 빠른 신전·곡선 회수는 유지하고, 이번에는 아래보기의 전신 노출을 해결하는 데 집중했다. 추가 무게감은 실제 접촉 순간의 반동·적 반응·소리를 함께 조율할 후속 항목이다.

computer-use 업데이트 버전으로 영상 접근을 재시도했으나 초기화 단계의 Windows ACL 오류로 재생 프레임을 얻지 못했다. 고릴라 암즈 원본과 동작을 직접 비교하거나 일치시켰다고 주장하지 않는다. 기존 Manny의 흰 전완과 높은 가드 실루엣은 남아 있다.

관련: Feature/architect/2026-09-12_arms-only-first-person.md, Feature/doc/evidence/arms-only-20260912/, Feature/doc/images/arms-only-20260912/, DevLog/20260912.txt.

커밋·푸시는 하지 않았다.

## 최종 통합 결과

- 입력 격리 테스트 빌드 20.00초 성공.
- RESULT=ARMS_ONLY_PASSED frames=390 failures=0.
- RESULT=BASIC_BRAWLER_PASSED frames=480 failures=0. 일반 어퍼컷 피해31.8·강화36.2, 몸통 차징 회전7.57도, 머리5.91도·최종 카메라1.27도, 점프89.99cm, 손 프레임 간 이동11.144cm. 기존 조준·카메라 불변 및 취소·자원·쿨다운 검사 유지.
- RESULT=FIST_PRESENTATION_PASSED frames=120 failures=0, VIEW_AUDIT completed=3. 기존 너클 최종 카메라1.493도, 무기·사망·카메라 강도0 복구 확인.
- git diff --check 및 CheckGeneratedChanges 경로 검사 통과. 기존 다른 미커밋 바이너리 변경 경고는 보존된 이전 작업을 포함한다. 이번 신규 자산은 Editor에서 생성·저장했다.

입력 분리 후 조준 불변을 포함한 회귀가 통과해 첫 실패의 외부 시야 입력 간섭 가능성과 일치했다. 이 결과를 물리 마우스 반응 테스트까지 완료했다는 의미로 확대하지 않는다. 일반 실행의 입력 경로와 카메라 제한은 바꾸지 않았다.

실제 전투 프리뷰: images/arms-only-20260912/arms-only-combat.gif (300프레임, 10초, 800×450). 프레임 수와 총 재생 시간을 확인했다. 이는 30Hz 테스트의 실제 렌더 화면이며 실시간 성능 벤치마크 수치를 의미하지 않는다. 아래보기14장과 전투 주요 구간15장을 독립 시각 검토했다. 모든 연속 프레임이나 외부 원본과의 동작 일치를 검증했다고 확대하지 않는다.
