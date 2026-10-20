# 아린 걷기·대화 애니메이션

- 브랜치: `Sprint#5-19-arin-walk-talk-animation`.
- 요청: 기본 걷기와 말하기 애니메이션을 제작·적용한다. 참고 자료가 없으면 Mixamo 홈페이지를 이용한다.
- 기준선: 아린 RefinedV3 BLEND SHA `56cc68fda9a32ec932a83d57a20dbb8897ef51933922c609027a542e2ad01f74`, FBX SHA `45296f9fcbd0d182406c85d29f8627409e090dc0c97ec97815a8035221448f00`. 23제작 본·170cm·1메시·4K PBR와 기존 Idle·대화/보급 기능을 보존한다.

## 설계와 레퍼런스

현재 아린에 걷기/말하기 클립은 없다. 보유 mannequin/Polyart 걷기 에셋은 다른 골격이므로 호환된다고 가정하지 않는다. Mixamo에서 실제 Walking/Feminine Walk Forward와 Talking/General Conversation 프리뷰를 확인했다. 현재 로그아웃으로 FBX 다운로드는 Adobe 로그인이 필요하다. 직접 다운로드한 Mixamo 모캡이라고 주장하지 않고, 공개 프리뷰의 교대 발 착지·반대팔 스윙·고개와 설명 제스처를 참고해 전용 리그에 keyframe을 제작한다.

걷기는 30fps·36프레임 주기·1.2초 in-place loop, 기준 속도110cm/s를 시작 계약으로 사용한다. root는 고정하고 2-bone 다리 IK를 bake해 발바닥 착지/공중 여유를 검사한다. ball 본에는 가중치가 없으므로 독립적인 발가락 구부림은 주장하지 않는다. 말하기는 3초 stationary loop·상체/고개/팔 제스처이며 기존 얼굴·손가락 리그가 없으므로 음성 lip sync와 개별 finger animation은 포함하지 않는다.

게임플레이 전환은 C++ 전용 ActorComponent가 담당한다. Tick은 비활성화하고 0.1초 타이머로 실제 actor XY 이동 속도를 측정한다. 대화 요청 > 걷기 > Idle 우선순위, 시작/정지 hysteresis, teleport 제외, 속도에 따른 walk play rate, 동일 클립 재시작 방지를 구현한다. clip과 기준 속도는 Tutorial DataAsset의 설정이며 Blueprint는 참조만 구성한다. 시작마을 위치와 패트롤 동작은 이번 요청으로 추가하지 않는다.

## Ultrawork Packets

| 패킷 | 목표·영역 | 완료 조건 | 검증 | 롤백 경계 |
|---|---|---|---|---|
| A1 | Mixamo 실제 reference·기준선 조사 | 실제 프리뷰/로그인 제약·출처 기록, 기존 source 보존 | 공식 사이트 관찰·SHA | 새 reference/evidence만 |
| A2 | Blender Walk/Talk motion | loop·root 고정·발 착지·옷/가방 변형·실제 export | fresh animation FBX/BLEND, 핵심 frame/front/profile/oblique, GIF | 새 Art/NPC/Arin/Animation 폴더 |
| A3 | C++ 애니메이션 상태 전환 | 실제 이동/대화 열기·닫기·fallback·Tick 비활성 | BuildEditor, 실제 PIE state/playback | NPC/DataAsset 및 새 전용 component |
| A4 | UE 클립 import·설정·저장 | RefinedV3 기존 골격으로 새2클립, DA 참조와 속도 구성 | sourceSHA/골격/시간·GUIDA·재로드/새 process | 새 Walk/Talk 에셋과 guide 설정 |
| A5 | 통합 검토·문서화 | 기본 튜토리얼67회귀 + 실제 Walk/Talk 전환·재생·이동/대화 종료·출처/한계 | 실제 PIE/엔진 화면/Content 보존 감사·하네스 | 전용 문서/evidence |

## Exit gate

실제 최종 클립을 새 프로세스에서 가져오고 핵심 착지/스윙·제스처 프레임을 열어 확인한다. 정지 이미지나 같은 시퀀스 이름만으로 실제 재생을 승인하지 않는다. 기존 포션2개·중복/만석 재시도·포탈 잔여1개를 보존한다. C++ reflection 변경은 실제 Editor build와 재시작으로 검증한다. 전체 source mesh 재생성·4K 재bake·기존 골격 변경은 피하고 새 애니메이션만 import한다. 일일 한국어 DevLog, Feature/doc, SprintChangeLog를 갱신하며 사용자 승인 없는 commit/push는 하지 않는다.

## 완료 판정 (2026-10-09)

A1~A5를 완료했다. 전용 Walk/Talk와 23본 리그·스킨·PBR를 포함한 별도 Arin_Motions.blend를 제작하고 애니메이션만 기존 UE 골격에 가져왔다. 최종 Editor 빌드에 성공했고, 실제 PIE 159/159(기존 67개 전부·새 23단계)를 통과했다. 임시 5FPS는 플레이어 초기화 후 적용해 실제 게임/벽시계 간격을 확인했으며 종료 후 원래 120FPS로 복원했다. 새 Editor-Cmd PID51740의 저장 재로드와 실제 엔진 6화면 검토도 통과했다.

첫 Walk의 부모 로컬 방향 혼용으로 다리가 늘어난 문제는 armature 좌표 방향으로 수정했다. 타이머가 저프레임에서 한 프레임에 여러 번 호출될 때 Idle로 초기화되던 문제는 bMaxOncePerFrame 및 경과시간 0 건너뛰기로 수정했다. 프레임 제한 검사 두 번의 같은 실패는 Architect 검토로 돌아가 PlayerController BeginPlay의 FPS 덮어쓰기 원인을 확인한 뒤 검사 순서를 수정했다.

Content 신규 2개·Guide DataAsset 변경 1개 외 기존 6,363개 SHA가 같고, 기존 아린 골격·모델·텍스처 9패키지 및 Art 59개 파일을 보존했다. 상세 결과와 범위는 [기능 문서](D:/Project/Rogue10m/Feature/doc/2026-10-09_arin-walk-talk-animation.md)와 [통합 결과](D:/Project/Rogue10m/Feature/doc/evidence/arin-walk-talk-20261009/tutorial-runtime.json)에 기록했다.
