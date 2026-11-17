# Rogue10m 실제 모션 선선택 및 판타지 VFX 제작안

2026-11-10. 확인 컴퓨터: Jinsoo. 코드, 에디터, 공격 데이터, 몽타주, 피해·타수·HitDelay는 이 조사에서 변경하지 않았다. 이 문서는 실제 소스의 확인 수준과 앞으로 만들 연출을 분리한 제작안이다. 너클 자원·계열은 미정이며 시각 색상은 자원이나 속성 확정이 아니다.

## 1. 바로 사용할 판단

- 너클 대표: 기존 HookFlurry는 백열난무의 집중 난타로 보존한다. 연환권의 짧은 연타와 혼동하지 않는다. 새로운 실제 소스 후보는 Fab NinjaRightUppercut(금강쇄), Mixamo Cross Punch(붕권), Punch Combo(연환권)로 선선택한다.
- 대검 대표: 현재 Charged의 Mixamo great sword slash (2)는 패왕참 후보, high spin attack은 회전참 후보다. 지면 슬램을 Charged에 이미 있다고 주장하지 않는다. 대지파쇄에는 별도 jump attack 후보를 검증한다.
- 마법 대표: 보유 Combat Magic Animations에서 실제 파일을 먼저 고른다. 정확한 시각 검증이 된 첫 후보는 AS_ManaCastShot이다. FireballCharge→FireballSpell, MagicDefend, GroundManaShot은 파일 실재가 확인된 추가 후보이며 상세 손·발 포즈와 접촉 프레임은 검증 전이다.
- 동일 소스의 재생속도만 바꾸고 서로 다른 완성 기술로 세지 않는다. 20개 역할 모두에 대응표를 제공하지만 20개 독립 모션을 검증·구현했다는 뜻은 아니다.

## 2. 출처·보유·라이선스·호환성

### F1. Close Combat Animset / Ibrahim Akinde
https://www.fab.com/listings/0a4a5e43-6941-4a4b-ba7f-2bf7e8015c5c
- 현재 로그인 UI: 내 라이브러리에 저장됨 / 스탠다드 라이선스. 새 구매 없음.
- 현재 지원 표기: UE4.15–4.27 및5.0–5.6. UE5.8 공식 표기는 확인되지 않는다.
- 원본 스켈레톤: 로컬 NinjaRightUppercut의 바이너리 문자열에서 UE4_Mannequin_Skeleton 경로 확인. 원본 스켈레톤 파일도 실재한다.
- 로컬 공격: NinjaLeftHook, NinjaRightUppercut, NinjaLowRightThaiKick, NinjaStraightKick.
- 로컬 방어: NinjaBlockIntro, NinjaBlockLoop, NinjaBlockHit. 로컬 회피: NinjaFrontEvade, NinjaBackEvade, NinjaLeftEvade, NinjaRightEvade의 InPlace/RootMotion 버전.
- 위치: D:\Project\Rogue10m\Content\CloseCombatAnimSet\Animations\InPlace\ 아래 Attacks, Blocks, Evades.
- Manny Preview 폴더의 A_CC_LeftHook_Manny, A_CC_RightUppercut_Manny도 실재하지만 파일 존재를 동작·카메라·판정 QA 통과로 해석하지 않는다.
- 상세 라이선스는 취득 시점 조건을 별도 확인해야 하며, 현재 Standard 표기는 과거 취득계약 재구성을 뜻하지 않는다.

