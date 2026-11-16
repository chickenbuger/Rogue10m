# 저장소 파일 전체 현황 감사

2026-11-17 한국시간. 기준 원격 추적 ref는 `origin/main`, 커밋은 `6bfc78c6fb38049aeb384823792f146c17f4ca3d`다. 원래 작업 폴더는 `Sprint#9-32-landscape-world-blockout`, HEAD `f1574a3e1f24a9bc1e7fb31163ea153df06b9cda`다.

## 17,269개와 실제 GitHub 파일의 구분

이번 스냅샷의 GitHub main은 **11,963파일·7,570,307,054바이트(약7.57GB)**다. 원래 작업 폴더의 변경 목록은 **17,271항목 = 수정74 + 미추적17,197**이다. 사용자가 본17,269와 현재 값은2개 차이가 있지만 당시 스냅샷이 없어 그 두 파일을 특정하지 않는다. GitHub 전체 파일 수와 로컬 변경 목록 수는 다른 값이다.

원래 폴더의 추적 경로는12,155개이며 미추적을 합치면29,352개다. 이는 ignored 파일을 제외한 Git 목록이며 디스크 전체 파일 수가 아니다. 수집 전후 변경 목록에 이 감사가 만든 파일 이외의 변화가 없었다. 아래 수치는 감사 산출물 생성 전 기준이다.

수집은 `git ls-tree -r -l -z <고정커밋>`, `git ls-files --stage -z`, `git ls-files --others --exclude-standard -z`, `git status --porcelain=v1 --untracked-files=all -z`를 사용했다. 크기는 Git blob 또는 로컬 파일의 비압축 바이트이며 clone 전송량·Git 전체 이력 크기와 다르다.

## GitHub main 구성

| 경로 | 파일 수 | 크기 MB |
| --- | ---: | ---: |
| Content | 6,423 | 4,480.71 |
| Feature | 4,693 | 2,354.98 |
| Art | 79 | 720.89 |
| Source | 234 | 2.82 |
| ui-concepts | 4 | 2.54 |
| Scripts | 265 | 2.24 |
| tmp | 5 | 2.22 |
| SourceArt | 6 | 1.63 |
| Docs | 174 | 1.41 |
| DevLog | 55 | 0.66 |

상위 확장자는 `.uasset`6,330개/4,356.15MB, `.png`873개/1,129.39MB, `.blend`17개/448.45MB, `.fbx`25개/361.68MB, `.json`2,257개/228.54MB다. 전체 확장자별 집계는 JSON에 있다.

## 로컬 미추적17,197개

총2,730,327,349바이트(약2.73GB)다.

| 경로 | 파일 수 | 크기 MB |
| --- | ---: | ---: |
| DevLog | 1,519 | 1,501.60 |
| Feature | 10,870 | 1,121.67 |
| Content | 4,526 | 103.84 |
| Scripts | 170 | 1.68 |
| Source | 87 | 1.17 |
| Docs | 9 | 0.23 |
| Tools | 15 | 0.13 |
| Art | 1 | 0.00 |

미추적 `.uasset`은8,795개다. `Content/` 밖의 4,273개는 백업·보존 증거 등에 있으며, 모두 신규 런타임 자산이라고 해석할 수 없다. 실제 `Content/` 아래 전체 미추적 파일은4,526개다. PNG1,318개/1,644.46MB와 JSON4,146개/267.85MB도 포함돼 있으므로 변경 개수만으로 구현 규모를 판단하지 않는다.

## GitHub에서 큰 파일10개

| 경로 | 바이트 |
| --- | ---: |
| `Content/Stylized_Village/Overview_Map/Maps/Overview_Map.umap` | 81,342,985 |
| `Content/Stylized_Village/Assets/_Global/Materials/_UtilityTextures/HDRI_Epic_Courtyard_Daylight.uasset` | 66,844,567 |
| `Content/StarterContent/HDRI/HDRI_Epic_Courtyard_Daylight.uasset` | 66,790,681 |
| `Art/NPC/Arin/Animation/Arin_Motions.blend` | 60,822,253 |
| `Art/NPC/Arin/Animation/Arin_Motions.blend1` | 60,822,253 |
| `Feature/doc/evidence/arin-reference-refinement-20261009/third-build/Arin.blend` | 59,134,638 |
| `Art/NPC/Arin/ReferenceRefined/Arin.blend1` | 59,033,348 |
| `Art/NPC/Arin/ReferenceRefined/Arin.blend` | 58,659,459 |
| `Feature/doc/evidence/arin-reference-refinement-20261009/first-build/Arin.blend` | 58,652,313 |
| `Content/Rogue10m/Characters/Arin/RefinedV3/SK_Arin.uasset` | 53,269,287 |

