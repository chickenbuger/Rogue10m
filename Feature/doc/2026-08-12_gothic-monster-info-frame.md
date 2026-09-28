# 고딕 몬스터 정보 프레임 제작 결과

## 결과

몬스터를 조준했을 때 표시되는 현재 420×68 `MonsterInfoWidget`을 꾸밀 수 있도록 고딕 정보 프레임 이미지와 Unreal Texture2D를 준비했다. 기존 Widget Blueprint와 바인딩은 변경하지 않았다.

## 산출물

- 최종 투명 PNG: `Content/UI/HUD/Gothic/T_HUD_GothicMonsterInfoFrame.png`
- Unreal Texture2D: `/Game/UI/HUD/Gothic/T_HUD_GothicMonsterInfoFrame`
- 크로마 원본: `Content/UI/HUD/Gothic/Source/T_HUD_GothicMonsterInfoFrame_Chroma.png`
- 후처리 스크립트: `Scripts/PrepareGothicMonsterInfoFrame.py`
- 임포트/검증 스크립트: `Scripts/Editor/ImportGothicMonsterInfoFrame.py`

## 이미지 사양

- 해상도: 840×136 RGBA. UMG에서 420×68로 표시하는 2배 원본이다.
- 가시 영역: `(10, 14)~(830, 121)`.
- 상단 빈 명패: `LV N : 몬스터 이름` TextBlock 영역.
- 하단 빈 트랙: 체력 ProgressBar와 현재/최대 체력 TextBlock 영역.
- 좌우 마름모 홈: 추후 속성 또는 상태 아이콘을 올릴 수 있는 선택 영역.
- 고정 문자, 숫자, 아이콘, 초상, 체력 채움색 없음.

## 권장 UMG 적층

1. `WBP_MonsterInfo` 최하단 Image 또는 Border Brush에 프레임을 420×68로 배치한다.
2. `UI_MonsterNameText`를 상단 명패 중앙에 둔다.
3. `UI_MonsterHealthBar`를 하단 검은 트랙 안쪽에 둔다.
4. `UI_MonsterHealthText`를 ProgressBar 중앙에 겹친다.
5. 속성 표시를 다시 사용할 경우 좌우 마름모 위에 작은 Image를 별도 배치한다.

## 검증

- 크로마키 제거: 투명 픽셀 1,351,603 / 1,573,328.
- 최종 네 모서리 알파 값 0.
- 원본 비율 유지 후 840×136 투명 캔버스 중앙 정렬.
- Texture2D 크기 840×136.
- `TEXTUREGROUP_UI`, NoMipmaps, `NeverStream=true`.
- UnrealEditor-Cmd 오류 0, 경고 0, `RESULT=PASSED`.

## 생성 프롬프트

Codex 내장 `imagegen`으로 기존 고딕 인벤토리/HUD를 스타일 레퍼런스로 사용했다. 흑철·청동·붉은 룬, 상단 빈 이름 명패, 하단 빈 체력 트랙, 작은 좌우 마름모 홈, 고정 텍스트·아이콘·채움색 없음, `#ff00ff` 크로마키 배경을 지정했다.
