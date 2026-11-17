# Staff 지팡이 장착 조사·구현 준비 결과

2026-11-10 UTC. **현재 지팡이 실제 장착은 확인되지 않았으며, 장착 코드·시험·에디터 도구는 별도 작업공간에서 준비했다.** 공유 WeaponVisualComponent·에디터·빌드·에셋은 변경/실행하지 않았다. 커밋·푸시도 없다.

## 실제 확인

현재 `Staff` 타입은 존재하지만 `WeaponVisualComponent`는 GreatSword와 Knuckle만 시각화한다. `Wand` 별도 enum은 없다. 캐릭터 `GetMesh()`의 `hand_r` 부착 기반은 있으나 Staff 메시 참조가 없어 숨겨지는 상태다. 너클/대검의 기존 메시 연결은 이번 읽기 전용 조사에서 확인했으며 새 플레이/그립 QA를 수행한 것은 아니다.

[로컬 파일 조사](evidence/staff-equipment-20261110/local-asset-inventory.json)는 Content의 6,451개 `.uasset`에서 Staff/Wand/Scepter 계열 이름의 SM/SK/SKM 파일을 찾지 못했다. 모든 메시의 형상을 열람한 것은 아니다. 마을 팩의 Rod4종과 Beam1종은 파일·해시를 기록한 **미선정 후보**다. 10/19 My Library 보유 이력과 로컬 팩 동일 버전·현재 조건을 구분했으며, 건축 소품을 검증된 지팡이로 대신 세지 않았다.