### F2. Combat Magic Animations / SoerGame
https://www.fab.com/listings/05d90ca6-d294-4b93-a1b7-232214057850
공식 미리보기: https://www.youtube.com/watch?v=P4Bcb3Bn5H0
- 현재 로그인 UI: 내 라이브러리에 저장됨 / 스탠다드 라이선스 / UE5.6–5.8.
- 상세 기술명세: Epic & MetaHumans Skeleton Yes, IK bones Yes, 35 animations, 전부 In-place.
- Characters0, Materials0, MaterialInstances0, Textures0. 따라서 이 상품 자체가 마법진·VFX를 제공한다고 설명해서는 안 된다.
- 로컬35개 애니메이션 파일이 D:\Project\Rogue10m\Content\CombatMagicAnims\Animations 에 실재한다.
- 대표 파일: AS_Ascension, AS_CastSpell, AS_CastStopSpell, AS_ChantingPrayer, AS_DualEnergyBlas, AS_FireballCharge, AS_FireballSpell, AS_GroundManaShot, AS_HeadRaiseMagicCast, AS_KneelingFireballCast, AS_MagicDefend, AS_MagicStrike, AS_MagicStrike2, AS_ManaCastShot, AS_Revelation, AS_StaffBlast, AS_StaffDefend 등.
- 가격은 보유 상태 UI에서 노출되지 않아 추정하지 않았다. 추가 구매가 필요하지 않다.
- UE5.8 표기지원과 Rogue10m 개별 클립 실사용 QA 통과는 별개다.

### F3. Stylized Characters – Modular Stylized Character Creator – Adventurer / Polyart Studio
https://www.fab.com/listings/8ecc05ee-7447-4a8a-af10-1adccc7e8bd2
- 기존 같은 날 취득 감사에서 보유/Standard/UE5.1–5.9, Epic skeleton/IK/18애니메이션 확인 기록이 있다. 이번 조사에서는 이 페이지를 다시 열지는 않았다.
- 이는 캐릭터 기반 후보이며, 20개 전투 기술의 독립 공격 모션팩으로 대신 세지 않는다.

### M1. Mixamo Great Sword Pack
https://www.mixamo.com/#/?page=1&query=Two+Handed+Sword
- 로그인된 공식 UI에서 Great Sword Pack /51 animations 확인.
- C:\Users\PC\Downloads\Rogue10m_Mixamo_GreatSword_20261110 에51클립+X Bot.fbx 실재. 기존 다운로드는 FBX Binary/30fps/키감소 없음.
- 바이너리 FBX7700 파싱으로 mixamorig:Hips 및 양손 본, TimeMode6(30fps), AnimationStack 길이를 확인했다. 파일 Global TimeSpan 기본값1초 대신 실제 AnimationStack LocalStop을 사용했다.
- high spin: 57샘플, 마지막프레임56, 시간1.8667초. jump attack: 시간2.1667초. power up3.5초. blocking0.5초.
- 기존 기본8 조사 담당의 확인값: idle2.0, walk1.3667, run0.7333, attack1.2, slash1.2667, slash(3)1.8333, high spin1.8667, slash(2)3.5333초. 이 값은 접촉 타임코드가 아니다.
- X Bot/mixamorig는 native Manny가 아니다. source/target IK Rig·기준포즈 보정·손잡이·pelvis scale·발 접촉·root XY/Z/회전 분리를 검증해야 한다. UE5.8 import와 실제 타격 QA는 별개다.
- high spin의 공식 설명은 Great Sword High Spin Attack From Run. 정지 제자리360회전이라고 단정하면 안 된다. 실제 미리보기 재생·57 total frames·몸의 이동 포즈를 확인했으나 검 메시 부착·정밀 접촉 프레임은 검증하지 않았다.
- Adobe 공식 FAQ: https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html . 기존 같은 날 감사에서 무료·게임을 포함한 개인/상업/비영리 사용의 royalty-free 안내 확인. 원본 애니메이션 라이브러리로 별도 재배포하지 않는다.

### M2. Mixamo Punch 검색 및 선선택
https://www.mixamo.com/#/?page=1&query=Punch
- 공식 검색에서 실재 이름/설명 확인: Cross Punch / A Cross Punch, Punch Combo / Four Punch Combo, Combo Punch / Eight Punch Combo, Uppercut / Standing Left Uppercut Punch, Male Action Pose / Exaggerated Punch With Right Hand, Flying Knee Punch Combo / Jumping Knee Followed By A Punch, Elbow Punching / Male Elbow Punch.
- Cross Punch는 X Bot 미리보기 재생/61 total frames와 발을 딛고 가드로 복귀하는 한 장면 확인. Punch Combo는 X Bot 재생/67 total frames와 가드 장면 확인. 아직 새 다운로드·프로젝트 수입은 하지 않았다.
- 목록상 이름만 확인한 나머지는 상세 궤적·루트·타격 프레임 검증 전이다. Flying Knee Punch Combo는 공중 무릎→주먹이지 지면 슬램의 확보 증거가 아니다. Elbow Punching도 어깨 충돌이 아니다.

