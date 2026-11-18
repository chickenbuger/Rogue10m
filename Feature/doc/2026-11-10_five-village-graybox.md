# 다섯 마을 그레이박스 제작 준비 결과

2026-11-10 / **별도 그레이박스 레벨 실제 생성·저장·재로드·경로 충돌 및 시각 검증 완료**.

## 실제 상태

기존 `OpenRunMap.umap`과 `StartHub.umap`은 보존하고, **새 `.umap`에92개 액터를 실제 배치·저장했다.** 다섯 마을 부지·직행축·갈림길·왕복 도로와 예약 마커가 있는 별도 후보이며 기존 런 포털에는 연결하지 않았다. 마을별 퀘스트·상점·던전과 재방문 진행 상태는 아직 구현되지 않았다.

- 실제 레벨: `/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110`.
- 범위:10개 지점,19개 양방향8m 도로,15개 건물 더미,15개 서비스/던전 예약 마커, 비차단 보스 입구와 전투 부지.
- 중앙 직행300m는 저장된 실제 도로 중심선에서도 확인했다.6m/s에서50초는 계산값이며 실제 보행·60초·15분 콘텐츠·보스전은 미검증이다.
- 기존 월드·포털·보스·NPC·대검·카메라·입력·세이브 연결은 변경하지 않았다. 커밋·푸시도 하지 않았다.

## 준비와 검증

[배치 도면](evidence/five-village-graybox-20261110/layout-preview.png)은 오프라인 자료다. 실제 엔진 화면은 [전체](evidence/five-village-graybox-20261110/reviewed-view-overview.png), [사선](evidence/five-village-graybox-20261110/final-view-south-perspective.png), [낙엽촌 근접](evidence/five-village-graybox-20261110/final-view-village-v1.png)을 직접 열어 확인했다. 카메라/좌표 축은 캡처 메타데이터에 남겼으며 도면을 엔진 캡처로 대체하지 않았다.

| 검증 | 실제 결과 |
| --- | --- |
| 제작·재로드 | 새 레벨1개+재질 인스턴스13개.92액터의 label·위치·회전·scale·메시·재질·충돌이 원본 맵을 경유한 재로드 후 일치 |
| 정적 배치 |544검사 실패0.8m도로·건물 최소여유1.567581m, 전체 메시 바닥 경계, 결정성·음성 대조군 확인 |
| 엔진 충돌 |748검사 실패0.19간선38방향 capsule sweep,636개5m바닥표본, capsule-floor 양성 대조군. PIE·CharacterMovement 테스트와 구분 |
| 원본 보존 |StartHub/OpenRunMap/L_Menu 파일 SHA256 불변. 기존 맵·vendor·대검·NPC·세이브 저장 호출 없음 |
| 종료 |PID53928 정상 종료,20:20:54UTC LogExit. 이후 UE/UBT/dotnet 프로세스0 확인 |

엔진 기본 `/Engine/BasicShapes/BasicShapeMaterial`의 Color 파라미터를 읽기 확인하고13개 프로젝트 소유 MaterialInstance로 재사용했다. 새 셰이더 그래프13개를 만들지 않았다. 첫 호출은 UE5.8 setter가 값을 쓰고도 항상false를 반환하는 구현 때문에 중단됐다. 실제 색상과 빈 레벨을 확인한 후, 기록된 첫 재질/빈 후보에만 한정된 이어 만들기로 복구했다. 실패 보고서도 보존했다. 최종 결과를 엔진 로그 전체 오류0으로 표현하지 않는다.

첫100000lux 캡처의 노출 경고는 보존했다. 후보 광원만6000lux로 낮춰 저장·재로드했고, 전체 카메라의 노출이 안정된 뒤 경고 없는 최종 전체 화면을 확보했다. 프로젝트 CVar·설정은 바꾸지 않았다. 최종 마을은 건물 더미와 색 구분 단계이며 완성 미술·조명 품질은 아니다.

## 남은 조건과 다음 작업

상위 담당자가 대검 PID42616 정상 종료와 에디터/빌드 사용권 이전을 명시적으로 알린 뒤 시작했다. 시작 직전 UE/UBT/dotnet0을 확인했고, 사용 후 원래 StartHub·dirty0·PIE없음으로 복귀해 에디터를 완전히 종료했다. 인계 대기는 해소됐다. 신규 C++ 변경이 없어 빌드는 실행하지 않았다.

후속 작업은 생산 Character/카메라/입력·20분 시계로 실제60초 보행을 확인하고, 기존 월드의 입장·보스 앵커와 통합하는 것이다. 선택 퀘스트·서비스·던전 왕복 상태는 예약 마커와 분리해 다음 수직 구현으로 연결한다. 실제15분 콘텐츠, 물리 입력·전체 런은 남아 있다.

최종 자산·해시·검증·종료 근거는 [final-verification.json](evidence/five-village-graybox-20261110/final-verification.json), 재현·인계 절차는 [README](evidence/five-village-graybox-20261110/README.md), 롤백 경계는 [architect 문서](../architect/2026-11-10_five-village-graybox.md)에 있다. 현재 미해결 도구 차단·승인 대기·실행 결과 불명인 동작은 없다. 커밋·푸시를 하지 않았다.
