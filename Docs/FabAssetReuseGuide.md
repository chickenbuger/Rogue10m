# Fab 보유 에셋 우선 활용 가이드

## 개발 원칙

사용자 요청에 따라 신규 콘텐츠 작업은 기존 프로젝트 자원과 Fab 보유 에셋부터 검토한다. 적합한 메시·재질·클립·Niagara·효과음을 선별해 사용하고, 현재 판타지/스타일라이즈드 화면과 C++·Data Asset 기반 게임 구조에 맞춰 적용한다. 특정 자산이나 새 제작을 명시한 사용자 요청은 우선한다.

이 문서는 2026-10-19의 부분 조사다. 실제 Epic Games Launcher > Fab > 내 라이브러리의 최신순 상단과 `Combat`, `Monster` 검색에서 서로 다른 카드 30개를 확인했다. 전체 보유 목록을 끝까지 조사하거나 라이선스·버전·실제 클립을 검증한 결과가 아니다. 카드 제목의 말줄임은 그대로 보존했다.

## 기본 애니메이션·VFX는 보유 원본 편집부터

기본 동작과 효과는 적합한 보유 에셋을 기반으로 수정·편집하는 방식을 기본으로 한다. 원본 팩을 보존하고 프로젝트용 파생본에서 필요한 부분을 조정한다. 합리적인 편집으로 요구를 충족하기 어렵거나 사용자가 새 제작을 명시한 경우에는 새 원본을 제작하고 선택 이유를 기록한다.

| 작업 | 먼저 적용할 편집 | 적용 시 확인 |
| --- | --- | --- |
| 기본 이동·공격·가드·피격 | 적합한 기존 클립 선택, 리타기팅, 필요한 구간 편집, 재생률·블렌딩·포즈·몽타주·Notify 조정 | 발 접지와 이동/root motion, 머리 카메라, 실제 접촉·판정 시점, 상태 전환 |
| 공격 궤적·속성·명중·포탈 VFX | 기존 효과의 파라미터·재질·색·크기·수명·방출량·재생 시점·부착 위치 조정 | 1인칭 시야와 효과 가독성, 실제 공격 타이밍, Niagara 상태·중첩·풀링과 비용 |

예를 들어 좌훅/어퍼컷은 Close Combat 후보의 적합한 구간을 편집해 현재 몽타주·타격 시점에 맞추고, 속성 궤적은 보유 Slash Trail 후보의 색·크기·수명·부착을 조정하는 방식부터 평가한다. 후보의 실제 클립·포맷과 현재 게임 연결은 작업 시 확인한다. 이번 원칙 보완에서 게임 에셋을 편집한 것은 아니다.

## 다음 기능에서 먼저 볼 후보