## 3. 기존20기술과 정확한 대응

각 행의 준비→접촉→회복과 VFX는 앞으로 맞출 제작 목표다. 원본의 정밀 시간분할을 이미 측정했다는 뜻은 아니다. 피해는 현재 승인된 실행·명중 경로를 보존한다.

|기술/후보슬롯|선선택한 실제 모션|준비→접촉→회복 및 판타지 VFX 목표|미확정·기존과 차이|
|---|---|---|---|
|K01 연환권/공용|M2 Punch Combo(Four Punch Combo),67샘플, 온라인 미리보기|가드 압축→손마다 다른 작은 잔상권/압력핵→앞선 잔상부터 소멸|원본4타를 기존 제안3타로 억지 절단하지 말고 실제4타 기반으로 기획 수정 제안. 기본공격과 별도화 필요|
|K02 붕권/공용|M2 Cross Punch,61샘플; 현재 A_Knuckle_BodyStraight 보존|뒷주먹 작은 수렴선→직선 권격에 짧은 투명 압력쐐기/접점핵→옆으로 분산 먼지|신규Mixamo는 미다운로드/리타겟 전. 현재 사용자 제작 모션의 provenance를 Fab 소스로 바꿔 적지 않음|
|K03 금강쇄/공용|F1 NinjaRightUppercut 로컬|낮춘 주먹에 압축된 작은 호→위로 접히는 광편/얇은 입자기둥→상승선이 끊기며 가드|현재 금강쇄 연결이 마무리 단발인 사실과 별개. 올려치기 원본 후보는 확보했으나 신규 적용·띄움 미구현|
|K04 유운수/공용|F1 NinjaBlockIntro/Hit + NinjaLeftHook 조합 후보|열린 얇은 반원→성공시에만 공격선을 흘리는 곡선/반격 접점→실패는 선만 소멸|블록 소스는 진짜 파리 성공 분기가 아님. 방어창/성공토큰/실패회복 별도 필요|
|K05 철산고/공용|적합한 어깨충돌 소스 미선정. F1 FrontEvade는 접근 참고만|낮은 발먼지→어깨 앞 납작한 압력판→제동 뒤 짧은 먼지|팔꿈치/회피를 어깨타격으로 둔갑시키지 않음. 기존 안전 공격 보존, 새 모션 필요|
|K06 파진각/공용|F1 NinjaLowRightThaiKick 로컬; StraightKick은 직선 대안|축발 작은 압축먼지→낮은 초승달/접점에 재질파편→발 착지와 함께 잔광 정리|기존 안전모션 정책 자동 해제 금지. 킥 리타겟·카메라·발 높이 검증 필요|
|K07 백열난무/동시|현재 A/AM_Knuckle_HookFlurry 보존; M2 Combo Punch(Eight Punch Combo)는 미래 대체 후보|양손에 번갈아 작은 집중핵→난타마다 시간차 잔상권/압력링→마지막만 넓은 링 뒤 즉시 소멸|현재6타와 미래8타 혼합 금지. 정확한 타수/피해 유지, 원본8타의 재설계는 별도|
|K08 무극패왕권/동시|현재 A/AM_Fighter_Overlord_PowerPunch 보존; M2 Male Action Pose(Exaggerated Punch With Right Hand) 비교 후보|권에 모이는 가는 나선→한순간 확대되는 반투명 권 형상/낮은 부채압력→외곽부터 찢겨 소멸|현재 charged 실행을 동시기술로 완료 재연결했다고 주장하지 않음. 과장포즈 실제미리보기/공격성 미검증|
|K09 태산압정/차징|적합한 주먹 도약슬램 미선정|발밑 작은 예고형태→착지1회 낮은 압력원/방사파편→먼지만 외곽 잔류|Flying Knee Punch Combo는 슬램이 아님. 소스 부재를 명시하고 기존 AM_Knuckle_Special 유지|
|K10 천붕권/차징|M2 Cross Punch 파생 검토, 현재 charge권은 보존|긴 차지의 작은 이중 압축형→한 점 핵/짧은 직선 균열→검은 빈 공간이 생기듯 잔선 차단|전용 차지준비/회복 미확보. K02와 소스만 공유하고 완성2기술로 세지 않음|
|G01 분쇄참/공용|M1 great sword attack,1.2초|양손의 무게실린 준비→검 궤적에 얇고 넓은 절단면/접점 쐐기→검 제동 뒤 파편 낙하|현재 Primary01 원본. 정확한 오버헤드 여부·접촉프레임 미검증. 기본공격과 기술의 콘텐츠 분리 필요|
|G07 대지파쇄/동시|M1 great sword jump attack,2.1667초, 로컬FBX|실제도약 크기에 맞는 예고선→검 지면접촉 때만 부채균열/파편/짧은 충격핵→낮은 먼지|새 수입·착지·천장·root·판정 미검증. 기존 Charged가 슬램이라는 주장 금지|
|G09 패왕참/차징|M1 great sword slash (2),3.5333초; 현Charged 원본|작은 검날수렴핵→한 번의 두꺼운 절단면/접점파편→길게 남는 회수자세와 짧은 잔광|현재 기본8 복구·검증 진행, 활성 연결/타격프레임 승인 전. 기존HitDelay 유지|
|G02 회전참/공용|M1 great sword high spin attack,57샘플/1.8667초|진입/허리감기→몸이 도는 검날 위치만 호형막/끝쪽파편→회전제동과 외곽소멸|From Run이므로 정지360도 범위와 불일치 가능. root회전 제거로 모션을 망가뜨리지 않고 검증|
|G03 파쇄참/공용|M1 great sword slash (3),1.8333초 비교후보|중심 낮춘 준비→짧고 각진 절단/방어접점 금속파편→균열형 잔광만 소멸|현재Primary03과 공유. 방어붕괴 판정 신규·기본공격 중복 미해결. 전용 최종소스 선정 전|
|G04 돌진참/공용|M1 great sword slide attack 로컬|낮게 전진준비→실제미끄럼 궤적에만 짧은 검날호/종점파편→마찰먼지/제동|슬라이드가 서서돌진과 다름. 실제동작에 맞춰 낮은 진입베기로 수정 제안, 벽·root QA 필요|
|G05 반격참/공용|M1 great sword blocking(0.5초)+slash 조합후보|짧은 검면 준비→성공 접점 얇은 금속호/되베기 접점핵→실패는 검가드회수|block을 성공파리로 해석하지 않음. 분기·실패클립·동일실행1회 성공처리 필요|
|G06 올려베기/공용|slash(4)/(5) 로컬 존재하나 상승궤적 미검증, 최종 미선정|아래로 모은 힘→검날실제상승 궤적의 좁은 광편→위쪽 잔광이 끊김|파일이름 slash만으로 upward선정 금지. 적합한상승 소스 확인 전 원본변경 없음|
|G08 섬멸연참/동시|M1 attack→slash→slash(3) 편집조합 후보|각 준비 무게전환→각베기에 다른방향의 절단면/마지막접점만굵게→최종제동|하나의 네이티브콤보가 아님. 연결포즈/발위치/루트/새타수 검증 전|
|G10 단공참/차징|M1 slide attack + power up(3.5초) 조합검토|짧은 차지핵→승인된 실제전진선에 좁은 절단흔적→종점제동·옆먼지|서로다른클립 연결 필요, 벽관통/적캡슐관통 승인아님. G04와 독립 전용기술 완성으로 세지 않음|