사용자 지정 [Combat Magic Animations](https://www.fab.com/listings/05d90ca6-d294-4b93-a1b7-232214057850)은 애니메이션 소스다. [기존 조사](../../Docs/FantasyMotionVFXSources.md)의 보유/Standard/UE5.6–5.8 기록을 인용했고 공개 상품 페이지·[Fab 일반 조건](https://www.fab.com/eula)을 다시 확인했다. 계정·개별 취득계약 신규 검증이나 구매는 하지 않았다. [Mixamo 공식 FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html)도 확인했지만 새 다운로드·새 법적 동의는 없다.

## 독립 작업공간의 실제 코드

경로는 `tmp/staff-equipment-20261110/candidate/`이며 원본 .h/.cpp 스냅샷은 같은 작업폴더의 `baseline/`에 보존했다.

- WeaponVisualComponent 후보: `StaffMesh` soft 참조, `StaffAttachSocket`(기존 hand_r 기본), `StaffGrip` 조정값을 노출했다. Staff의 오른손 시각 선택과 왼손 잔류 숨김을 추가하고 검 전용 양손 보정은 그대로 검에만 남겼다. 메시·소켓 누락, NaN/비정규화 transform·0/음수 scale은 숨기고 진단한다. 검증된 모델이 없어 StaffMesh 기본값은 비어 있다. Identity grip은 미튜닝 fallback이다.
- `Source/Rogue10m/Tests/Rogue10mStaffVisualTests.cpp`: 구체 HumanMale BP를 이용한 장착/미지정/소켓/scale/전환/복원 시험을 준비했다. 기존 추상 Character 직접 spawn은 복사하지 않았다. 알려진 검 메시를 **합성 장착 fixture**로 쓰며 실제 지팡이 아트·인벤토리·동작 인증은 아니다. 예상 진단 경고를 명시한다.
- `Scripts/Editor/AuditStaffEquipment20261110.py`: 에셋 저장/할당 없이 로컬 목재 클래스·bounds·LOD0 삼각형/정점·재질·충돌, 손 bone 부착, 현재 Staff DA→몽타주 segment 원본/구간/rate/loop·HitDelay를 기록하도록 준비했다. fresh label과 원본 해시를 검사한다. 실제 플레이어 런타임/그립 검증을 대신하지 않는다.
- `Scripts/Editor/StageStaffAnimations20261110.py`: 기존 Fab StaffStrike/StaffShot의 메인 Manny 재생 후보와 HumanMale 렌더 검토 파생을 별도 fresh 경로에 준비하는 스크립트다. 실행·리타게팅·저장은 아직 하지 않았다. 세부 상태는 증거 summary를 따른다.

## 기존 모션 근거와 남은 편집

[보존된 원본 감사](evidence/humanoid-boss-npc-20261110/asset-audit.json)에서 StaffStrike2.0초·StaffShot2.9초·MagicStrike2 1.7초·ManaCastShot7.8초·SpellAndCastFireball5.4초가 기록되어 있다. 모두 CombatMagic 쪽 SK_Mannequin이며 메인 Manny와 다른 skeleton 자산이다. [과거 Staff 상세 감사](evidence/mouse-head-stable-locomotion-20261013/fresh-assets/asset-motion-audit.json)는 당시 전체 구간·segment rate1·loop1 및 별도 DA 재생률을 기록한다. 당시 5개 HitDelay0은 새 동작의 방출/피해 정합성이 입증됐다는 뜻이 아니다.

`Rogue10mStylizedCharacter.h`의 실제 재생 대상은 숨긴 AnimationSourceMesh다. 장착 메시를 붙이는 보이는 몸과 구분해야 한다. 따라서 실제 소켓·리타게터·몽타주 연결을 에디터에서 감사한 후 준비/방출/회복 프레임을 측정하고 파생 구간·블렌드를 편집한다. 원본 보존, 카메라·root·기존 피해 계약, [애니메이션 증거 규칙](../../Docs/AnimationRecordingGuide.md)을 유지한다.

## 검증 수준·통합 대기

현재 수행한 것은 파일명/해시 조사, 기존 근거·소스/API 대조, Python 도구2개의 AST 구문 검사, Unreal을 import하지 않는 리타겟 도구 실행 인자 검사10건, `git apply --check --whitespace=error-all` 및 독립 검토다. [준비 검사 기록](evidence/staff-equipment-20261110/preparation-checks.json)과 [패치 검사](evidence/staff-equipment-20261110/patch-preflight.txt)에 남겼다. **UHT/C++ 컴파일·UE 자동화·실제 리타게팅·모션 편집·렌더·물리 입력은 실행하지 않았다.** AST와 patch 검사를 동작 통과로 보고하지 않는다.

공유 파일 `Source/Rogue10m/Components/Rogue10mWeaponVisualComponent.cpp/.h`와 에디터는 대검 담당 소유다. 실제 소스 반영·에디터 실행은 담당 완료 및 부모 통합 조율 대기다. 원본 snapshot과 현재 파일이 다르면 최신 대검 변경 위에 재기반하고, 전체 파일을 덮어쓰지 않는다. 자동 승인 거절은 없었다.

실제 장착의 추가 차단 요인은 **적합성·사용 조건·그립이 확인된 지팡이 모델을 아직 선정하지 못한 것**이다. 메시 감사 후 모델을 고르고 원본 보존 파생·hand_r 조정·StaffStrike/Shot ready/contact/recovery 캡처, 검→Staff→너클→맨손·외형 교체, 검 전용 AnimBP 복원, 실제 인벤토리 장착을 검증해야 한다. 장비 미리보기는 skeletal component 복제 방식이므로 새 static 지팡이 표시까지 자동 인증되지 않는다.

코디네이터 인계물은 [패치](evidence/staff-equipment-20261110/integration.patch), [summary](evidence/staff-equipment-20261110/summary.json), [계획](../architect/2026-11-10_staff-equipment.md)이다. 패치 적용 후 한 번의 직렬 빌드와 `Rogue10m.Weapons.StaffAttachment` 자동화가 필요하다. 에디터 도구는 공유 세션 소유자의 명시적 실행으로만 사용한다.

감사 도구의 인자는 `-RogueStaffAuditLease=CoordinatorGranted -RogueStaffAuditLabel=<fresh label>`, 리타겟 도구는 `-RogueStaffStageLease=CoordinatorGranted -RogueStaffStageLabel=<fresh label>`이다. 이 표식이 실제 lease를 취득하지는 않는다. 담당자 조율을 마친 단일 에디터 프로세스에서 실행하며 이전 후보·증거 label은 재사용하지 않는다.

## 공용 일지·Sprint 반영 문안

> [마법 지팡이 장착 조사 및 코드 준비 / UTC 2026-11-10]
>
> Staff 타입은 있으나 현재 시각 메시 선택이 대검·너클뿐이라 지팡이가 표시되지 않음을 확인했다. 로컬6,451개 자산의 파일명 검사에서 명시적 지팡이 메시를 찾지 못했고, 마을 팩 Rod4·Beam1은 미선정 후보로 해시만 기록했다. 적합성·현재 조건이 확인되지 않은 소품을 임의 활성화하지 않았다.
>
> 별도 작업공간에 Staff 메시/소켓/grip 장착 분기, 누락·유효성 guard, 구체 HumanMale 합성 장착 시험, 에셋/모션 감사·보유 Fab 리타겟 준비 도구를 작성했다. 애니메이션은 기존 CombatMagic 원본을 우선하며 새 구매·동의·다운로드는 없다. 공유 소스·에디터는 대검 담당 완료 및 통합 조율 대기이며 UE 빌드·실제 장착·리타겟·렌더·커밋·푸시는 수행하지 않았다.
>
> [Notion 요약 후보] 현재 게임에서 지팡이가 실제 표시된다는 증거는 없다. 장착 코드와 검증 준비를 완료했으며 검증된 메시 선정·직렬 통합·Fab 모션 파생·실제 그립 캡처가 다음 단계다.

Sprint 문안: 목표=마법 지팡이 실사용 여부 확인과 장착 기반 준비, 변경=독립 코드/시험/감사·리타겟 도구, 검증=읽기 전용 근거·파일 해시·구문·패치/독립검토, 상태=준비 완료·공유 적용/UE 검증 전, 차단=대검 파일/에디터 인계 및 지팡이 모델 미선정. 공용 DevLog/Sprint·Git은 부모 코디네이터가 갱신한다.

## 2026-11-10 재개 검증
- 특정 지팡이 모델을 지정한 사용자 요청은 확인하지 못했다. 현재 로그인된 Fab에서 Wizard for Battle Polyart(Dungeon Mason)와 Combat Magic Animations의 보유·Standard·UE5.8 지원을 새로 확인했다. 구매·새 다운로드·약관 수락은 하지 않았다.
- Wizard 로컬 Vault에서 Staff01/02/03 정적메시와 공통 재질·텍스처, 정확히 5개 원본 바이트만 설치했다. Editor AssetRegistry 재귀 의존성도 이 다섯 패키지 안에서 닫혔고 SHA는 보존됐다. Staff01은 226 triangles, 227.79cm이며 검수 프리뷰에서는 180cm로 축소했다.
- 공식 MCP AssetTools로 /Game/Weapons/Staff20261110/SM_BattleWizardStaff01을 새 복제·메타데이터·저장했다. 이 파생본을 3인칭 렌더 바디 hand_r에 임시 장착해 검수했다. 아직 production StaffMesh 기본값은 미지정이다.
- Combat Magic StaffStrike(2.0s)/StaffShot(2.9s)을 StaffResumeC의 Manny/Human 파생4개로 저장했다. 실제 트랙89/166, 최대 손 이동36.30~110.70cm, root 수평 이동0, 프레임레이트·프레임 수·root 플래그 보존, 원본/공유17개 SHA 동일이다. 실패 A/B는 보존했고 Root 체인 가정과 Human Root/Pelvis 문자열 대소문자 검사를 수정했다.
- StaffRuntimePreviewA에서 기존 런타임 retarget ABP와 실제 소스 본 오차0.0046cm 미만, 8개 PNG 완료·직접 검토를 확인했다. 무기는 보이지만 기본 백색 재질·후측면·고정 표본이므로 손가락 표면 접촉이나 실제 전투 판정 승인은 아니다. 현재 실제 플레이어 외형을 사용하는 별도 게임 장착·모션 QA를 준비 중이다.
- Staff 스킬 DA/몽타주/피해·입력·주문 확정/취소 의미와 첫인칭 기능은 바꾸지 않았다. 공개 원본/재사용 가능한 파생 에셋 배포 권한은 별도 미확인이므로 commit/push하지 않았다.
- 근거: evidence/staff-equipment-20261110/{StaffProvenanceA,BattleWizardStaffInstallA,StaffMeshDerivativeA}, evidence/staff-animation-stage-20261110/StaffResumeC, DevLog/20261110_Staff/StaffRuntimePreviewA.
