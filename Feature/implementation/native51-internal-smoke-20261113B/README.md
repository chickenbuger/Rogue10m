# Native51 내부 Windows smoke derivative

**UNSEALED / NOT_EXECUTED** 준비물이다. 메인은 Compact의 NPC/DA/Director 콘텐츠를 편집 중이며 최종 입력을 아직 캡처·동결하지 않았다. 마지막 보고 맵 크기는1800m다. UI/시각/저장 routing/production NPC 동작은 UNVERIFIED이고 release_eligible=false다.

- `internal_smoke.py`, `Invoke-InternalSmoke.ps1`: 별도 Native51 schema와 소유 세션 release, 최종 콘텐츠 freeze·전체 입력·Editor6/DLL·보호 저장·출력6개·전후 해시를 요구한다.
- `native51_validation.py`, `native51-policy.json`: 실제 Native51B receipt 형식과 정확51개 ID, DLL/SourceConfig/build/launcher 연결,11개 경고 사례19events의 전체 wrapper·로그·순서와 전체 RED0를 검증한다.43→51 치환이나 경고 일괄 무시는 없다.
- `strict_primitives.py`: 실제 E5에 사용된 Cook-Fix-E의 바이트 동일 helper다. UBT Rebuild·cook/Windows·FOOBASEDIR 및 실제 cooker/Windows metadata 검증을 재사용한다.
- `content-freeze.UNSEALED.json`: 최종 콘텐츠 입력/맵 manifest와 DLL·배치 상태를 묶을 초안이다. 현재 배치 상태를 과거 NPC 미배치로 고정하지 않는다.
- `OWNER-RUNBOOK.md`: 새 전체입력 capture→최종 freeze→별도 owner release→정규 빌드 순서.
- `results/retained-readonly-audit-final.json`: NativeD 전체RED0, 기존E5 성공·보존 metadata와 Native51B51/51·경고19·현재 Source/Config368/DLL 일치의 읽기 전용 검증.
- `results/Final01/validation-summary.json`: 이번 실제 오프라인 검증의 최종 결과.

기존 strict/B/B2/E5/이전 NonGuiSmoke는 수정하지 않았다. E5는50파일/1,948,283,459B인 기존 내부 artifact이며 **이번 NPC 처치귀속 수정을 포함하지 않는다**. 새 출력과 capture가 보존 사본·원본 임시 archive에 겹치지 않도록 검사한다. E5 payload 전체를 다시 읽는 대신 기존 delivery/postflight/IoStore manifest pins를 확인했고 새 출력 경계를 강화했다.

과거 NativeA startup15 미승인 기록은 현재 NativeD/Native51B에 적용하지 않는다. 두 새 원문 로그는 RED0이며 Native51 검증기는 startup_review를 받지 않는다. 별도로 자동 심사에서 거절된 root-startup-approval-A.json 및 metadata/clone QA는 계속 승인 대기이며 이 패킷에서 생성·재시도·우회하지 않는다.

이 Work는 코드·오프라인 파일 검사만 했다. UE/UBT/UAT/Cook/게임·GUI·승인 작업·공유 수정은0회다. 실행 가능한 새 실제 package나 release/UI 검증을 제공한 것으로 해석하면 안 된다.