## 4. 마법·마법진·효과를 함께 만드는 대표안

정식 서클의 단계·해금 의미는 사용자 미확정이다. 아래 원형진은 우선 주문 읽기를 돕는 시각 기획이며 별도 게임규칙을 확정하지 않는다. 장착/명령체계는 최신 마법사 규칙을 따른다.

1. AS_ManaCastShot: 공식 미리보기59.78초 화면의 클립명 확인. 맨손 마네킹이 두 발을 딛고 한 팔을 가슴 쪽에, 다른 팔을 바깥으로 굽혀 손을 들어 준비하는 장면 관찰. 손의 실제 발사 포즈·정확한 좌우·release프레임은 추가검증. 준비에는 손바닥 앞 작은 열린 진/주변 점입자, 방출에는 진의 전방선이 열리며 짧은 핵을 발사, 실제충돌에는 좁은방사패턴, 회복에는 손앞진이 바깥부터 풀림을 신규제안한다.
2. AS_FireballCharge→AS_FireballSpell: 두 파일 실재. 원본의 hold/charge구간과 throw/release접합을 검증한 뒤 작은구체·분리된회전호·흡입입자→방출시 손에서분리→실충돌의황백핵/주황외피→위로상승하는잔불을 신규제안한다. 이 세부동작·탄도·이펙트가 원팩에 있다는 뜻은 아니다.
3. AS_MagicDefend: 파일실재. 준비에는 전방 열린 반원진, 실제방어방향에만 얇은중첩호, 성공접점 짧은방사무늬, 끝나면 작은파편처럼 바깥으로풀림을 신규제안. 구체방어/상시무적/반사피해는 이 시각안으로 승인하지 않는다.
4. AS_GroundManaShot: 파일실재. 손/발/몸동작 확인 후 낮은 방향선→지면을따라연속되는핵/균열형광편→종점압력형충돌→전진순서대로잔류선소멸을 제안. 장식용바닥핵 개수와 실제타수를 혼동하지 않는다.
5. AS_HeadRaiseMagicCast: 공식미리보기19.59초 화면명과 상체회전 준비장면 확인. AS_Ascension계열 부유포즈도 관찰했지만 화면표기 철자와 로컬명을 대조할 필요가 있다. 부유는 별도높이·피격·취소정책 전 기획용이며 임의비행 기능을 만들지 않는다.

