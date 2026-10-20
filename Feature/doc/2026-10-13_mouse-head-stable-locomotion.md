# 마우스 기준 머리 안정화·안전 공격·이동 속도 연동

완료일: 2026-10-13
브랜치: `Sprint#5-30-mouse-head-stable-locomotion`
환경: Unreal Engine 5.8, 현재 인간 남성 Appearance Mesh/LOD1, 단일 플레이

## 적용 결과

머리와 카메라의 시선을 마우스 기준으로 분리했다. 머리가 공격 원본의 큰 회전을 따라 돌아가지 않으며, 상체·팔은 기존 시야 조준을 따르고 하체는 실제 보행과 FootIK를 유지한다. 별도 First Person Mesh는 사용하지 않는다. V키 외형 점검과 머리까지 포함한 그림자는 유지한다.

각도는 마우스에 대한 잔여 quaternion의 총 회전 상한이다. 마우스 자체의 큰 회전을 제한하거나 각 축에 별도 상한을 더하지 않는다.

| 상태 | 머리·카메라 회전 상한 | 카메라의 몸 기준 위치 잔여 상한 | 최종 실행 실측 회전 최대 |
|---|---:|---:|---:|
| 정지·걷기 | 2° | 1cm | 2.000000° |
| 달리기 | 4° | 2cm | 3.805321° |
| 공격·차징·blend-out·공중·회피 | 3° | 1.5cm | 3.000000° |

달리며 공격하거나 차징하면 3°가 우선한다. 공격 종료·착지한 프레임에는 실제 평가 상태에 맞춰 바로 2°로 제한한다. 기본 시야 `-20°`, 눈 오프셋 `(8,0,-3)cm`을 유지했다. 몸 기준 흔들림만 감쇠하므로 실제 캐릭터 이동과 점프 높이는 즉시 카메라에 반영된다.

## 구현

- Native AnimInstanceProxy가 기존 최종 그래프를 한 번 평가한 뒤 목·머리 회전을 보정한다. 목은 기본 mouse aim 35%, 머리는 전체 시선을 적용한다. 보간 뒤에도 상태 상한을 다시 적용하며, source 렌더 본의 local 위치·scale와 하체·손목·손가락 회전은 보존한다.
- PreUpdate의 이전 상태로 공격 종료를 판단하지 않도록 GT PreEvaluateAnimation에서 제한값을 다시 읽는다. 실제 평가에 사용한 mouse/source basis만 PostEvaluate로 공유하고 Appearance finalization 때 그 포즈와 묶어 카메라에 전달한다. 워커가 UObject를 읽지 않는다.
- 카메라는 Appearance head에서 이미 적용한 mouse basis를 제거한 잔여 회전에 현재 입력을 한 번 합성한다. 늦게 바뀐 source 월드 회전은 정확히 transport한다. 기준 head anchor에서 위치 잔여만 제한하며 같은 프레임의 반복 camera/trace 호출은 필터를 추가 진행시키지 않는다.
- StylizedCharacter의 실제 컴포넌트 등록 뒤 Movement→Animation Source Mesh→Appearance 의존 순서를 구성했다. 집중 QA도 이 production 의존성을 요구하며 Source에 테스트 전용 의존성을 대신 추가하지 않는다.
- Combat의 자원·쿨다운·타격 예약 이전과 직접 PlayCommonMontage 양쪽에서 검토된 금지 소스와 중첩 segment를 검사한다. 실패한 원본으로 돌아가거나 피해만 재생하지 않는다.
- 실제 수평 속도로 보행을 제어한다. 현재 BlendSpace의 Idle0/Walk300/Jog600 샘플 혼합은 유지하고 600cm/s 초과에서는 속도/600으로 재생률과 발자국 간격을 조정한다. 실측 900→1.5배, 1200→2배다. 공중·회피는 별도 동작을 유지한다.
- 공격은 승인 당시 `AttackSpeedMultiplier × Skill.AnimationPlayRate`와 타격 예약의 기존 공속 계약을 유지한다. 공격 도중 능력치가 바뀌어도 이미 승인된 동작·타격 시간은 바뀌지 않는다.

