# 무기별 6슬롯과 너클 아이덴티티 개발 결과

작성: 2026-11-10. 상태: 기반 구현·회귀검증 통과, 신규 개별기술 제작과 오라 시각 개선은 진행 중.

## 구현
- 기존 AttackInputSlot enum 숫자0~5를 보존하고 QSkill/ESkill/RSkill/MouseChord를 추가했다.
- DataAsset AllowedSlotPolicy는 Common/MouseChordOnly/RightChargeOnly/BasicOnly 및 기존에셋 호환 Legacy를 지원한다.
- 공용4개와 전용2개만 사용자가 장착하며 Primary는 프로필 고정 기본공격이다. 기본공격·패시브·Ctrl은 장착6개에 포함하지 않는다.
- 현재무기·해금·활성기술·허용슬롯을 함께 검증한다. 같은 기술의 중복장착과 자유키 단축으로 제한을 우회하는 경로를 막았다.
- 무기별 durable 프리셋과 명시적 빈슬롯을 보존한다. preview 기술은 저장하지 않고, 실제로 해금된 뒤에는 전환/저장 공통규칙으로 선택을 보존한다.
- 기존 v5 저장의 compatible 선택을 이관한다. 우클릭차징 선택을 우선하며 비어 있으면 구 좌차징 선택을 우차징으로 옮긴다. 구 enum 값은 바꾸지 않았다.
- BottomHUD는 Q/E/R/RMB/좌+우/RMB차징 6개를 표시한다. 스킬 키 설정은 공용 Q/E/R 장착으로 제한하며 슬롯 제한을 안내한다.
- 새 인벤토리 초기화에서 무기 자동장착만 건너뛰어 뉴비 맨손 시작을 보장한다. 기존 저장 장비와 방어구 초기장착은 유지한다. 기초무기 전체 지급시점은 미정이다.
- 너클 Ctrl은 별도 일시 컴포넌트에서 공속/이속/피해/스킬범위 보정 및 Post Process/기존 프로젝트 Niagara 재사용 오라를 실험한다. 기본 GAS 속성의 저장값을 덮어쓰지 않는다. 공격 승인시 효과를 snapshot하므로 이후 상태변경이 이미 승인된 공격을 재증폭하지 않는다.
- 종료·무기교체·사망·런종료에는 보정과 전용효과를 정리하고 재발동 중첩을 허용하지 않는다. 버프/오라/차징 상태는 durable 저장에 넣지 않는다.

## 검증 현황
- C 최초 UHT 상속함수명 충돌은 IsIdentityActive/DeactivateIdentity로 교정했다. C3 통합빌드 이후 D·E Editor 빌드까지 성공했다.
- C3_1110A에서 assertion은 성공했지만 fixture의 Unarmed 프로필 누락 warning1을 엄격한 실패로 기록했다. 경고를 숨기지 않고 실제 맨손 프로필을 fixture에 추가했다.
- D_1110A: SlotPolicy·PresetsAndNovice 2개와 기존 HybridSkillTree 3개가 모두 경고0/오류0 통과했다. 슬롯 제한·최대6·중복 이동·잘못된 저장 원자 거부·구 차징 이관·빈슬롯·실해금 preview 보존을 확인했다. 기존385XP·내공·장착선택은 뉴비에서 활성화하지 않으며 너클 재장착 시 복원했다. 뉴비 고정주먹의 실제 GAS 승인도 확인했다.
- 별도 D2 HumanoidUnit_D2A는 ContactTiming·ProfileTransitions 2개가 경고0/오류0 통과했다. 앞의5개와 합계7개 clean 실행이지만 서로 다른 DLL/보고서임을 구분한다. 접촉허용오차 및 몽타주 fixture 수정은 보스 담당 범위다.
- TP_D1: 실제 Controller InputKey 13/13 시나리오, 786 checks, failures0. 실제 production LeftJab와 원래 GAS에서 LMB 사용증가1, Q/E/R/RMB/Ctrl의 뉴비 무기능력 미실행을 확인했다. 탭/차징/동시클릭·해제순서·늦은 반대클릭·메뉴취소·슬롯제거 검증을 포함한다. 실제 물리 입력을 했다는 뜻은 아니다.
- Identity_E1(E2 DLL): 숨김 렌더링의 실제3P에서 Controller InputKey Ctrl로 발동. 54 checks, failures0, 8 PNG, 기존396개 세이브 해시 보존, launcher/engine exit0. MaxWalkSpeed 620→744→620, 유효공속1.04→1.30→1.04, 기본 GAS 이동속도620/공속1.04/공격력13은 불변이었다. 피해1.08/범위1.10 배율 getter와 중첩/쿨다운 거부, 만료·무기교체·Die 및 직접 런종료 핸들러 정리를 검증했다. 실제 적 HP의 버프 전후 비교나 승인된 공격 도중 만료 snapshot 검사는 이 런에서 수행하지 않았다.
- Identity_E1의 활성/후반활성 캡처에서 청색 오라 유지와 상체 시인성, 만료/교체/Die 호출 후 제거를 직접 확인했다. 다만 차용한 Charge Niagara가 바닥의 큰 검푸른 소용돌이로 보이며 발·하체를 가린다. 최종 오라 시각품질은 미완료다. PP 컴포넌트의 생성/제거는 수치 확인했으며, PP 단독 체감 A/B는 오라 굴절과 분리해 추가 검수한다. 교체 직후 캡처에는 약한 청색 잔상이 남아 별도 렌더 잔상 검수가 필요하다.
- Identity 런종료 케이스는 production 핸들러 직접 호출이며 실제 런 정산을 끝낸 검사가 아니다. 사망은 production Die 호출 뒤 버프 정리 확인으로, 체력0·죽음 HUD 전체흐름 완료를 주장하지 않는다. 독립 생명주기 케이스 사이 쿨다운만 QA에서 초기화했다. 물리키·풀런·저장 재시작은 미실시다.
- 정적리뷰에서 구 QuickSlot 차징우회, 구 ChargedPrimary 이관누락, abstract fixture와 State 누락, preview 실해금 후 선택유실을 찾아 교정했다.
- 재실행: Scripts/VerifySkillLoadouts.ps1(슬롯2+성장3+Humanoid2), Scripts/VerifyWeaponIdentity.ps1. 각 QA 세이브와 증거를 별도 보존한다.
- 증거: Feature/doc/evidence/weapon-skill-loadouts-20261110/D_1110A/{SkillLoadout,HybridSkillTree}/report/index.json, Feature/doc/evidence/weapon-identity-20261110/Identity_E1/{launcher.json,engine.log,saves-before.json,saves-after.json}, DevLog/20261110_WeaponIdentity/Identity_E1/{results.txt,samples.csv,01_baseline.png~08_death_cleared.png}.

