# 오픈월드 탐험 지도 제작 결과

완성된 `/Game/Rogue10m/Maps/OpenRunMap`의 실제 제작 좌표와 저장된 R16 지형 데이터를 사용해 한국어 탐험 지도를 제작했다. `Sprint#5-13-open-world-map`에서 문서·지도 산출물만 추가했다. Unreal 맵·Content·C++·설정과 기존 미커밋 작업은 변경하지 않았고 커밋·푸시는 하지 않았다.

지도: `Feature/doc/evidence/open-world-map-20261005/OpenRunMap_Map.png`, 2200×2550px. 재현: `Scripts/RenderOpenWorldMap.py`를 번들 Python/Pillow/NumPy로 실행한다. 입력 근거와 좌표→픽셀 투영 기록은 같은 폴더의 `map-data.json`이다.

## 표현과 정확도

1800m 정사각형 지형을 북쪽 +Y·동쪽 +X로 투영했다. 실제 1009² 높이 샘플을 뒤집어 북쪽이 위가 되도록 음영·10m 등고선으로 표현했다. 주요 지역 6개와 제작 도로 정점 3개 경로, 폭60m 강과18m×120m 교량 3개를 동일 축척으로 표시했다.

강은 y=50m·중심선 x=-750~750m인 유한 선분이다. 둥근 끝을 포함해 x=±780m까지이며 맵 가장자리로 이어지지 않는다. 중앙 강 교차로는 중앙 다리와 같은 위치다.

입장 야영지의 안전 반경80m·실제 집3채, 북쪽 유적의 열린 남쪽 입구와 탑 위치, 실제 풍차 좌표(525,-32)m와 고정 보스 좌표(150,698)m를 표현했다. 스포너 5곳은 전투 구역 기호로 표시하며 현재 몬스터가 그 위치에 고정되어 있다고 의미하지 않는다. 시작마을은 별도 맵으로 지도 바깥 포탈 연결 삽입도에 표시했다.

저장된183개 PCG 타일을 기반으로 수목 분포를 기호화했다. 개별 나무 인스턴스 위치를 그대로 캡처한 정사영 화면은 아니며, 수목·건물은 지도 기호다. 잠금 지역·퀘스트 관문·클리어 출구는 추가하지 않았다.

## 검증

- 독립 좌표 검토로 OpenWorldLayout·creation/reload·지형 검사 기록의 지역·길·교량·조우 좌표 일치를 확인했다.
- 렌더 시 creation의 POI와 교량 데이터가 현재 제작 정의와 동일한지 확인했다. PCG183타일의 생성 완료와 빈 실행 메시지를 확인했다.
- R16모서리4곳과 중심 높이를 제작 함수와 비교해 양자화 오차1cm 이내를 확인했다. 원본 입력3개 SHA256을 지도 데이터에 기록했다.
- 한국어 글꼴 존재·모든 라벨의 지도 내부 배치·라벨 사각형 비중첩을 검사했다. 지도 PNG를 실제로 열어 방위·길·3교량·6지역·범례·축척과 라벨 가독성을 육안 확인했다.
- Python AST·PNG 무결성·diff 검사를 수행했다. Unreal 콘텐츠 변경이 없어 에디터 재실행·빌드·PIE는 필요하지 않다.

계획: `Feature/architect/2026-10-05_open-world-map.md`. 입력: `Scripts/Editor/OpenWorldLayout.py`, `Feature/doc/evidence/open-world-20261005/creation.json`, `terrain-height.r16`, `pcg/tiles.json`. 기존 제작 결과: `Feature/doc/2026-10-05_open-world-playmap.md`.

![완성된 오픈월드 지도](D:/Project/Rogue10m/Feature/doc/evidence/open-world-map-20261005/OpenRunMap_Map.png)
