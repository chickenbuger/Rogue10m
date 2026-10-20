# 아린 레퍼런스 기준 수정 설계 — 독립 외형 검토

2026-10-09. 검토자 `/root/arin_reference_art_critic`. 첨부 PNG, 실제 v2 6뷰 시트와 원본 8뷰를 직접 열어 비교했다. 모델 소스 수정, Blender/Unreal 실행, 이미지 생성은 하지 않았다. 이 문서는 수정 전 진단이며 최종 품질 승인 문서가 아니다.

지금 v2는 모험가의 구성 요소는 갖췄지만 새 이미지의 얼굴·머리·천 드레이프·재질에는 큰 차이가 있다. 특히 얼굴과 머리를 그대로 두고 자수와 노이즈만 추가하면 사용자가 요구한 수정에 도달하지 못한다. 제공 FBX의 실제 형태가 레퍼런스와 가깝다면 그 표면과 UV를 수정의 기반으로 사용하고, 기존 v2는 기능/스켈레톤/Idle 유지 및 되돌리기의 기준으로 쓰는 방향이 우선이다. FBX 실물과 텍스처 검사가 끝나기 전에는 이 선택을 확정하지 않는다.

## 1. 형태를 먼저 확정

| 영역 | 이미지의 구체적인 목표 | v2의 실제 차이 | 권장 수정 |
| --- | --- | --- | --- |
| 얼굴 | 큰 아몬드 눈, 어두운 윗눈꺼풀과 속눈썹, 헤이즐/갈색 홍채, 부드러운 코와 볼, 자연스러운 작은 미소 | 좁은 수평 눈, 밝은 녹색 홍채, 눈꺼풀 표현 부족, 단순한 긴 코/중앙 얼굴, 넓고 매끈한 턱 | donor 얼굴이 적합하면 보존. 자체 수정이면 눈구멍/눈꺼풀과 코-볼-입 연결 형태부터 고친다. 앞/양쪽 3/4/프로필/클레이를 확인한다. |
| 머리 | 짙은 밤색, 비대칭 옆가르마, 뿌리 볼륨과 물결치는 굵은 잠금, 얇은 잔머리, 볼륨 있는 앞쪽 땋은 머리 | 매끈한 캡 위의 두꺼운 리본, 갑작스러운 캡 하단 경계, 길고 가는 braid 연결 줄기 | donor hair/UV 보존 우선. 캡-잠금-땋기 연결을 자연스럽게 만들고 큰/중간/작은 덩어리 계층을 준다. 색/strand normal은 그 뒤다. |
| 케이프/후드 | 목 둘레 접힌 후드, 위팔 근처까지 감싸는 둥근 어깨 드레이프, 겹치는 전면 날개, 얇은 잎 테두리 | 얇고 뾰족한 평판 어깨, 등에서 떨어진 삼각형 뒤 플랩 | reference/donor 형태의 접힘과 곡면을 유지. 전체를 팽창시키지 말고 관절 움직임 여유와 층간 두께를 따로 해결한다. |
| 골반/다리 | 허벅지와 무릎 사이 바지의 천 여유, 곡선형 taper, 무릎/발목 주름 | 길고 매우 좁은 매끈한 leg tube | 동일 키에서 donor/기존 모델 front T-pose와 profile 비교. 연속 seat/inseam을 유지하고 다리별 관절 변형을 점검한다. |
| 손 | 엄지 기저 볼륨, 길이 차이가 있는 손가락, 관절/뿌리 taper와 web space | 넓고 특징 없는 손바닥 아래에서 얇고 곧은 손가락 4개, 다각형 끝, 단순 타원 손톱 | 적합한 donor 손 표면을 유지하거나 뿌리/엄지 둔덕부터 수정. 손톱을 두꺼운 액세서리로 확대하지 않는다. 개별 손가락 리깅 여부는 별도로 기록한다. |
| 부츠 | 넓은 접힌 cuff, 가죽 shaft/발목 주름, heel/welt, 발을 감싸는 스트랩 | 좁은 매끈한 튜브, 균일한 둥근 cuff 띠, 둥글게 부푼 toe/sole | 발길이/지면 접촉을 유지하며 shaft와 heel/sole를 조각. strap/buckle/sole를 함께 갱신한다. |

정밀한 수치 비율은 앞 이미지 하나에서 확정할 수 없다. 키와 프레이밍을 맞춘 donor/v2 비교에서 어깨-머리폭, 허리-골반폭, 허벅지/종아리 taper, 부츠-하퇴 길이, 손-전완 길이를 기록한다. v2는 의도적인 stylized 비율이므로 일괄 축소/확대보다 레퍼런스의 상대적인 형태를 따른다. 숨겨진 뒤쪽 디자인은 donor의 실물 표면 또는 명시한 가정으로 채운다.

## 2. 색과 표면 체계

