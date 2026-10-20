# Blender MCP 연결과 아린 입체 캐릭터 제작 계획

사용자가 지정한 `hassledzebra/codex_blender_mcp`를 실제 설치·연결하고 아린 NPC용 모델을 제작한다. 저장소에는 SKILL.md가 없고 Python MCP 서버와 Blender 애드온이 있다. 실제 도구로 구현하며 스킬 설치가 완료됐다고 주장하지 않는다. `Sprint#5-16-blender-arin-npc`에서 기존 미커밋 작업을 보존한다.

## 선택한 Ultrawork Packets

| 패킷 | 목표 | 수정 범위 | 완료 조건 | 검증 | 원복 경계 |
| --- | --- | --- | --- | --- | --- |
| B1 | 실제 Blender MCP 세팅 | 프로젝트 전용 환경·애드온·Codex MCP 항목·시작/호출 스크립트 | addon ping/version/scene과 실제 명령 성공, loopback만 사용 | 실제 stdio MCP 결과 | 새 blender 항목·애드온과 소유 프로세스만 |
| B2 | 얼굴·머리·입체 외형 | Scripts/Blender/ArinHead.py | 앞/옆/뒤의 얼굴·눈·코·입·귀·땋은 머리 일관성 | 실제 Blender 렌더 | 신규 모델 소스 |
| B3 | 몸·의상·소품 | Scripts/Blender/ArinBody.py | 170cm 성인 체형·팔/손/다리·분리된 망토·가방·물약2개 | 실제 메시 구조·렌더 | 신규 모델 소스 |
| B4 | 리깅·Idle·FBX | 모델 조립·명시적 웨이트·별도 스켈레톤 | 실제 뼈·가중치·두 초 루프 Idle·FBX/GLB/BLEND | 시간 진행·변형·FBX 재임포트·크기 | 신규 모델 패키지 |
| B5 | 외형 연결 검토 | 전용 Unreal 임포트와 guide data 참조 | 검증 통과 때만 같은 Skeleton의 mesh/Idle, 기존 대화·지급 유지 | 실제 에디터 저장·재로드·집중 PIE | 외형 참조 원복 |
| B6 | 검토·기록 | Feature/doc·DevLog/20261007·SprintChangeLog | 실제 적용/미적용을 구분한 결과·파일 전달 | diff·출처·검사 보고서 | 신규 문서·추가 기록 |

## 제작 계약

지난 Apollo 실루엣과 세 면 사진 투영을 재사용하지 않는다. Blender에서 부위별 실제 메시를 구성하고 얼굴 특징·옷·망토·소품에 독립 형상과 재질을 준다. 초록 여행복·갈색 땋은 머리·보급 가방·물약2개의 아린 콘셉트를 유지한 스타일화된 프로토타입이다. 원본 이미지와 동일한 얼굴을 재구성했다고 보장하지 않는다.

모델 좌표는 Z up/front -Y/미터, 발0·머리1.7m. 머리와 몸 모듈은 나뉘며 root가 실제 Blender MCP 호출을 직렬화한다. Live 애드온의 exec가 globals/locals를 분리하는 문제는 한 네임스페이스 안에서 소유 스크립트를 실행하는 호출 래퍼로 우회한다. vendor 원본을 수정하지 않는다. 미리보기/렌더·진짜 모델 파일을 구분한다.

망토를 팔과 분리하고 가방·물약은 고정 본에 연결한다. 새 humanoid rig와 자체 Idle을 함께 내보내며 기존 Quinn/Manny Skeleton·MM_Idle은 수정하지 않는다. 얼굴/손 품질·입체 외형·팔 내리기/Idle 변형이 수용 가능해야 기존 NPC에 적용한다. 검증은 실패 상한을 완화하거나 파일 존재만으로 게임 품질을 승인하지 않는다.

## 세팅·보존 경계

기존 portable Blender4.5.14를 우선 재사용한다. 활성 사용자 Blender 창/씬을 변경하지 않고 소유 인스턴스에서 작업한다. 새 애드온과 Codex blender MCP 항목만 설치하며 기존 설정을 보존한다. 네트워크는127.0.0.1:9877에 제한하고 외부 유료 모델 API는 사용하지 않는다. 저장소에 라이선스 파일이 없어 vendor 소스는 ignored tmp에 보존하고 전달 패키지에 재배포하지 않는다.

C++·Build.cs 변경이 없으면 전체 빌드를 반복하지 않는다. `.uasset/.umap`은 에디터로만 저장한다. FBX 실제 재수입·170cm·본/가중치/Idle·정면/측면/사선 화면·관련 PIE를 검증하며 커밋과 푸시는 실행하지 않는다.
