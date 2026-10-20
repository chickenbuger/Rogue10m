# 격투가·대검술사 혼합 성장 스킬트리 구현

## 범위와 기준

사용자가 확정한 [혼합 성장 계획](../../Docs/HybridSkillTreeDesign.md)과 [격투가 v3 화면](../doc/evidence/hybrid-skill-tree-20261018/imagegen-mockup-v3.png)을 실제 C++ 성장·전투·저장·K 메뉴에 연결한다. 격투가는 내공·투지만 사용한다. 대검술사는 원본 참조의 투지·마나 방향을 적용하며 기존 5개 기본 공격을 보존하고 기존 공격의 프로젝트 파생 기술을 핵심기로 추가한다. 다른 직업의 전투와 프로필을 확장하지 않는다.

작업 브랜치: `Sprint#5-38-hybrid-skill-tree-runtime`. 시작 시 `Sprint#5-37-main-checkpoint`의 미커밋 변경을 그대로 이어받았으며, 병행 체크포인트가 로컬 main에 병합된 뒤 기준을 `05f74f1`로 맞췄다. 기존 변경 목록/해시는 `evidence/hybrid-skill-tree-runtime-20261020/baseline-tracked.json`에 보존한다. 갱신된 사용자 지침에 따라 검증·기록 후 이번 범위를 커밋하고 `develop` → `test` → `main`으로 통합한다. 원격 게시가 승인 검토에서 차단되면 차단 상태를 기록한다.

## 자원 재사용

`Docs/FabAssetReuseGuide.md`를 확인했다. 이번 작업은 새 Fab 반입 없이 프로젝트에 이미 연결된 Brawler/GreatSword AttackSkill·SkillProfile·몽타주·아이콘을 재사용한다. 대검 신규 기술은 기존 프로젝트 공격의 파생 Data Asset으로 제작하고 원본 애니메이션·vendor 파일을 변경하지 않는다. 실제 생성 전 원본 경로·몽타주 연결을 명령줄 Editor에서 조회한다. 기존 폴더 존재를 Fab 소유권·새 라이선스 검증으로 주장하지 않는다. 원형 UI는 기존 이미지 시안을 참고해 native UMG/Slate로 구현하며 화면 전체 PNG를 UI로 대체하지 않는다.

## 작업 패킷

|패킷|목표 / 수정 위치|완료 조건|검증|되돌릴 경계|
|---|---|---|---|---|
|R1 성장 원장|Data 정의, PlayerState 소유 SkillTreeProgressionComponent, Town, SaveGame/Subsystem|두 숙련, 자동 스탯/기술, 실제 투자비용, OR 심화, 활성1계열, v1/v2 이전, v3 교차검증|C++ Automation: XP 경계·원장·선행·환급·이전·거부|새 컴포넌트/데이터와 이번 통합 diff|
|R2 전투 연결|CombatComponent, Character|유효 명중 실행당1XP, 무기별 스탯 적용, 기존 기본기/바인딩 보존, 아이덴티티 발동·자원/공명|실제 전투 runtime/Automation, 빌드|이번 전투/스탯 통합 diff|
|R3 노드/대검 콘텐츠|Catalog C++, Editor 생성·검증 스크립트, 프로젝트 DataAsset|격투 전체 계획 노드와 대검 성장·아이덴티티·핵심기 실제 참조|headless Editor 생성/검증, 누락/순환/allowlist 검사|신규 HybridSkillTree 자산 및 명시한 profile 확장|
|R4 원형 UI|기존 K 창 native 부모 + 새 Graph/Entry|2직업 탭, 선택·조건·실제 XP·공용 P·구매·활성·재분배·키장착, 잠김 상태|실제 게임 렌더 캡처·해상도·입력 확인|이번 SkillTree UI 변경|
|R5 통합·검토·문서|Tests, Feature/doc, Docs, DevLog|Editor 빌드 및 관련 테스트 통과, 독립 리뷰 해결, 증거와 한계 기록|BuildEditor.ps1, CheckGeneratedChanges.ps1, 자동화/Editor 검증|테스트·문서 추가분|

## 구현 계약

- 숙련 XP는 무기 계열별로 분리하고 맨손/너클은 Brawler를 공유한다. 다단·다중 대상과 처치타격을 포함해 실제 살아 있는 몬스터에 양수 피해를 준 실행에 한 번 지급한다.
- 격투 노드는 계획의 기존 9개 잠금 무공·스탯 합계를 따른다. 이전 전수 기술은 legacy 허용으로 보존하며 숙련 스탯을 소급 생성하지 않는다.
- 공용 포인트는 `LV-1-특성 실제 비용-트리 실제 비용`이다. 음수/중복/불가능한 저장을 거부하고 저장 실패 시 추가 거래를 차단한다.
- 아이덴티티는 새 tree enum과 allowlist를 사용한다. 기존 돌주먹 실행을 입문 뒤로 잠그지 않는다. 기본 전투 실행은 기존 Combat/GAS를 유지한다.
- 마을 서비스 조건에서 투자·활성 전환·재분배한다. 자원 절대값과 재사용 시간 보존, 새 게이지0, 비활성 계열 효과 미적용.
- 무료 숙련 보상은 이벤트로 자동전수하며 별도 Tick을 만들지 않는다.
- 테스트는 QA 전용 저장 prefix 또는 저장 없는 격리 world를 사용한다. 생산 프로필 저장을 테스트 대상으로 삼지 않는다.

## 검증 진행

R1~R5 구현·독립 검토·실행 검증 완료. 최종 Editor 빌드8 성공, Automation3개 Success, 실제 게임50개 확인과 독립 프로세스 저장 복원14개 확인에서 실패0을 기록했다. 원형 UI의 버튼 고정 영역과 라벨을 실제1080p/720p 캡처로 보완했다. 자세한 증거와 검증 한계는 [결과 문서](../doc/2026-10-20_hybrid-skill-tree-runtime.md)에 기록한다.

병행 보스 정산 작업이 로컬 main에 반영돼 통합 기준을 `f49915b4`로 갱신했다. 빈 스테이징과 기존 기준의 조상 관계를 확인한 뒤 index/branch 기준만 fast-forward하고 작업 파일을 보존했다. 최종 빌드·실행 QA는 이 공유 소스 상태에서 수행했다. 선행 체크포인트 `Sprint#5-37-main-checkpoint=05f74f1`은 유지한다.
