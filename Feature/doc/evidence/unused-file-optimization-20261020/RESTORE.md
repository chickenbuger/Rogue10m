# 정리 파일 복구

프로젝트 루트 `D:\Project\Rogue10m`에서 실행한다. 도구는 manifest에 있는 개별 파일만 처리하며 폴더나 Git 이력을 삭제하지 않는다.

```powershell
.\.venv\Scripts\python.exe Feature\doc\evidence\unused-file-optimization-20261020\optimize.py --mode verify
.\.venv\Scripts\python.exe Feature\doc\evidence\unused-file-optimization-20261020\optimize.py --mode restore
```

- `verify`: 정리된 원본7개 부재, 보존 사본4개와 gzip3개의 해제 bytes/SHA-256을 전량 검사한다. 압축본 자체 해시도 receipt에 기록한다.
- `restore`: Blender4개는 selection.json의 keep에서 복사하고, JSON3개는 `.json.gz`에서 원래 경로로 압축 해제한다. 원본 경로가 존재하면 동일 해시일 때만 유지하며 다른 파일을 덮어쓰지 않는다. 보존 사본과 gzip은 그대로 남는다.
- 다시 최적화하려면 `--mode apply`를 실행하고 `--mode verify`로 확인한다. 각 원본은 삭제 직전 다시 해시 검사한다.

전체7개의 실제 복구와 재정리를 수행했다. `restore-receipt.json`은 모든 원본이 다시 생성되고 크기·SHA-256이 일치한 증거다. `verify-receipt.json`은 마지막 최적화 상태를 확인한다.

압축 자료를 읽을 때는 Python 표준 `gzip.open(path, 'rt', encoding='utf-8')` 또는 압축 해제 도구를 사용한다. `EnhanceFighterArmSwing.py`의 기존 `read(name)`은 일반 JSON이 없으면 같은 이름의 `.gz`를 자동으로 읽는다. baseline 존재 가드는 두 형식 모두 확인해 기존 기준선을 보존한다. `write()`는 기존 동작을 유지하므로 나중에 검증기를 실행해 새 일반 JSON이 생기면 그 파일이 우선이다. 동시 존재 파일은 새로운 증거인지 검토하고 예전 selection 해시가 다르면 이 정리 도구로 제거하지 않는다.

프로세스가 중단돼 `.gz`나 복구 JSON이 일부만 기록되면 도구는 해시 불일치로 중단한다. 검증된 원본/압축본을 먼저 확인하고 selection의 해당 항목에서 해시가 다른 **부분 출력만** 개별 정리한 후 재시도한다. 유효한 원본이나 archive를 함께 삭제하지 않는다. Git 추적 원본은 기준 `77a52b6a0276ae9fc471ee70fc85972df819f268`에도 남아 있다.

이번 정리는 현재 체크아웃 파일의 용량을 줄인다. 과거 Git 객체는 복구를 위해 유지하므로 `.git` 크기와 최초 clone 전송량이 같은 양만큼 줄어드는 것은 아니다.
