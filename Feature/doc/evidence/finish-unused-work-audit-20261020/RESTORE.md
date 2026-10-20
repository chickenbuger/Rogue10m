# 추가 정리4개 복구

프로젝트 루트에서 실행한다. 확정 원본4개는 Git 비추적 tmp 출력이며 동일 내용의 보존 파일3개는 Git에 추적된 기능 검증 기록이다.

```powershell
.\.venv\Scripts\python.exe Feature\doc\evidence\finish-unused-work-audit-20261020\manage-selected.py --mode verify
.\.venv\Scripts\python.exe Feature\doc\evidence\finish-unused-work-audit-20261020\manage-selected.py --mode restore
```

`restore`는 selection.json의 keep을 원래 위치에 exclusive create로 복사하고 크기·SHA-256을 확인한다. 기존 파일이 다르면 거절하며 덮어쓰지 않는다. 실제4개 복구 후 전량 해시 확인을 완료했다. 다시 정리하려면 `--mode apply`와 `--mode verify`를 순서대로 실행한다. 미리 보려면 원본4개가 존재하는 상태에서 `--mode preview`를 사용한다.

이 도구는 이전20261020 개별 파일 정리기의 검증된 경로/해시·복구 처리를 재사용하며 다른 후보229개를 자동 삭제하지 않는다. 재실행 시에도 원본이 Git 추적이면 거부하고 보존 파일의 Git 추적 여부를 확인한다. 중단으로 복사본이 일부만 남으면 검증된 canonical 보존본을 유지하고 해당 부분 출력만 해시 확인 후 정리한다.

원본11947바이트는 과거 빌드 stdout2개와 같은 감사 응답 캐시2개다. 현재 검증·재생성 도구 입력과 고유 자료, 이전87개 보존 결정, checkpoint242개, 실제참조15개 등은 삭제하지 않았다.
