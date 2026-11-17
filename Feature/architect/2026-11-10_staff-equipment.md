# Staff 장착 및 보유 모션 재사용 패킷

2026-11-10 UTC. 목표는 마법 직업이 실제 지팡이 메시를 사용하는지 확인하고, 현재 대검 작업과 분리해 필요한 장착 코드·검증을 준비하는 것이다. 사용자 후속 지침에 따라 새 동작 제작보다 기존 Fab/Mixamo 모션을 우선한다.

## 확인한 현재 상태

- `ERogue10mWeaponType::Staff`는 있으나 `Wand`라는 별도 타입은 없다. enum·저장·프로필을 확장하지 않는다.
- `WeaponVisualComponent`의 메시 선택/표시는 GreatSword·Knuckle만 처리한다. Staff는 현재 메시를 선택하지 못하므로 지팡이가 실제로 보인다고 보고할 수 없다.
- Content 6,451개 `.uasset`의 파일명 조사에서 Staff/Wand/Scepter 계열 이름의 SM/SK/SKM 파일은 0개였다. 이는 파일명 기준이며 모든 메시의 형상을 열람한 전수검사가 아니다.
- `SM_Rod_Short/Medium/Long/XL`, `SM_Beam_Long_1`은 로컬 목재 건축 소품이다. Rod의 클래스·치수·축·그립은 이번에 로드하지 않았다. Beam은 과거 감사에서 StaticMesh·약302×15.7×16cm였지만 무기 적합성·현재 사용 조건이 확인되지 않아 선정하지 않았다.
- 원본 CombatMagic 팩의 Staff 모션과 프로젝트 Staff DA·Montage·Niagara는 로컬에 있다. 모션 팩의 존재는 지팡이 메시 확보나 현재 화면 적용 증거가 아니다.

근거는 [Fab 재사용 가이드](../../Docs/FabAssetReuseGuide.md), [실제 원본 조사](../../Docs/FantasyMotionVFXSources.md), [로컬 목록/해시](../doc/evidence/staff-equipment-20261110/local-asset-inventory.json), [마법진 연계 설계](2026-11-10_magic-circle-niagara-integration-plan.md)다.

## ULW 패킷과 파일 소유

| 패킷 | 목표·수정 범위 | 완료·검증 조건 | 롤백/소유 |
| --- | --- | --- | --- |
| A 읽기 전용 조사 | 소스·기존 문서·파일 목록·공개 조건 | 메시/모션/계정 이력/실사용 구분 | 이번 담당, 완료 |
| B 장착 구현 준비 | 별도 `tmp/staff-equipment-20261110/candidate`의 WeaponVisual .h/.cpp와 신규 Staff 시험 | StaffMesh/StaffGrip/StaffAttachSocket, 없는 자산·소켓·유효하지 않은 transform은 숨김. 원래 검/너클 분기 보존 | 공유 파일의 실제 소유자는 대검 담당. 별도 사본만 변경 |
| C 에디터 조사 준비 | 신규 `AuditStaffEquipment20261110.py` | 원본 해시, 클래스·bounds·LOD·재질·충돌, 손 bone/socket, 현재 DA·segment·rate·HitDelay 기록 | 에디터 slot 전달 후 실행, 에셋 저장/할당 없음 |
| D 보유 모션 파생 준비 | 신규 `StageStaffAnimations20261110.py` | StaffStrike/StaffShot을 메인 Manny·HumanMale로 별도 경로에 리타겟, 원본 보존·포즈 기록. 결과는 미활성 후보 | 에디터 slot 전달 후 실행. vendor·기존 skeleton·DA 수정 없음 |
| E 통합·QA | 대검 담당 완료 후 변경을 재기반하여 공유 소스 반영 | UHT/Editor 빌드, `Rogue10m.Weapons.StaffAttachment`, 실제 지팡이 장착·전환·동작 촬영 | 부모 통합 담당이 단일 빌드/에디터 운영 |

공유 `WeaponVisualComponent.cpp/.h`·에디터를 이번 담당이 선점하지 않는다. 패치의 baseline SHA가 달라졌다면 최신 대검 변경을 보존하며 재기반한다. 강제 적용·동시 빌드·바이너리 직접 편집은 하지 않는다.

