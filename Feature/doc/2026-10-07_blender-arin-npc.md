# Blender MCP 연결과 아린 제작 결과

실제 Blender4.5.14 LTS와 사용자 지정 upstream MCP/애드온을 설치·연결했다. 전용 loopback9877과 isolated 사용자 설정을 사용하고 Codex `blender` 항목을 추가했다. upstream은 SKILL.md가 없는 MCP 저장소이며 vendor 원본을 수정하지 않았다.

아린의 실제 입체 메시175개·21,726정점·42,764삼각형,23본 rig·정지 root·2초 Idle과 BLEND/FBX/GLB를 제작했다. 얼굴·머리·몸·의상·가방·물약은 메시이며 참조 사진 투영을 사용하지 않았다. 실제 FBX 재수입18/18과 StartHub 실제 PIE64/64가 통과했다. UE에서 FBX armature root가 추가되어24본이고, 같은 Skeleton의 SK_Arin/ArinIdle을 안내 NPC에 적용·저장했다.

전용 Unreal 폴더 `/Game/Rogue10m/Characters/Arin/Blender`의 신규19개와 guide DA/BP/외부 액터3개만 변경했다. Content 크기·수정시각 기준6312개 중6309개가 같고 삭제0개다. 대화·물약2개·중복방지·만석 재시도·실제 소모 후 포탈 잔여1개 전달을 검증했다. 이 실행은 reflected interaction API를 사용했으며 실제 F 입력·화면 가독성을 새로 검사했다고 주장하지 않는다.

v1은 단색 PBR15섹션이며 UV·텍스처·LOD·얼굴/손가락 리그가 없는 스타일화된 프로토타입이다. 당시 FBX 검사는 소유 live Blender에서 수행했으며 새 프로세스 검사가 아니다. C++ 변경·전체 빌드·패키징·성능 검사는 하지 않았다. 검증 근거는 `Feature/doc/evidence/arin-blender-20261007`에 있다. 사용자 후속 지시에 따라 v1은 Versions/v1과 quality baseline에 동결하고 `2026-10-07_arin-quality-refinement.md`의 품질 개선을 진행한다. 커밋·푸시 없음.
