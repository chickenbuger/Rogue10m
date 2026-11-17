# Windows 실행 패키지 빌드

## 범위
Editor DLL 성공과 별도로 Win64 Game Target의 실제 실행파일·cook·stage·pak/IoStore·archive를 수행한다. 기존 VS2022/MSVC14.44·SDK22621을 확인하고 새 설치/업데이트는 자동 수행하지 않는다.

## 작업 단위
1. 현재 Source/Config/Content/Build metadata/Scripts를 독립 물리복사 D:/R10/Win1111A/Rogue10m으로 고정한다. 공유 콘텐츠 junction·기존 빌드출력/저장/캐시를 재사용하지 않는다. 원본 Work는 복사 이후 계속 가능하다.
2. PackageWindows.ps1에 Target/Win64/명시4맵/IoStore·fresh출력·SDK확인·실제결과기록을 추가한다. L_Menu/StartHub/OpenRunMap과 새 Hub 포털 목적지 FiveVillageGraybox를 cook한다. 실제 설정 문제만 최소 교정한다.
3. 독립 체크포인트에서 Game Development BuildCookRun. 실패로그·코드/참조/쿠킹 원인을 보존한다. 오류 무시·vendor 편집·공용 dirty 소스 덮어쓰기는 하지 않는다.
4. 패키지 exe를 격리 UserDir로 cold 실행하고 메뉴 PNG·로그·필수패키지/새맵 포함·실제 저장 불변을 검증한다. packaged smoke와 전체런/물리 입력을 구분한다.

## 되돌림
공유 변경은 PackageWindows.ps1의 최소패치와 필요시 확인된 빌드수정에만 한정한다. 이전 스크립트는 tmp/windows-packaging-20261111/baseline 보존. 독립 빌드 폴더는 증거로 남기며 삭제/기존출력 덮어쓰기·공개배포/푸시는 하지 않는다.


## 실제 실행에서 보완한 계획
첫 패키지의 검은 메뉴와 무기 누락을 실제 실행/IoStore로 발견했다. native runtime soft 참조의 좁은 15경로 cook 규칙과 exact42 자산 검사를 추가하고, C의 header 미리보기 재질 누락까지 실패로 보존한 뒤 D에서 재검증했다. 결과는 Feature/doc/2026-11-11_windows-executable-package.md 참조. 전체 런/전투 검증은 다음 단계다.
