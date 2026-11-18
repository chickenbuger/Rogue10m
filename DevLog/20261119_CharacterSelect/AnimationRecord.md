# 캐릭터 선택 프리뷰 기록

- 일자/브랜치: 2026-11-19, `Sprint#9-35-character-select-ui`, 실제 렌더 C1119.
- 범위: 정면 UI 프리뷰의 대기 포즈 스냅샷·전신 프레이밍. 원본 클립, Montage, 재생률, 블렌드, notifies, root motion 및 전투 판정 시간은 편집하지 않았다.
- 원본 포즈 소스: `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple`, `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C`. C1119 초기화 Display 로그에 실제 로드된 클래스와 해상도가 기록됐다.
- 카탈로그: `/Game/DataAsset/Character/Customization/DA_CharacterCustomizationCatalog`. 기존 종족별 RetargetAnimClass·mesh·skeleton 참조를 유지했다. 카탈로그 파일은 보호 SHA 대조가 동일하다. 이것은 플레이어 본체의 `ABP_Common_Unarmed` 설정 변경이 아니다.
- 변경: 캡처 직전 delta0으로 원본→리타깃→부착 파츠를 동기 평가한다. 전신 범위와 출력 화면비로 UI 카메라를 맞추고 발을 하단에 배치한다. Tick은 활성화하지 않는다.
- 실제 관측: C1119에서 드워프 남·인간 여의 대기 자세, 전신과 선택 상태를 확인했고 세 슬롯 화면에서는 오크 남도 표시했다. 머리와 발 잘림은 없고 B의 작은 A-pose/공중 배치가 보완됐다. 기존 회색 Debug 몸체 재질은 보존돼 외형 완성을 주장하지 않는다.
- 준비(대기) 근거: [최종 대표 실제 화면](character-selection-final.png). 접촉/최초 피해/회복·손/무기 궤적은 이 UI-only idle 검증에 해당하지 않아 촬영·측정하지 않았다.
- 입력 구분: C는 프로그램 UMG 입력과 실제 Slate hit path, B의 추가 확인은 OS 마우스 입력이다. 일반 전투 런 완주는 실행하지 않았다.
- 재현: Editor 빌드 후 `Scripts/VerifyCharacterLobbyStyle.ps1 -Label <신선한 영문라벨> -Interactive` 실행. QA 프로필·성장·스킬 저장 접두사와 신규 출력 경로를 사전 검사하고 실제 세이브 전후 바이트를 비교한다.
- 실패 보존: Feature/doc/evidence/character-select-implementation-20261119/important-failure/B1119-displaced-outlines.png 및 runtime-review.md. 최종 결과와 원문 실패를 구분한다.
