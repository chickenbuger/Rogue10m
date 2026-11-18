# 인간형 보스와 NPC 서있는 모습

## 범위와 보존
- UE 5.8 / Sprint#9-29-humanoid-boss-npc. 기존 사용자 변경과 미추적 QA 기록은 보존한다.
- 기존 지상 최종 보스 전투 패턴, 이동·피격·보상·HUD 계약을 유지하고 인간형 외형/애니메이션을 연결한다.
- Arin RefinedV3 NPC는 프로젝트 원본을 보존하고 자연스러운 대기 자세를 파생 에셋으로 개선한다.
- 보유 Fab 후보: Polyart Modular Stylized Characters, Close Combat Animset, Combat Magic Animations. 로컬 존재와 현재 계정 소유 확인을 구분한다. 외부 획득/라이선스 검토는 별도 진행 중.

## Ultrawork Packets
1. 에셋/리깅 조사: 현재 스켈레톤, 메쉬, 클립, 보스 설정을 Unreal Python으로 읽어 증거 JSON에 기록. 원본 수정 없음.
2. 보스 구현: 기존 패턴 데이터/캐릭터 계약을 재사용하고 필요 시 호환성·접지·타격 시점 기능을 제한적으로 추가. 독립 파생 에셋만 제작. 완료 조건은 지상 인간형 표시와 기존 세 패턴 정상 동작.
3. NPC 개선: 자연스러운 대기 파생 애니메이션, 발 접지와 상호작용 유지. 필요 시 Mixamo→Manny→Arin 리타깃을 검증하며 각 단계 결과를 구분.
4. 통합 검증: BuildEditor.ps1, 관련 자동화/런타임 QA, 렌더 캡처, CheckGeneratedChanges.ps1. 수치 통과와 실제 화면 검증을 구분.
5. 리뷰/문서: feature 결과, 한국어 DevLog, SprintChangeLog. 라이선스·검증 미완료이면 완성/배포로 표시하지 않음.

## 롤백 경계
새 Scripts/소스/파생 콘텐츠와 이번 명시 변경만 되돌릴 수 있도록 목록과 전후 상태를 기록. vendor 원본, 저장 데이터, 보유 기존 QA 결과는 수정하지 않는다.

## 최신 사용자 지시와 설계 반영
- AI는 기존 BT_Monster/BB_Monster와 native BT task 유지. 인간형은 opt-in UAnimInstance native locomotion graph + DefaultSlot montage로 공격·피격·사망 재생. 타격 시점은 기존 authoritative windup/recovery timer를 유지한다.
- AAA 공개 자료: Santa Monica Baldur 첫 보스의 읽기 쉬운 초기 공격/피격 반응(https://blog.playstation.com/2018/08/16/fighting-a-god-behind-the-scenes-of-god-of-wars-first-boss-battle/), Ghost of Tsushima 예측 가능한 공격 예고(https://blog.playstation.com/2020/06/23/ghost-of-tsushima-mastering-the-katana/), TLOU2 neutral idle/호흡(https://blog.playstation.com/?p=334953). 원작 자산이나 내부 구현 복제 없이 예고·회복·고정 타격 방향·미세 호흡 원리를 적용한다.
- 직행 1분 이내, 다섯 마을과 옆길 퀘스트를 통한 성장 약15분 목표는 향후 맵 담당의 범위. 이번 보스 작업은 맵을 독자 변경하지 않는다.
- Fab 현재 소유: Polyart Adventurer https://www.fab.com/listings/8ecc05ee-7447-4a8a-af10-1adccc7e8bd2 (UE5.1–5.9), Combat Magic https://www.fab.com/listings/05d90ca6-d294-4b93-a1b7-232214057850 (UE5.6–5.8), Close Combat https://www.fab.com/listings/0a4a5e43-6941-4a4b-ba7f-2bf7e8015c5c (공식5.6까지,5.8 실제 retarget 검증). 모두 현재 UI의 라이브러리 저장 및 Standard License 표시 확인, 과거 취득 당시 조건 복원과는 구분.
- Mixamo 무료 게임용 animation 사용 근거 https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html . XBot source/raw staging 파일은 게임 통합과 별개이며 standalone 공개 재배포 권리 확인 없이 public push하지 않는다.