색은 첨부 이미지의 중립적인 영역을 참고하되 렌더 조명과 컬러 관리가 섞인 픽셀을 곧바로 sRGB 재질 값으로 복사하지 않는다. 같은 스튜디오 노출에서 아래 관계가 읽혀야 한다.

- 피부: 따뜻한 peach 기반, 볼/코 주변의 얕은 blush와 주근깨. 얼굴·코·귀·목·손이 하나의 색/roughness 체계를 쓴다. 얼굴에만 사진을 붙이고 목/귀를 단색으로 남기지 않는다.
- 헤어: 깊은 chestnut 갈색, 잠금 방향의 절제된 밝은 하이라이트와 root 변화. 큰 리본마다 다른 단색을 주는 방식은 피한다.
- 녹색 의상: 옅은 mint 대신 muted olive/forest. 주요 접힘의 음영은 실제 형태/normal로 만들고 albedo에 강한 검은 조명 그림자를 고정하지 않는다.
- 블라우스/앞치마: warm ivory. cuff/허리의 큰 주름, 직조 미세 normal/roughness, leaf border를 서로 다른 스케일로 분리한다.
- 가죽: dark russet/brown. 구조적인 이음/접힌 부분/마모를 먼저 보여주고 균일한 noise를 전체에 덮지 않는다. normal은 결, roughness는 마모/패널 차이, albedo는 절제된 색 variation을 맡는다.
- 금속: 둔한 bronze/gold. leaf brooch/buckle/rivet의 작은 highlight가 녹색과 가죽에서 구분되어야 한다.
- 포션: 코르크, 유리 외곽, 붉은 액체가 서로 읽혀야 한다. UE에서 투명도가 불안정하면 불필요한 물리 정확성보다 실제 gameplay view의 실루엣/색 대비를 우선한다.

## 3. 장비와 의상 디테일

레퍼런스의 satchel은 v2의 작은 단일 세로 플랩 주머니보다 폭이 크고 넓은 둥근 flap, 두 개의 버클/스트랩, 층이 있는 측면과 stitch가 있다. 어깨 스트랩은 가슴과 belt/bag attachment에 접촉해야 한다. belt는 허리 곡률을 따라야 하고 buckle 뒤에 갑자기 끊기거나 떠서는 안 된다. forearm bracer는 실제 wrapped shape, 얇은 겹침, rivet와 seam으로 읽혀야 한다.

leaf embroidery는 cape 경계와 coat/apron 하단의 반복적인 가지/잎 방향이 주된 특징이다. 게임 거리에서 안 보이는 개별 잎마다 높은 poly geometry를 넣는 대신 donor atlas 또는 durable cloth texture 소스에 문양을 보존/저작하고, 필요하면 얕은 normal/height로 보강한다. 숫자상 4개 재질/16장 텍스처라는 사실만으로 이 표면이 구현됐다고 판단하지 않는다.

## 4. donor 검사 이후 선택 기준

1. donor가 얼굴/머리/의상 silhouette와 UV/실제 텍스처를 갖추면 donor 표면을 우선 보존한다. 현재 procedural v2에 일부 부속만 덧붙이는 방식보다 reference identity를 더 많이 유지할 가능성이 높다.
2. donor가 전체 단일 remesh라면 semantic 영역별 무리한 분리가 seam을 만들 수 있다. 기존 UV와 재질 경계를 유지한 전체 표면 skin/retopo를 검토하고, 보이는 얼굴/헤어/손부터 데이터 손실을 확인한다.
3. donor에 rig가 없으면 기존 Arin 기능 skeleton에 맞춘 안전한 bind를 root가 결정한다. 1.7m/바닥 중심/중립 T-pose를 먼저 정규화한 다음, 관절 주변 topology와 influence를 검사한다.
4. donor가 textureless이거나 조명 그림자가 baked된 albedo만 있으면 geometry의 likeness와 PBR 표면을 별도로 평가한다. 참조 PNG 단일 정면 projection은 옆/뒤/손/귀의 완성도를 증명하지 않는다.
5. donor가 지나치게 고밀도이면 source를 보존하고 export용 retopo/LOD를 분리한다. 얼굴/손/embroidered silhouette를 indiscriminate decimate하지 않는다. 최종 budget은 root가 실측 데이터로 설정한다.

## 5. 동일 조건 재검증

기존 v2 원본과 exporter 결과는 그대로 동결한다. 수정 source와 새 FBX를 같은 카메라/조명/크기에서 앞, 뒤, 양측, 3/4, 얼굴 양쪽, 손/엄지, 부츠, waist/crotch, shoulder로 렌더하고 실제 파일을 연다. donor 단일 neutral T-pose만 보고 최종 리깅을 승인하지 않는다.

