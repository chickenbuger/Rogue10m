# 프로젝트 자원 읽기 전용 독립 조사

프로젝트 폴더·소스/제작 스크립트·문서를 확인했다. 아래는 로컬 존재와 사용 기록이며 이 조사만으로 현재 Fab 보유를 입증하지 않는다. `.uasset`/`.umap` 바이너리, 런처 캐시, 계정 파일은 읽거나 수정하지 않았다.

| 로컬 팩 | 확인한 흔적 | 한계 |
| --- | --- | --- |
| Stylized_Village | 시작 마을 주택/광장/길과 OpenRunMap 나무/지형/물/교량, Terrain LayerInfo 참조 | 현재 전체 의존성 그래프·화면 재검증은 미실행 |
| StylizedCharacter | Human/Dwarf/Orc 남녀 6종과 종족별 IK Retarget 구조 참조 | 각 제품의 Fab listing ID/버전 대조 미실행 |
| Polyart/ModularStylizedChars | 메시/의상/애니/Blueprint 폴더, SharedResources Manny IK Rig 스크립트 참조 | 전체 모듈형 외형 적용은 미확인 |
| MonsterForSurvivalGame | 6종 조우와 25종 공용 로스터/Idle·이동·공격 연결 문서 | 이번 작업에서 게임을 실행하지 않음 |
| CombatMagicAnims | 마법/지팡이·직업 전투 제작 스크립트 참조 | 개별 클립의 현재 runtime 연결은 해당 작업에서 확인 |
| CloseCombatAnimSet | 필요한 33패키지와 10/2 Manny 프리뷰 11종 준비 문서 | 그 문서 시점에는 게임 DA/몽타주 미연결. 현재 상태를 과거 기록만으로 확정하지 않음 |
| AdvancedPortalsSystemVFX | Sparks/Vortex를 공통/직업 전투 VFX에 재사용한 기록 | 이번 UI 조회에서 현재 보유 카드는 미확인 |
| UI_SFX_Free_Pack | Cues/Wavs 폴더 존재 | 소스/제작 스크립트/결과 문서에서 실제 재생 근거 미확인 |

아린은 사용자 제공 FBX를 개선한 `Art/NPC/Arin` RefinedV3와 Idle/Walk/Talk 통합 기록을 갖는다. SourceArt에도 기존 시작 장비/물약 아이콘, 로비 배경, 맨손/대검 아이덴티티 이미지가 있다. 이들을 Fab 출처나 미제작 콘텐츠로 분류하지 않는다.

근거: `Feature/doc/2026-10-05_start-village.md`, `Docs/WorldStructure.md`, `Docs/OpenWorldMonsterDesign.md`, `Feature/doc/2026-08-19_class-combat-animation-vfx.md`, `Feature/doc/2026-10-02_close-combat-animation-preparation.md`, `Art/NPC/Arin/Animation/README.md`, `Art/NPC/Arin/ReferenceRefined/README.md`.