## 5. 사용자 영상 네 편의 실제 관찰과 적용

모두 실제 플레이어·시각 프레임 접근에 성공했다. 짧은 샘플을 보았고 전편의 모든 동작을 연속시청하거나 음성을 청취한 것은 아니다. 자막 사용불가가 표시됐으며 muted=false가 음성 내용을 들었다는 증거는 아니다. 탐색 직후에는 이전 화면이 잠시 남을 수 있어 아래시간은 대표구간 안내이고 프레임단위기준으로 사용하지 않는다.

### https://www.youtube.com/watch?v=qkPn2ZEbxhU
JSlow /2025 언리얼5 게임 이펙트 포트폴리오(스타일라이즈드 작품 추가),1:56.
- 보라 지팡이끝 구체와 가느다란 흐름리본/점입자, 지면 파손·파편의 잔류, 백청 얼음파편/서리원형영역, 검을든인물 뒤에 흩어지는 작은 잔광을 각각 관찰했다.
- 특히 실제재생70.73초/paused=false/readyState4 때 얼음·서리영역을 확인했다.
- 적용: 효과의 발생원점과 접점을 구분하고, 명중후 잔류는 바닥에 낮게 두어 실루엣을 비운다. 지팡이 효과를 너클의 자원계열로 전환확정하지 않는다.

