# OpenRunMap 지형·동굴·마을 배치 복구

`Sprint#17-1-combat-key-remap`의 기존 `/Game/Rogue10m/Maps/OpenRunMap`에 복구했다. 통합 전 월드맵은 [GitHub의 `f137e2a9` 체크포인트](https://github.com/chickenbuger/Rogue10m/commit/f137e2a92439877c61a1d074d6bb66de61ec2627)에서 확인했다. 요청 이미지의 수렵촌 화면은 그 맵의 [기존 Unreal 캡처](evidence/landscape-natural-boundary20261117/visual/village-earth-hunter.png)와 일치한다. [ImageGen 전체 지도](../../Docs/References/WorldMap/rogue10m-world-map-concept-20261112.png)는 마을과 진행 경로의 콘셉트 참조다.

- [검증된 `5d23f12e` 복구본](https://github.com/chickenbuger/Rogue10m/commit/5d23f12e68d112475adc77f66f36f4e186de7e4b)의 80개 패키지를 현 브랜치에서 Git 객체와 대조했다. Landscape 컴포넌트 256개, Q 지하 훈련고와 Z 지하 작업함의 구조·장식 인스턴스 712개, 도로·연결로·마을 흙바닥 데칼 39개를 포함한다.
- Unreal 보관본에서 마을 건물과 소품 197개, 울타리·마을 표식 액터 6개와 그 인스턴스 679개를 같은 좌표로 재배치했다. 지면이 맞는 나무·바위 173개를 옮겼다. 서쪽 지형 지지면과 Q 동굴 접근부 높이가 달라 장식 3개를 보류했다. 새 재질은 기존 배너 색상 세 개를 복제했고 목재와 석재는 복구된 동굴 재질을 재사용했다.
- 회색 외곽벽 4개는 메시의 표시를 껐다. 위치, 범위, 충돌 프로필과 Pawn·Camera·Visibility 응답은 저장 전후 동일했다. 남쪽 포탈 도착과 교량을 포함하는 활성 맵 전체 Landscape는 유지했다. 빈 지형 면적을 줄이는 작업은 남아 있다.

새 Unreal 명령줄 세션에서 376개 배치의 위치·메시·재질·충돌과 울타리 인스턴스가 보관본과 일치했고 Map Check는 오류와 경고가 모두 0건이었다. 명령줄 모드에서 레벨 UI의 PIE 상태 API를 호출했을 때 초기 검사가 종료됐고, 월드 API로 바꿔 재실행했다. 외곽벽 레이캐스트는 명령줄에서 충돌을 확인할 수 없어 실제 레이 통과 수에 포함하지 않았다.

현재 작업 폴더를 사용하는 Unreal Editor에서 맵을 다시 열고 [세 마을과 도로를 함께 보이는 뷰포트 화면](../../DevLog/20261122_OpenRunLayoutRestoration/openrun-map.png)을 남겼다. 두 동굴 내부도 3D 화면에서 확인했다. 에디터 레이캐스트에서 중앙로와 세 마을의 지면은 Z=9600cm이고 서쪽·중앙 교량의 상판, 남쪽 도착 지형, 보스 바닥을 확인했다.

두 동굴 16개 지점에서 기본 삼인칭 카메라 치수로 세 방향씩 구체 충돌을 조회했다. 48회 중 9회에 암벽이 카메라를 당겼고 최소 거리는 Q 267.31cm, Z 235.69cm였다. 이 측정은 에디터의 정적 검증이다. `Legacy3` 자동 보행은 실제 StartHub 포탈 진입과 `OpenRunMap` 도착을 포함해 17개 검사를 통과했다.

명령줄 성공 세션의 로그 검사기는 `LoadErrors` 페이지 초기화 행을 오류로 집계했다. 해당 Map Check에는 오류와 경고가 없었다. 인터랙티브 에디터는 가동했고 Zen 서비스 응답은 정상이다. MCP의 기본 8001 포트는 다른 리스너가 사용해 현재 세션을 그 주소에 연결할 수 없었다. 화면 확인은 현재 세션이 직접 저장한 뷰포트 캡처를 사용했다.