## 안전 모션과 자산

활성 스킬 35개의 montage 참조와 CommonMotion의 회피를 정면 잽·훅·짧은 펀치·0.2초 비회전 스텝으로 교체했다. HeadSafe 폴더에 새 sequence7개·montage33개를 만들었다. 기존 MM_Attack_01/02/03, MM_Dash, Overlord PowerPunch의 과도한 회전과 보관된 Martelo·뒤돌기 위험 킥·전후 구르기는 활성 경로에서 허용하지 않는다. 원본은 삭제하거나 덮지 않았다.

74개 공격 DA의 montage 외 피해·소모·타격 횟수·쿨다운·공속·첫 타격 지연을 확인했고, 13개 프로필의 해금·입력 바인딩을 보존했다. montage33개의 섹션 시각·연결·metadata와 전체 Notify 상태는 native 비교로 교체 전후 동일함을 확인했다. 저장한 자산을 새 Editor 프로세스에서 읽었을 때 활성69 montage·26 source에 회전 위험 후보가 0개였다. 알려진 위험 목록과 이번 자산 감사를 근거로 하며, 미래의 모든 미검토 클립을 자동 분류하는 기능은 아니다.

어퍼컷은 source0.21초 접촉을 실제 기존19/30초에, Overlord 정면 펀치는 source0.22초를 기존0.28333333초에 맞췄다. 손의 root 상대/컴포넌트 T/R/S, 머리·골반·양발 높이와 모든 local key를 검증했다. 원본 첫 root 위치는 스탠스의 기준 높이이므로 유지하고 이후 travel만 고정했다. 파생 클립의 최대 구조 위치 오차는 약0.000374cm이며 원본 local 위치·scale 오차는0이다. 모든 무기에서 주먹과 실제 표적의 시각적 접촉까지 인증한 것은 아니다.

Common AnimBP는 기존 단일 Walk / Run BlendSpace의 PlayRate에 native getter 하나만 연결했다. FootIK·가드·이동 공격 마스크·slot·다른 그래프 변경이 없음을 비교하고 compile·저장·fresh reload를 확인했다. 모든 자산 쓰기는 Unreal Editor API로 수행했다.

## 검증

| 검증 | 결과 |
|---|---|
| 최종 Editor 빌드 | build6 성공56.43초; 직전 production 빌드5 성공11.37초 |
| 실제 runtime04 | 24상황·1800프레임·489428기록 검사·실패0 |
| 실제 render02 | 같은24상황·1800프레임·489428기록 검사·실패0, PNG1800장 연속1280×720 |
| 머리 분리 비활성 대조 | 21상황·1575프레임; head cap3150개와 basis transport1개만 예상 검출 |
| Fist02 전투 회귀 | 840프레임 실패0, 맨손4/너클5 입력·14실제 피해 펄스·V·무기 전환 취소·점프·머리 그림자 |
| Moving01 회귀 | 1100프레임·258033검사·실패0, 9스킬·16피해 펄스·정지/차징/점프 취소·원본 pose/FootIK |
| fresh 자산 읽기 | 74 DA 비모션 필드·13프로필·graph 범위 보존; unsafe active0 |
| 독립 검토 | 기획1·개발2·검증2 분담; source20파일 검토 승인과 수치·원본PNG9장 관찰 |

최종 runtime/render 로그의 검사 수489429와 metrics489428의 차이는 마지막 metrics 저장 확인1회다. Moving01은 build4에서 동일 Source→Movement fixture 조건으로 실행했고, 그 뒤 production에 옮긴 순서는 강제 조건이 없는 runtime04와 Fist02로 검증했다. 동일 회귀를 불필요하게 반복하지 않았다.

원본 대조에서 source 하체 회전 최대오차0.000002958°, local 위치·scale 오차0, 손목·손가락 회전 최대오차0.000003818°다. Appearance8개 팔다리 길이 변화 최대0.000005157%이며 유한성·정규화도 검사했다. 이는 전체 target local T/S·손가락 피부 표면 보존을 인증하는 값은 아니다. 카메라 반복 read의 위치 오차0, 늦은 마우스 합성 오차0.000002415°, capsule100cm 이동과 source 월드 회전80° transport 오차0이다.

