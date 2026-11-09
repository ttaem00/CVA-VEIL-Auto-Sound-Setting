# 설치 창

Windows 배포 ZIP 전체를 한 폴더에 풀고 **CVA-VEIL-Setup.exe**를 열면 설치 창이 나옵니다.
한국어가 기본이며 오른쪽 위에서 English 또는 日本語를 고를 수 있습니다.

1. 설치 파일을 확인합니다.
2. OBS가 열려 있으면 방송·녹화를 마친 뒤 직접 닫고 **다시 확인**을 누릅니다.
3. **설치하기**를 누릅니다. Windows 권한이 필요한 경우 **권한 허용하고 설치** 버튼이 나옵니다.
4. 완료 후 **OBS 열기**를 누르고 CVA-VEIL 창의 **채널 설정**에서 소리를 연결합니다.

설치 창은 OBS를 강제로 종료하거나 방송·녹화를 시작하지 않습니다.
설치 언어와 플러그인 안의 표시 언어는 각각 고를 수 있습니다. 플러그인의 첫 표시 언어도 한국어입니다.
Windows 전용 설치 창이며 Windows PowerShell 5.1과 WPF를 사용합니다.
`CVA-VEIL-Setup.cmd`는 같은 설치 창을 여는 보조 진입점입니다.

## 파일 구성과 변경 범위

배포 폴더에는 다음 설치 진입점이 함께 있어야 합니다.

- `CVA-VEIL-Setup.cmd`
- `CVA-VEIL-Setup.exe`
- `CVA-VEIL-Setup.ps1`
- `CVA-VEIL-Install.ps1` — 소스의 `TRUEITEM-Install.ps1`을 이 이름으로 제공
- `CVA-VEIL-InstallCore.ps1`
- `package-manifest.json` 및 `loudness-balance-monitor/` 설치 파일

설치 대상은 기본적으로 `%PROGRAMDATA%/obs-studio/plugins/loudness-balance-monitor`입니다.
DLL, OBS용 언어 파일 3개, `data/i18n.json`만 설치합니다. 기존 소스·음량·필터·녹화 배정은 변경하지 않습니다.

이전 파일은 설치 전 `%LOCALAPPDATA%/CVA-VEIL/InstallerBackups`에 보관합니다.
설치 중 오류가 나면 교체한 파일을 되돌리고, 새로 추가한 파일은 제거합니다. 복원을 끝내지 못하면 성공으로 표시하지 않고 백업 위치를 안내합니다.
압축을 푼 폴더에는 백업을 쓰지 않습니다. 설치·실패 내용과 위치는 **설치 위치와 세부 정보**에서 확인합니다.

## 명령줄 미리보기와 개발 검증

`CVA-VEIL-Install.ps1`의 기본 실행은 파일 검증과 미리보기입니다. `-Apply`를 명시해야 설치합니다.
`-Destination`은 포터블 OBS처럼 별도 플러그인 위치가 필요한 경우 사용할 수 있으며 마지막 폴더 이름은 `loudness-balance-monitor`여야 합니다.
`CVA-VEIL-Setup.ps1 -PreviewOnly`는 설치를 실행할 수 없는 UI 확인용 진입점입니다.

공유 엔진은 파일 해시를 확인한 뒤 같은 볼륨에 교체 파일을 준비하고, 원본 보관을 검증한 후 파일별 원자 교체와 재검증을 수행합니다.
다른 CVA-VEIL 설치와의 동시 실행을 막으며, OBS가 실행 중이면 적용을 차단합니다.

격리 검사는 소스 루트에서 실행할 수 있습니다.

```powershell
./build-aux/tests/installer-core.tests.ps1
```

합성 파일만 사용하며 설치된 OBS나 개인 설정을 수정하지 않습니다. 미리보기·OBS 잠김·해시 불일치·정상 설치·중간 파일 교체 실패 후 복원·새 파일 제거·기존 관리 외 파일 보존·임시 폴더 정리를 검사합니다.
PowerShell 5.1과 7에서 실행했습니다. 이 검사는 실제 사용자 권한 요청(UAC)의 허용·취소 조작이나 모든 환경의 GUI 표시 검증을 대신하지 않습니다.

`Build-SetupHost.ps1`은 Windows에 있는 .NET Framework C# 컴파일러로 작은 WinExe 호스트를 만듭니다. `SetupHost.cs`는 별도 UI 사본 대신 동일 WPF 스크립트를 현재 STA 스레드의 PowerShell runspace에서 실행합니다. 결과 EXE는 `work/installer-host`에 만들어지며, 이 EXE와 위 스크립트를 같은 배포 폴더에 제공합니다. 실행 정책 선택은 설치 프로세스 안에만 적용되고 시스템·사용자의 정책을 바꾸지 않습니다.
