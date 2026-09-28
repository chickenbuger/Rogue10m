# 전체 폭 경험치·레벨 바 제작 결과

## 결과

기존 Rogue10m 고딕 HUD와 같은 흑철·청동·붉은 룬 재질로 화면 하단 전체 폭을 사용하는 경험치·레벨 바 이미지를 제작했다. 왼쪽 메달 중앙과 긴 바 내부는 비워 두어 UMG에서 현재 레벨 숫자와 경험치 진행도를 동적으로 표시할 수 있다.

## 산출물

- 최종 투명 PNG: `Content/UI/HUD/Gothic/T_HUD_GothicXPLevelBar.png`
- Unreal Texture2D: `/Game/UI/HUD/Gothic/T_HUD_GothicXPLevelBar`
- 크로마 원본: `Content/UI/HUD/Gothic/Source/T_HUD_GothicXPLevelBar_Chroma.png`
- 후처리 스크립트: `Scripts/PrepareGothicXPLevelBar.py`
- 임포트/검증 스크립트: `Scripts/Editor/ImportGothicXPLevelBar.py`

## 이미지 사양

- 크기: 1920×353px
- 형식: 투명 배경 RGBA PNG
- 구성: 좌측 원형 레벨 메달 + 우측 끝까지 이어지는 단일 경험치 트랙
- 장식: 낡은 흑철, 청동 테두리, 가시 장식, 제한적인 붉은 룬 포인트
- 고정 텍스트/숫자/아이콘/경험치 채움색 없음

## 권장 UMG 적층

1. 최하단 Image에 `T_HUD_GothicXPLevelBar`를 전체 폭으로 배치한다.
2. 레벨 메달 중앙에 TextBlock을 올려 현재 레벨을 표시한다.
3. 긴 검은 트랙 안에 ProgressBar를 올리고 좌우 장식 안쪽에 Padding을 둔다.
4. 경험치 수치가 필요하면 ProgressBar 중앙에 TextBlock을 추가한다.

## 검증

- 크로마키 제거: 투명 픽셀 1,402,104 / 1,573,538.
- 최종 네 모서리 알파: 모두 0.
- 최종 가시 영역: `(6, 6)~(1914, 347)`로 전체 폭 사용.
- Unreal Texture2D: 1920×353, `TEXTUREGROUP_UI`, `NeverStream=true`.
- UnrealEditor-Cmd 재검증: 오류 0, 경고 0, `RESULT=PASSED`.

## 생성 프롬프트

Codex 내장 `imagegen`을 사용했다. 기존 고딕 HUD를 스타일 레퍼런스로 삼아 화면 전체 폭의 얇은 경험치 트랙, 좌측 빈 레벨 메달, 흑철·청동·붉은 룬 재질, 텍스트·숫자·아이콘·채움색 없음, 단색 `#ff00ff` 크로마키 배경을 지정했다.