| 영역 | 실제 보유 목록에서 확인한 후보 | 프로젝트와의 연결 / 다음 적용 판단 |
| --- | --- | --- |
| 격투 동작 | Close Combat Animset — Ibrahim Akinde | `/Game/CloseCombatAnimSet`과 Manny 프리뷰가 이미 있다. 10/2 준비 문서에 좌훅·우어퍼컷 후보가 있으나 현재 게임 연결은 다음 작업에서 재조회한다. 현재 머리/카메라 안정화와 실제 타격 타이밍에 맞는 구간부터 평가한다. |
| 마법·직업 동작 | Combat Magic Animations — SoerGame | `/Game/CombatMagicAnims`와 직업 전투 제작 참조가 있다. 지팡이/마법 동작을 늘릴 때 기존 팩의 적합한 클립부터 확인한다. |
| 기존 몬스터 확장 | Monster for Survival Game PBR Polyart — Dungeon Mason | `/Game/MonsterForSurvivalGame`과 종별 로스터/동작 매핑 문서가 있다. 기존 공용 C++ 몬스터와 데이터 프로필에 맞는 종·클립부터 추가한다. |
| 몬스터 음성 | Monster Roars & Growls - Audio Pack — VoiceBosch | 보유 카드 확인. 현재 프로젝트 반입/재생은 미확인이다. 적 인지·피격·사망 음성이 필요할 때 종별 음색과 중첩 재생을 평가한다. |
| 마을·필드 | Stylized Village (Village, Stylized Villag… — Hivemind | `/Game/Stylized_Village`의 마을/필드 사용 기록이 있다. 지형·길·물·나무·건축의 기존 자원을 먼저 재사용한다. 카드와 로컬 패키지의 동일 버전 여부는 확인하지 않았다. |
| 모듈형 캐릭터 | Stylized Characters - Modular Stylized … — Polyart Studio | 보유 카드와 디스크 표시를 확인했다. `/Game/StylizedCharacter`, `/Game/Polyart/ModularStylizedChars`가 있으나 카드와 각 폴더의 정확한 제품 대응은 반입 시 확인한다. 기존 외형·리타깃·커스터마이징 구조를 유지한다. |
| 무기 속성 효과 | Slash Trail FX Elemental — SoftTofuVFX | 보유 카드 확인. 현재 반입은 미확인이다. 대검 등 무기 궤적의 필요한 효과부터 비교하고 1인칭 조준 시야와 과도한 화면 가림을 확인한다. |
| 유적·보스 공간 후보 | White castle — DmitriyDryzhak; Free Dark Fantasy Stone Statue, Pedes… — TEMPORAL ANOMALY | 보유 카드 확인. 적용은 미확인이다. 북부 유적 작업 때 필요한 구조물/소품과 기존 화면의 재질·규모 일치를 먼저 평가한다. |
| 숲 적·보스 외형 후보 | Pack of tree ents — SAN 3D Art; Wood Monster — Polyphoria | 보유 카드 확인. 메시/스켈레톤/클립·UE 지원·화풍을 먼저 확인하고 기존 조우 데이터에 적합한 경우 사용한다. |

실제 제품 속성 확인 없이 이름이나 썸네일만으로 기존 에셋을 교체하지 않는다. 위 행은 우선 조사할 후보이며 실제 import·적용 완료 목록이 아니다.

## 이미 프로젝트에 있는 추가 자원

| 로컬 팩/자원 | 확인한 상태와 활용 |
| --- | --- |
| AdvancedPortalsSystemVFX | 포탈/직업 효과 제작에서 Sparks·Vortex 재사용 기록이 있다. 이번 UI 조사에서는 해당 카드의 현재 계정 보유를 확인하지 않았다. 포탈/VFX 작업 시 기존 데이터와 실제 보유 항목을 함께 재확인한다. |
| UI_SFX_Free_Pack | Cues/Wavs 폴더가 있다. 코드·제작 문서에서 실제 재생 근거는 확인하지 못했다. UI 입력/확인/취소 피드백이 필요할 때 우선 청취·적용 후보로 삼는다. 이번 UI 조사에서 보유 카드는 미확인이다. |
| Art/NPC/Arin | 사용자 제공 FBX를 개선한 아린 RefinedV3와 Idle/Walk/Talk 통합 기록이 있다. 이미 적용한 NPC를 보존하며 새 인물의 요구에 맞을 때 모듈형 캐릭터 팩을 검토한다. |
| SourceArt의 기존 아이콘·로비·아이덴티티 이미지 | 보유 제작 자원이다. Fab 출처로 단정하지 않으며 기존 UI와 맞는 범위에서 재사용한다. |

## 프레임워크·템플릿 팩의 활용

내 라이브러리에서 Hyper Attribute Manager v4, Ultimate Difficulty Scaling (UDS), Advanced Grid Inventory System, Flexible Combat System - Basic, Dynamic Combat System - Magic, Basic Multiplayer Melee Combat System 등을 확인했다. 해당 시스템이 필요한 기능과 맞는지 살펴보고 데이터·표현·워크플로를 평가한다. 기존 전투·성장·인벤토리·저장을 바꾸는 통합은 별도 설계·검증 범위로 잡는다. 현재 사용 중인 기능은 이미 구현되어 있으므로 보유 팩이 있다는 이유만으로 교체하지 않는다.

Sci-Fi 연구실·정거장, 스포츠 장비, 큐브 월드 등도 보유 목록에 있었으나 현재 화면과 요구에 대한 적합성을 확인하지 않아 우선 적용 대상으로 정하지 않았다. 필요성이 생기면 다시 평가한다.

## 기능별 실행 순서

1. 해당 기능의 요구와 현재 연결된 에셋을 먼저 확인한다. 이 문서와 프로젝트 폴더·참조를 찾아 중복 제작을 줄인다.
2. Fab 내 라이브러리에서 해당 제품을 다시 찾고 보유/다운로드 여부·사용 조건·포맷·UE 버전을 확인한다. 전체 목록 재조사는 매번 반복하지 않는다.
3. feature 계획에 선택한 제품/제작자, source 경로, 필요한 항목, 적용 위치, 선택 이유를 기록한다. 현재 통합 상태를 명확히 구분한다.
4. 필요한 데이터 자산만 반입하고 vendor 원본을 보존한다. 새 사용부는 기존 프로젝트 경로/Blueprint 자식/Data Asset 구조에 맞게 연결한다. 코드 플러그인 활성이나 framework 적용은 해당 실행·승인 규칙을 따른다.
5. 변경에 맞춰 확인한다. 애니메이션은 스켈레톤·root motion·머리 카메라·접촉 타이밍, VFX는 Niagara 상태·풀링·시야, 소리는 감쇠/동시 재생, 환경은 충돌·LOD/Nanite·맵 이동 등을 실제 사용 범위에서 검증한다.
6. Feature 결과와 DevLog에 실제 사용 자산과 남은 한계를 기록한다. 원본 팩의 존재를 게임 적용이나 성능 검증으로 보고하지 않는다.

## 근거와 조사 범위

- [런처 보유 카드 관찰 기록](../Feature/doc/evidence/fab-owned-assets-20261019/launcher-observations.json)
- [프로젝트 자원 독립 조사](../Feature/doc/evidence/fab-owned-assets-20261019/project-assets.md)
- [시작 마을](../Feature/doc/2026-10-05_start-village.md)
- [월드 구성](WorldStructure.md)
- [몬스터 매핑](OpenWorldMonsterDesign.md)
- [직업 전투 애니메이션/VFX](../Feature/doc/2026-08-19_class-combat-animation-vfx.md)
- [Close Combat 준비 이력](../Feature/doc/2026-10-02_close-combat-animation-preparation.md)
- [아린 사용자 제공 모델 개선 기록](../Art/NPC/Arin/ReferenceRefined/README.md)

이번 작업은 UI 열람·로컬 문서/파일명 조사·개발 지침 저장이다. 구매·다운로드·팩 설치·에셋 import·라이선스 판정·UE 빌드·PIE는 실행하지 않았다. Portal 추가 검색 시 사용자 창 입력이 감지되어 상태를 다시 읽은 뒤 추가 UI 조작을 멈췄으며, 미완료 검색을 보유 확인으로 기록하지 않았다.
