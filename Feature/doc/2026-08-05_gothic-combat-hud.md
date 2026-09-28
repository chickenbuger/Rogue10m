# 고딕 전투 HUD 비주얼 개편 결과

## 결과

기존 1920×1080 기준 HUD 배치를 변경하지 않고, 레퍼런스의 어두운 고딕 액션 RPG 분위기를 반영한 독창적인 흑철·청동·붉은 룬 장식 세트를 제작해 실제 Widget Blueprint에 배치했다.

## 주요 변경

- 생성형 이미지 원본을 마젠타 크로마키 배경으로 제작하고 투명 PNG로 변환했다.
- 한 장의 원본에서 좌/우 바 프레임, 중앙 메달, 경험치 프레임, 슬롯 프레임을 파생했다.
- PNG 5개를 `/Game/UI/HUD/Gothic`의 UI용 `Texture2D`로 임포트했다.
- `WBP_Rogue10mMainHUD`에 네 개의 장식 Image를 ZOrder 0으로 배치했다.
- `WBP_QuickSlot`의 기존 `UI_SlotFrame` 브러시에 고딕 슬롯 텍스처를 적용했다.
- VitalBar, Identity, Skill/Item Slot Panel, MonsterInfo, SystemLog의 배경색과 브러시를 같은 어두운 팔레트로 정리했다.
- 체력, 스테미나, 경험치, 아이덴티티, 몬스터 정보, 로그, 획득 피드, 스킬/아이템 슬롯의 기존 좌표·크기·ZOrder와 바인딩 이름은 유지했다.

## 생성 이미지

- 최종 원본: `Content/UI/HUD/Gothic/T_HUD_GothicFrame.png`
- 파생 텍스처: 좌/우 바, 중앙 메달, 경험치 프레임, 슬롯 프레임.
- 제작 방식: Codex 내장 `imagegen` 사용 후 공식 크로마키 제거 도구와 Pillow로 알파 여백 정리.
- 생성 프롬프트 요약: 6:1 비율의 독창적인 고딕 전투 HUD, 좌우 자원 바, 중앙 원형 메달, 좌우 5슬롯, 하단 경험치 바, 낡은 흑철과 청동, 붉은 룬 포인트, 텍스트·아이콘·게이지 색 없음, 단색 `#ff00ff` 배경.

## 자동화

- `Scripts/PrepareGothicHUDTextures.py`: 투명 원본에서 재사용 텍스처 조각 생성.
- `Scripts/Editor/BuildGothicCombatHUD.py`: Texture2D 임포트와 Widget Blueprint 적용/저장.
- `Scripts/Editor/InspectCombatHUDLayout.py`: 변경 전 Designer 트리와 슬롯 좌표 덤프.
- `Scripts/Editor/ValidateGothicCombatHUD.py`: 배치 보존, 장식 위젯, 텍스처, 퀵슬롯 바인딩 검사.

## 검증

- UnrealEditor-Cmd에서 수정 Widget Blueprint 8개 컴파일/저장 성공: 오류 0, 경고 0.
- `ValidateGothicCombatHUD.py`: `RESULT=PASSED`.
- 주요 기존 위젯 9개의 Position, Size, ZOrder가 변경 전 기준선과 일치.
- Texture2D 5개 로드 성공, QuickSlot 필수 바인딩 4개 유지.
- `Scripts/CheckGeneratedChanges.ps1`: 통과. 변경된 바이너리 자산이 Unreal Editor를 통해 생성되었음을 확인.

## 참고

Windows 샌드박스 오류로 Computer Use GUI 자동화는 사용할 수 없었다. 대신 프로젝트의 UE 5.8 `UMGToolSet`과 UnrealEditor-Cmd를 사용해 모든 `.uasset`을 공식 Editor API로 수정·검증했다.
