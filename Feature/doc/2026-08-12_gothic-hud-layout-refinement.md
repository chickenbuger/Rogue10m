# 고딕 전투 HUD 하단 배치 보정 결과

## 완료 범위

- `WBP_LevelExperiencePanel`을 1920x52 하단 스트립으로 재구성하고 `ProgressionWidget`을 `(0, -52, 0, 52)`, ZOrder 6에 배치했다.
- 경험치 진행 색상을 초록색으로 변경했다.
- `WBP_Rogue10mMainHUD`의 중앙 메달리온과 좌우 체력/스태미나, 스킬/아이템 영역을 화면 중앙 기준으로 대칭 배치했다.
- 체력 역할 프리뷰를 적색, 스태미나 역할 프리뷰를 황금색으로 구분했으며 런타임 스태미나 데이터 색상도 황금색 계열로 맞췄다.
- 스킬 왼쪽과 아이템 오른쪽을 채우는 고딕 룬 날개 텍스처를 제작해 각 5칸 슬롯 패널에 연결했다.

## 신규 에셋

- `/Game/UI/HUD/Gothic/T_HUD_GothicSkillWing` (960x208)
- `/Game/UI/HUD/Gothic/T_HUD_GothicItemWing` (960x208)
- `/Game/UI/HUD/Gothic/T_HUD_GothicXPBottomFrame` (1920x52)

이미지 제작은 built-in image generation 모드로 진행했으며, 기존 Rogue10m 고딕 HUD 프레임과 슬롯을 스타일 레퍼런스로 사용했다. 생성 시 순수 마젠타 크로마 배경을 사용하고 공식 chroma-key 제거 도구로 투명 PNG를 만들었다.

## 적용 위젯

- `/Game/Widget/WBP_Rogue10mMainHUD`
- `/Game/Widget/Parts/WBP_LevelExperiencePanel`
- `/Game/Widget/Parts/WBP_SkillSlotPanel`
- `/Game/Widget/Parts/WBP_ItemSlotPanel`
- `/Game/Widget/Debug/WBP_GothicUIShowcase`

## 검증

- Editor target build: 성공
- Gothic UI 자동 검증: `RESULT=PASSED`
- Texture2D 크기/설정, 필수 Widget 바인딩, 하단 경험치 바 좌표, 중앙 대칭 HUD 좌표 확인
- `CheckGeneratedChanges.ps1`: Harness 경로 검사 통과; 프로젝트 내 기존 병행 작업을 포함한 Unreal 바이너리 변경 경고만 출력
- Unreal Editor `WBP_GothicUIShowcase` Designer에서 초록 경험치 바 하단 밀착 및 좌우 슬롯 날개 실제 표시 확인

## 참고

- 현재 열린 Editor가 Hot Reload된 모듈을 사용하면 상속 `BindWidget` 속성의 중복 컴파일 오류가 발생할 수 있어 메인 HUD는 저장 후 전체 Editor 재시작 시 정상 컴파일 대상으로 남긴다. 독립 Widget Blueprint와 자동 레이아웃 검증은 모두 통과했다.
- 최종 1920x1080 배치 미리보기는 `Saved/GothicHUDLayoutRefinedPreview.png`에 저장했다.
