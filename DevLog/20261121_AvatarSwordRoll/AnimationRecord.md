# 드워프 남성 대검 구르기의 손 지지

- 캐릭터는 StartHub의 실제 `SK_Dw_M_FullBody`와 기존 `IKR_Manny_To_Dw_M` 리타깃을 사용한다. 맨몸 애니메이션 원본은 이미 제작한 Blender 파생본 `/Game/Rogue10m/Animation/Common/Evade/A_CommonShoulderRoll`이며 이번 수정에서도 같은 클립을 재생한다.
- 60Hz 고정 실행에서 인벤토리로 `DA_Item_IronLongsword`를 장착하고 실제 Ctrl과 뒤 방향 프로그램 입력을 보냈다. 구르기는 304.5587cm를 이동했다.
- 대표 PNG는 기존 손 지지에서 오른팔이 몸을 받는 진행률 42.0%다. 표시된 오른손바닥과 손잡이는 같은 위치를 사용하며 검과 잔디 사이에 그림자가 분리되어 보인다. [대표 화면](sword-support-720p.png)
- 1280×720 Microsoft Basic Render Driver에서 촬영했다. 같은 카메라에서 진입 9.3%, 14.0%, 18.7%와 손 지지 42.0%를 비교했다. [진입 비교](../../Feature/doc/evidence/avatar-motion-20261121/dwarf-sword-entry-720p.png)
- 정확한 수치와 회귀 검증은 기능 기록에 보관한다. [기능 기록](../../Feature/doc/2026-11-21_avatar-motion-coverage.md)