## 생성물·백업·자산 분류

- 미추적 PDB4개는442,048,512바이트다. 모두 NPC 처치 귀속의 `before-editor-products` 두 폴더에 있고, 원래 파일을 보존하는 해시 manifest가 있다. 실행에 필요한 원본 콘텐츠가 아니라 재생성 가능한 디버그 심볼이다.
- GitHub에는 Blender 자동 백업 `.blend1`7개/202,023,864바이트가 있다. 대응 `.blend`와 실제 동일한지, 복구에 필요한 버전인지 확인한 뒤 별도 정리 여부를 결정해야 한다. 확장자만으로 삭제하지 않는다.
- 이미 추적된 Python bytecode는1개/2,227바이트다. 현재 ignore 규칙이 있어도 과거 추적 파일에는 소급 적용되지 않는다.
- `.sav`27개/3,788,717바이트는 모두 QA 폴더의 손상 주입 fixture로 분류했다. town-services 실패2개와 trait16의 첫 회차4개·다음 회차21개이며, 두 런타임 테스트 소스와 당시 리뷰에서 생성 목적을 확인했다. 실제 사용자 저장이라는 근거는 없다. 파일 내용은 열지 않았으므로 내부 민감정보 부재까지 인증하지 않는다. 이미 공개 main에 포함된 사실만 기록하며 삭제·이력 재작성은 하지 않았다.
- 벤더·엔진 팩 이름 경로는3,500개/4,001.28MB다. 경로 기준 후보 분류이며 현재 Fab 소유권·사용 여부·패키징 필요성을 인증하지 않는다.
- World Partition 외부 액터·외부 오브젝트는2,082개/27.43MB다. 맵의 저작 데이터이므로 캐시로 분류하면 안 된다.
- PNG·동영상·로그·JSON은 실제 검증 증거일 수 있다. 생성됐다는 이유만으로 전부 제외하지 않고 기능별로 원문/요약/실패 후보를 구분한다.

## 최소 ignore 제안

이번에는 `.gitignore`를 수정하지 않았다. 기존 PDB를 물리적으로 남기고 이미 있는 해시 JSON을 유지하면서, 다음 두 범위만 제외할 수 있다.

```gitignore
/Feature/doc/evidence/npc-combat-credit-20261113A/before-editor-products/*.pdb
/Feature/doc/evidence/npc-combat-credit-20261113A/FixtureDerivativeB/before-editor-products/*.pdb
```

이 규칙은 현재 미추적 PDB4개만 겨냥하며 PNG와 해시 JSON을 숨기지 않는다. `.blend1`, 저장 파일, 벤더 팩의 제거 또는 Git 이력 재작성은 이번에 수행하지 않았다.

## 전체 목록과 한계

- 고정 원격 전체 목록: `github-main-files.csv` — 원래 작업 폴더에만 보존, 이번 게시에 포함하지 않음.
- 로컬 추적 목록: `local-tracked-files.csv` — 원래 작업 폴더에만 보존, 이번 게시에 포함하지 않음.
- 로컬 미추적 목록: `local-untracked-files.csv` — 원래 작업 폴더에만 보존, 이번 게시에 포함하지 않음.
- [집계·SHA·시각·중복 blob·수집 중 변화](evidence/repository-file-audit-20261117/summary.json)

CSV는 모든 경로를 포함하고 크기·확장자·분류를 기록한다. 내용 자체나 Unreal 의존성 그래프를 전수 검증한 결과는 아니다. 실제 기능 게시 여부는 별도의 컴파일·자동 검사·게임 검증을 거쳐 판정한다. 파일 삭제·이동·Git index 변경·커밋·푸시는 이 감사에서 하지 않았다.

CSV 세 파일의 행 수·크기 합계·경로 중복을 확인했고 JSON 집계와 모두 일치했다. SHA256은 `summary.json`의 `artifact_checks`에 기록했다.

## 감사 뒤 적용한 제한된 보호

감사 시점의 미추적 PDB4개를 정확히 겨냥하는 위 두 ignore 규칙만 추가했다. 물리 파일과 SHA manifest는 보존했고 파일 삭제·이동·Git 이력 재작성은 없다. 원본/게시 사본의 규칙을 적용했으며 PNG·로그·JSON과 Unreal 외부 액터는 제외하지 않았다. 상세 CSV는 로컬에만 보존한다.