## 다음 카탈로그 골격(F 검증 대기)
- 무기 프로필에 optional ActiveTechniqueCatalog(TechniqueId+Skill)를 추가했다. 기존 전체 성장트리·기본공격·패시브·기본바인딩·해금·저장을 대체하지 않는다. 목록을 채우지 않은 기존 에셋은 기존 동작을 유지한다.
- 명시목록은 정확10개, 고유ID/스킬, 고정Primary/패시브 제외, 전체트리 소속, 명시 Common/MouseChordOnly/RightChargeOnly 정책을 요구한다. 목록이 잘못되면 조회 전체가 빈목록/null로 실패하고 일부만 공개하지 않는다.
- 마법사의 명령/즉발 규칙이 미확정이므로 Staff와 뉴비 Unarmed에는 이 근접 카탈로그를 허용하지 않는다. 공용6/동시2/차징2 분배는 제안이므로 검증에 강제하지 않는다.
- 신규 Rogue10m.SkillLoadout.ActiveTechniqueCatalog는 순수 데이터 검증과 fulltree/default/unlock 비변경을 검사한다. 아직 F 컴파일/실행 전이다. 이후 strict wrapper는 슬롯3+성장3+Humanoid2 총8개를 기대한다.
- TechniqueId를 실제 출시한 뒤에는 이름을 바꾸지 않는 계약이다. 현재 기존 저장은 soft-path 그대로이며 새ID 저장/별칭 이관·실제20개 카탈로그 에셋·Combat/UI 소비까지 구현했다고 주장하지 않는다.

## 남은 범위와 제한
- 10개 기술의 고유 동작·개별강화·새 콘텐츠카탈로그 전부를 제작한 상태는 아니다. 기존 스킬의 몽타주/히트지연/밸런스 값은 유지했다.
- 현재 아이덴티티 수치8초/30초쿨다운/공속25%/이속20%/피해8%/스킬범위10%는 임시 가정이다. 사용자 확정 밸런스가 아니다.
- 기존 혼합성장 자동효과는 보존되어 너클 Ctrl과 같이 적용될 수 있다. 새 Ctrl에 계열효과를 완전히 통합하는 작업은 후속 설계다.
- 기존 Unarmed→Brawler 성장매핑은 Unarmed→None으로 분리했다. 저장된 성장은 지우지 않으며 고정주먹과 재장착 복원은 위 D·TP 검사에서 통과했다. 실제 세이브 재시작 왕복은 별도다.
- 신규20개 모션·판타지 VFX 및 고유 판정은 단계별 제작 중이다. 기존 안전 공격을 새20개 완료로 집계하지 않는다. 실제3P 전투 중 밝음/어둠·다수 적·교체/사망·손/발·타격 프레임 검수가 필요하다.
- 직업명 로비표시 전체 통일, 기초무기 전체지급, 미래 직업 실구현은 완료로 주장하지 않는다.

## 관련문서
- 설계: Feature/architect/2026-11-10_weapon-skill-loadouts.md
- 전체 직업기획: Docs/WeaponClassDesign.md- 상세 동작/VFX: Docs/SkillMotionVFXDesign.md
- 실제 원본 매핑: Docs/FantasyMotionVFXSources.md
- 개별 제작단계/블로커: Docs/SkillProductionTracker.md