한쪽과 반대쪽 hip/knee bend, reach, elbow bend, torso twist 및 실제 2초 Idle 전 구간을 확인한다. cape, braid, bag, straps, coat split은 몸과 함께 확인하고, 가장 낮은 LOD에서 얼굴/머리/손 silhouette를 확인한다. 새 FBX를 fresh Blender process에서 열고 원본 source와 scale/hierarchy/weights/materials/action을 비교한 뒤 UE에서 실제 BaseColor/Normal/Roughness/Metallic, scale/ground contact, Idle와 기존 NPC 기능을 다시 확인한다.

현재 ledger 결론: `revise_primary_form_before_surface_polish`. FBX donor 실제 검사와 새 모델의 evidence가 아직 없으므로 최종 `retain_repair` 판정은 보류한다.

## donor 실물 검사 후 선택 갱신

추가로 실제 donor 4뷰와 canonical 7뷰를 직접 열었다. 정면 likeness, hood/cape의 drape, 머리 잠금/땋기, 자수, 주름, 부츠, 장비가 기존 v2보다 첨부 이미지와 훨씬 가깝다. 수정의 주된 베이스는 **donor의 전체 표면과 UV/color atlas를 보존하는 방식**으로 선택하는 것이 적절하다. `donor-visual-ledger.json`에 각 영역의 관찰과 근거를 기록했다. 원본 donor를 그대로 최종 게임 캐릭터로 승인한 것은 아니다.

원본은 45,249 verts / 90,186 tris, UV 한 개와 embedded color texture 한 개, rig/action/weights 없음이다. 실제 정면은 +X이고 arm span이 Y라서 기존 Arin 축과 크기/원점을 맞춰야 한다. 한 개 object에 26개의 connected component가 있으며 boundary 453, non-manifold 469, loose vertex 10개가 있다. 숫자 전체를 한 번에 자동 weld/fill하는 방식은 머리 잠금, 의상 틈과 UV에 손상을 줄 수 있으므로 의미가 있는 보이는 위치부터 원인을 확인한다.

우선 확인할 donor의 실제 문제는 다음과 같다.

- 현재 렌더의 색이 레퍼런스보다 밝고 회백색이다. neutral emission/albedo와 색공간을 검사한 뒤 hair/olive/leather 영역을 선택적으로 깊게 하고 PBR response를 구분한다. 피부를 포함한 전체 atlas 일괄 darken은 금지한다.
- canonical face.png의 crown/가르마 부근에 작은 검은 삼각형이 있고, left-hand.png의 bracer seam 하단에도 비슷한 mark가 있다. local geometry opening, intersecting face, baked texture shadow 중 실제 원인을 확인해야 한다. open/non-manifold 전체 수치만으로 그 두 mark의 원인을 확정하지 않는다.
- 눈/입 세부가 color atlas에 포함되어 있으므로 neutral 얼굴이 예쁘다는 이유로 눈꺼풀/표정 리깅 완료를 주장하지 않는다. 당장의 stationary tutorial NPC는 likeness와 Idle를 우선 유지하되 구조적 한계를 기록한다.
- donor의 넓은 cape가 T-pose에서 자연스럽더라도 팔을 내린 순간 심하게 접히거나 관통할 수 있다. coat/apron/braid/bag가 single surface에 섞여 있기 때문에 simple nearest-body weight를 무검증 승인하지 않는다. arms-down과 양쪽 hip/knee부터 실물 렌더한다.
- 90k tris는 기존 v2보다 높지만 전부 51k로 축소해야 한다는 근거는 없다. 편집 원본을 보존하고 새 export budget/LOD를 명확히 정한 뒤 얼굴/손/헤어를 region-aware reduction으로 보호한다.

최신 설계 결론은 `preserve_donor_surface_and_color_uv_as_revision_base_with_targeted_cleanup_pbr_and_compatible_skinning`이다. 최종 승인에는 같은 카메라의 새 source/export와 Unreal evidence가 필요하다.

embedded-original-color.jpg도 직접 열었다. 원본 2K atlas에는 짙은 chestnut/russet, olive, warm ivory와 눈/피부 디테일이 이미 들어 있다. 따라서 studio 렌더가 밝다는 관찰을 "원본 색이 없다"고 해석하지 않는다. 먼저 sRGB BaseColor 연결, neutral emission/albedo, shader와 조명을 진단하고, 원본 atlas를 보존하면서 영역별 PBR response를 보강한다. 전체 darken이나 새로운 정면 얼굴 이미지를 덮어붙일 이유는 아직 없다.

source 검사 담당자는 crown mark가 정규화 위치 대략 x=-0.009,y=-0.12,z=1.623의 7-edge closed boundary loop이고 bracer에도 작은 hole이 있다고 전달했다. 이는 해당 담당자의 실제 topology 검사 정보이며 본 art reviewer가 Blender에서 확인한 사실은 아니다. root는 그 topology/location 결과를 보존한 뒤 이 두 곳을 국소적으로 수정하고 동일 closeup에서 재검증한다.
