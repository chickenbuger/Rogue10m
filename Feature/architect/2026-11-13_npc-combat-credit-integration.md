# NPC 처치 귀속 소스 통합 패킷

격리 commit7c31084의 작은 NPC 소스 delta만 통합·정규 Editor 빌드·비GUI C++ 회귀로 검증한다. metadata 승인 JSON/clone metadata·이동·저장은 여전히 중단이며 이 작업은 그 허가나 우회가 아니다. 이 패킷 시작 당시 Computer Use는 금지였고 실제 GUI 입력은 0회다. 이후 사용자가 이번 작업의 Computer Use를 허용했으나 별도로 거절된 승인 기록과 clone QA의 중단은 유지한다.

| 단계 | 목표·변경 범위 | 완료 조건·검증 | 복구 경계 |
| --- | --- | --- | --- |
| Scope/Architect | 현재12대상·20관련해시, Work브랜치/외부프로세스, Units derivative와기존G08/VFX 보호 | 기존8 raw+normalized 일치, 신규4부재; combined12=runtime9+tests3; 패킷SHA3 일치 | 사용자변경·기존full30·다른Source를덮어쓰지 않음 |
| Implementer | combined12 한경로, existing8 byte백업+사전전체Source/Config/Scripts/Build/uproject manifest | git apply --check 후 한 번 적용,12 normalizedpostimage/나머지범위불변 | 복구가필요하면백업대상만비교검토; 사용자WIP/새외부변경 자동rollback금지 |
| Builder | 정규 Scripts/BuildEditor.ps1 -NoHotReload | UHT/Editor exit0,새DLL과로그별도evidence | 컴파일관련로컬문제만수정,생성캐시수동편집0 |
| Reviewer/Testing | 새UE8+기존NPC18,가능시 기존NativeD43을합친별도정확51선택 | 기존정상headlessNative경로,추가승인기록0,GUI/입력없이Source/Config·DLL전후검사;오류는분리기록 | metadata승인/guard완화가필요하면UE회귀중단,빌드/오프라인까지만 |
| Doc/Exit | 실제컴파일·회귀와미검증구분 | DevLog·Sprint·결과/evidence갱신 | commit/push/프로모션/새Windows패키지이번범위밖 |

기존 InternalWindows20261113E5_UNVERIFIED와 seal/input/manifest는 NPC수정 전 증거로 보존한다. 새Source/DLL을기존NativeD43·NPC18·E5결과에덮어쓰지 않는다. 새컴포넌트death 이력과creditreceipt 분리,정확attacker/RunId/generation/owner,중복·재진입·corpse뒤delayed전달을검토한다. portable202871은전달보고이며UE검증으로계산하지 않는다.

실제 Director 재생성/충돌·loot·외부 Blueprint listener 경제 보상·production GUID 전환,old live receipt hotreload이관은자동성공으로표시하지않는다. 새run/cold hook rebuild가필요하며3인칭/피격시취소/면역보류를유지한다.