초기 실패도 보존했다. application1은 root 좌표 비교, application2는 UE5.8 Python 비노출 섹션 속성에서 저장 전에 중단했다. 원본133 SHA 변경0·HeadSafe 디스크 파일0을 확인한 뒤 수정했다. runtime01의 blend-out 경계와 Fist01의 착지 평가 순서는 production에서 고쳤다. 촬영용 render01의36프레임 단축은 쿨다운·훅 종료 검사를 손상해5건 실패했으며, 최종 촬영은 일반 실행과 같은75프레임을 사용한다. 검사 상한·비용·쿨다운을 완화하지 않았다.

## 실행 영상과 관측 범위

[25초 주요 장면](D:/Project/Rogue10m/tmp/mouse-head-stable/mouse-head-stable-preview-20261013.mp4) · [60초 전체 검증](D:/Project/Rogue10m/tmp/mouse-head-stable/mouse-head-stable-full-20261013.mp4)

현재 기본 idle/잽·V잽, 보행·달리기·공속별 잽·너클·점프·안전 회피를 담았다. 1280×720/30FPS/H.264이며 장면 사이 이동은 검증 case 전환이다. 관절 구조를 보기 위해 촬영 프로세스의 모션 블러만 껐고 production 설정은 바꾸지 않았다.

기본1인칭에서는 내려간 손과 공격 팔 일부가 하단 HUD 뒤에 가린다. V에서 머리·목·팔·다리 연결과 비회전 동작은 관찰했으나 뒤쪽 시점만으로 손가락 표면 전체를 인증하지 않는다. QA의 숨겨진 collision floor는 보이는 테스트맵 바닥보다 높으므로 영상만으로 모든 발의 지면 접촉을 판단할 수 없다. 현재 외형/LOD1 단일 플레이 범위이며 다른 외형·LOD 전환·사망·메뉴 차단 각각의 새 런타임, Shipping cook·네트워크·장기 밸런스 검증은 수행하지 않았다.

## 보존·작업 파일

시작 Source/Config/Scripts455개 중14개 수정·7개 추가·삭제0이다. Content6359개 중 의도한 기존37개(DA35+MotionData1+CommonBP1)만 바뀌고 HeadSafe40개만 추가됐다. 다른 Content의 크기/mtime 변화·삭제0, 원본 focused SHA의 범위 외 변경0을 확인했다. 생산 프로필·진행도·스킬 키3개의 SHA/목록도 원본 그대로다. 이전 미커밋 작업을 보존했다. 커밋·푸시는 하지 않았다.

핵심 변경은 AppearanceCamera·ViewAlignedUpperBodyProxy·FighterCombatStanceAnimInstance, CharacterAnimation/Combat/Character 안전 gate, StylizedCharacter 평가 순서다. Editor helper2개와 제작/감사 스크립트3개·집중 QA/wrapper를 추가했고 Fist/Moving 회귀 oracle를 새 계약에 맞췄다. 설계·결과 문서, 오늘 DevLog와 SprintChangeLog를 기록했다.

원래 OpenRunMap 에디터를 새 빌드로 다시 열었고 공식 MCP로 PIE종료, 읽기 전용 API로 dirty0·2/4/3기본값·mouse head/upper aim 활성·안전 Dodge 참조를 확인했다. 생성 경로와 범위 내 문법/공백 검사 결과는 증거 폴더에 남긴다.

관련: [설계](D:/Project/Rogue10m/Feature/architect/2026-10-13_mouse-head-stable-locomotion.md), [독립 QA](D:/Project/Rogue10m/Feature/doc/evidence/mouse-head-stable-locomotion-20261013/qa-review.md), [소스 리뷰](D:/Project/Rogue10m/Feature/doc/evidence/mouse-head-stable-locomotion-20261013/final-source-review.md), [증거](D:/Project/Rogue10m/Feature/doc/evidence/mouse-head-stable-locomotion-20261013/).