### https://www.youtube.com/watch?v=5ZQdZOK8hhg
HK RealTime VFX /Unreal Engine5 | Houdini | Game VFX Portfolio – [Lightning],1:36.
- 공중인물의 백색·보라 전기핵/가지번개, 사선의 굵은 낙뢰와 지면의 백색접점/보라연무·붉은불꽃, 대상사이를 잇는 가는체인번개를 각각 관찰했다.
- 영상에 Chain Lightning 및 trace로대상찾기 설명 화면도 확인했다. 이를 프로젝트가 이미 구현했다고 해석하지 않는다.
- 적용: 굵은주형상1개와 가는보조가지로 위계를주고, 끝점의 핵/분출을 분리한다. 원작의 전체백색 노출·큰 섬광을 그대로복제하지 않고 게임가독성에 맞춘다.

### https://www.youtube.com/watch?v=GnoA5k-ktdM
Unreal Effect Concept Class /Unreal Game Effect portfolio _ Student10,0:47.
- 자홍·보라 수직핵/검은방사선, 여러 백청 수직낙뢰·바닥가지번개/파편, 그뒤의 넓게 부서진바닥/외곽먼지·흩어진전기조각을 샘플로 관찰했다.
- 적용: 짧은밝은핵→매체에따른파편→바닥에남는형태의 3층으로 나눈다. exact정밀발생순서·각지속시간은 추가관찰대상이다.

### https://www.youtube.com/watch?v=I5pVCT0KdLg
Unreal Effect Concept Class /Unreal Game Effect portfolio _ Student11,1:07.
- 두마네킹의 무기스윙, 바닥에일렬로이어진청색발광핵/가지전기흔적과작은황금불꽃, 다른씬의노란바닥표적원과중앙생물을 관찰했다.
- 적용: 선형충격은 한 장의 긴리본만그리지 말고 앞서가는핵/뒤따라꺼지는바닥형태로 읽힌다. 표적원은 예고와실충돌을 구분하는 별도모양으로 제작한다.

## 6. 제작 전 통과조건

- 20개 전용모션 모두 확보·완료했다고 보고하지 않는다. 어깨충돌/너클도약슬램/확실한상승대검은 공백이다.
- 모션마다 원본파일·SHA·원본길이·스켈레톤·root정책·파생경로를 기록한 뒤 접촉/회복을 실제영상으로 검증한다.
- 준비/방출/실명중/취소/종료를 별도시각이벤트로 분리한다. 피해는 현재권한실행이 결정하며 VFX Notify에서 중복피해를 호출하지 않는다.
- 장식의반경·잔광·바닥흔적이 곧 피해범위는 아니다. 보스의밀림/띄움면역·벽·천장·절벽을 검증한다.
- 너클FistAuthored5개,현재공격데이터·타수·HitDelay,입력/카메라 안전정책을 보존한다.
- 영상은 형태·위계·발생과소멸을 참고한 새로운VFX기획의 근거다. 원영상의 메시·텍스처·코드·원에셋을 추출하거나복제하지 않는다.


## 2026-11-10 재사용 및 배포 확인

- Adobe Mixamo 공식 FAQ는 개인·상용·비영리 프로젝트의 royalty-free 이용과 게임 제작을 허용한다. 로컬 편집, 리타게팅과 게임 제작은 진행한다. https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html
- Fab 공개 라이선스 설명은 프로젝트 수정/배포와 프로젝트 협업자 private repository 공유를 허용하며 standalone asset의 재판매/무료 재배포는 제한한다. https://www.fab.com/eula
- 원본 FBX 또는 독립 재사용 가능한 derivative animation .uasset의 public GitHub source 배포를 명시적으로 허용하는 근거는 확인하지 못했다. 리타게팅이나 확장자 변경 자체를 재배포 허가로 간주하지 않는다. 개별 자산의 취득 라이선스도 별도 확인이 필요하다.
- 이번 Mixamo/Fab 원본·파생 애니메이션은 로컬 제작/검증에 사용하며, 공개 source asset 배포는 권리 확인 전 보류한다. 이 기록은 법률 확정 판단이 아니며 실제 EULA와 개별 취득 조건 확인을 대체하지 않는다.
- 이번 변경에서 commit/push/merge를 수행하지 않았다. 현재 보유 asset의 기존 공개 이력에 관한 판단은 하지 않는다.
