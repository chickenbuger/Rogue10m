# 대검·맨손 아이덴티티 이미지

- 완료일: 2026-10-14
- 브랜치: Sprint#5-31-identity-image-assets
- 제작: 기본 built-in imagegen. 자산별 신규 생성1회와 같은 형상의 프레이밍 편집1회. CLI/API fallback과 수동 픽셀 편집은 사용하지 않았다.
- 범위: 신규 프로젝트용 원본 PNG2개 제작·검증. 실제 Unreal Texture import·HUD 연결·게임 코드 변경은 수행하지 않았다.

## 최종 이미지

| 이미지 | 표현 | 원본 |
|---|---|---|
| 대검 | 흑철·은빛 날, 황동 가드·가죽 손잡이, 붉은 주황 균열 | [T_Identity_GreatSword_v2.png](D:/Project/Rogue10m/SourceArt/UI/Identity/2026-10-14/T_Identity_GreatSword_v2.png) |
| 맨손 | 기존 돌주먹 계열의 네 마디와 엄지, 금빛 균열·비취 기운 | [T_Identity_Unarmed_v2.png](D:/Project/Rogue10m/SourceArt/UI/Identity/2026-10-14/T_Identity_Unarmed_v2.png) |

![대검](D:/Project/Rogue10m/SourceArt/UI/Identity/2026-10-14/T_Identity_GreatSword_v2.png)

![맨손](D:/Project/Rogue10m/SourceArt/UI/Identity/2026-10-14/T_Identity_Unarmed_v2.png)

현재 HUD의 흑색·금색 프레임과 원형 게이지 안쪽 사용을 고려했다. 문자·수치·외곽 프레임은 넣지 않았다. v1의 큰 실루엣을 v2에서 균일하게 줄여 본체 높이를 대검75.76%, 맨손72.65%로 맞췄다. v1 원본은 로컬 초안으로 보존했고, 최종 v2 PNG2개만 기존 SourceArt PNG 제외 규칙의 예외로 지정했다.

## 검증과 범위

기획1·제작2·검증2 역할로 분담했다. 검증 담당들이 실제 원본 이미지를 읽었으며 양쪽 PNG는1254×1254 RGBA, 알파0~255, 손상 없이 정상 디코딩됐다. 실질 본체와 검끝·가드·손잡이·주먹이 잘리지 않았고 네 마디와 교차 엄지를 확인했다. 두 프롬프트와 파일의 Git 제외 여부도 검사했다. 미세 저투명도 잔여 픽셀 일부는 있으나 불투명 배경이나 체크무늬를 넣은 이미지가 아니다.

현재 실제 아이콘 공간은68×68이다. 실루엣과 색 대비를 우선했지만 실제 HUD에 연결한 뒤의68px 실행 검증은 수행하지 않았다. 대검은 주먹보다 폭이 좁아 연결 시 시각적 체급을 확인해야 한다. 대검의 실제 아이덴티티 자원을 분노로 확정하거나 매핑을 변경하지 않았다. PNG 제작만 했으므로 Unreal 빌드는 실행하지 않았다.

## 생성 프롬프트 원문

- 대검 신규 생성: [v1 프롬프트](D:/Project/Rogue10m/SourceArt/UI/Identity/2026-10-14/T_Identity_GreatSword_v1.prompt.txt)
- 대검 여백 편집: [v2 프롬프트](D:/Project/Rogue10m/SourceArt/UI/Identity/2026-10-14/T_Identity_GreatSword_v2.prompt.txt)
- 맨손 신규 생성: [v1 프롬프트](D:/Project/Rogue10m/SourceArt/UI/Identity/2026-10-14/T_Identity_Unarmed_v1.prompt.txt)
- 맨손 여백 편집: [v2 프롬프트](D:/Project/Rogue10m/SourceArt/UI/Identity/2026-10-14/T_Identity_Unarmed_v2.prompt.txt)

주요 지시는 단일 무기/주먹, 기존 검정·금색 HUD와의 조화, 투명 배경,64px에서 읽히는 큰 실루엣, 외곽 게이지·문자 없음이다. 편집은 같은 형상·재질·색을 보존하면서 중앙 크기와 여백만 조정하도록 요청했다. 정확한 전달 문장은 각 파일에 저장했다.

최종 SHA256:

- 대검: c4ec52066143a16c033416a1c2b17fdbe03ce5ca45e5b4cc4dbd83da14182267
- 맨손: 52498f4bb5d9da2437015d7a719b1fb8ab41ea9b78e2d66dfbf35042c82d0656

관련: [기획](D:/Project/Rogue10m/Feature/architect/2026-10-14_identity-images.md), [파일 QA](D:/Project/Rogue10m/Feature/doc/evidence/identity-images-20261014/file-qa.json), [독립 시각 QA](D:/Project/Rogue10m/Feature/doc/evidence/identity-images-20261014/visual-qa.md).

기존 작업과 원본 돌주먹을 덮지 않았다. 커밋·푸시는 하지 않았다.