## 모션 선정·편집 범위

첫 후보는 기존 보유 [Combat Magic Animations](https://www.fab.com/listings/05d90ca6-d294-4b93-a1b7-232214057850)의 `AS_StaffStrike`와 `AS_StaffShot`이다. 기존 2026-11-10 로그인 UI 조사에는 보유/Standard/UE5.6–5.8/35개 In-place가 기록되어 있다. 이번에는 공개 상품 및 [Fab 일반 조건](https://www.fab.com/eula)만 다시 열람했고 계정·개별 취득계약을 새로 재검증하지 않았다. [Mixamo 공식 FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html)의 무료·게임 사용 안내도 확인했으나, 현재 Fab 원본으로 시작할 수 있어 새 Mixamo 다운로드·계정 동의는 하지 않았다.

원본 skeleton `/Game/CombatMagicAnims/Demo/Mannequins/Meshes/SK_Mannequin`과 메인 Manny skeleton `/Game/Characters/Mannequins/Meshes/SK_Mannequin`은 서로 다른 자산이다. StylizedCharacter는 숨긴 AnimationSourceMesh에 몽타주를 재생하고 `GetMesh()`의 종족별 몸은 Retarget AnimBP로 표현한다. 따라서 Manny 파생은 재생 후보, Human 파생은 렌더 검토 후보로 분리하며 Human 몽타주를 숨긴 Manny에 바로 연결하지 않는다.

처음에는 원본 전체 구간을 보존한다. 실제 준비/손 방출/접촉/회복을 프레임으로 측정한 뒤 프로젝트 파생본의 필요한 구간·블렌드·몽타주를 편집한다. 역사적 Staff audit의 HitDelay0을 현재도 맞는 값으로 가정하거나 새 지팡이 모션에 강제로 복사하지 않는다. 공속·재생률·쿨다운·피해·타수·root·카메라 계약을 바꾸려면 별도 검증한다.

## Exit Gate

현 단계의 완료는 독립 코드·검증 도구 준비다. 실제 장착 완료에는 사용할 메시의 출처/조건·클래스·피벗/그립 검증, 대검 담당으로부터 공유 파일/에디터 인계, 빌드·자동화·실제 3인칭 ready/contact-or-first-damage/recovery 증거가 필요하다. 검에서 Staff로 전환할 때 검 전용 AnimBP가 복원되는지, 반복 refresh에 부품이 늘지 않는지, 실제 인벤토리 장착과 저장 재개도 검사한다. 합성 메시를 쓰는 시험은 무기 아트·모션 QA가 아니다.

새 메시를 구매하거나 새로운 법적 동의를 받는 동작은 이 패킷에서 하지 않는다. 로컬 목재 후보가 부적합하거나 권한을 확인하지 못하면 그 사실을 기록하고 활성화를 보류한다. 공용 DevLog/Sprint·커밋·브랜치는 부모 코디네이터가 통합한다.

## 2026-11-10 재개: 로컬 소유 모션 파생 검수
- 특정 지팡이 모델을 사용자가 선택했다는 근거는 아직 없으며, 현재 StaffMesh 미지정 상태를 유지한다. 모델 다운로드·구매·새 약관 수락은 하지 않는다.
- Combat Magic Animations의 기존 My Library UI-023 및 2026-11-10 로그인 제품 페이지 소유/Standard 기록, 로컬 원본을 근거로 StaffStrike/StaffShot만 새 StaffResumeA 경로에 파생한다.
- 원본과 공유 스켈레톤·DA·몽타주는 해시로 보존하고, 전체 길이·유한 포즈·실제 트랙/손 이동을 확인한다. 지팡이가 없는 모션 검수는 실제 장착 완료로 취급하지 않는다.
- 에디터 PID36396을 독점 사용하며 공식 MCP 연결과 프로젝트 스킬을 읽었다. retarget batch는 해당 전용 Editor Python API로 실행하고, 원본 저장 및 게임 활성화는 금지한다.
