# VEIL Auto Sound Setting

**OBS에서 생방송과 PC 녹화에 담을 소리를 따로 고르세요.**

예를 들어 시청자에게는 음악까지 들려주고, PC에 남는 녹화에는 목소리와 게임 소리만 담을 수 있습니다.

**[Windows 설치 ZIP 받기](https://github.com/ttaem00/CVA-VEIL-Auto-Sound-Setting/releases/download/v1.3.0/CVA-VEIL-Auto-Sound-Setting-1.3.0-windows-x64.zip)** · [1.3.0 미리보기 안내](https://github.com/ttaem00/CVA-VEIL-Auto-Sound-Setting/releases/tag/v1.3.0)

Windows 64비트 · **OBS Studio 33.0.1**에서 확인했습니다. 설치하기 전에 OBS 버전을 확인해 주세요.

## 설치하고 첫 녹화까지

1. **설치하기** — ZIP의 모든 파일을 한 폴더에 풀고 OBS를 종료한 뒤 `CVA-VEIL-Setup.exe`를 실행하고 **설치하기**를 누릅니다.
2. **소리 고르기** — OBS를 다시 열고 **독 → CVA-VEIL-Auto Sound Setting → 채널 설정**으로 들어갑니다. 사용할 마이크·게임·음악을 연결하고 **다음**을 누릅니다. 각 소리를 **생방송**과 **녹화 영상** 중 어디에 담을지 체크합니다.
3. **확인하고 적용하기** — **바뀔 내용 보기 → 이 설정 적용**을 누릅니다. 짧게 녹화한 뒤 원하는 소리만 들어갔는지 확인하세요.

## 목소리가 묻힐 때

**사용할 소리를 평소 크기로 준비했어요**를 체크하고 **소리 검사 시작**을 누르세요. 평소처럼 25초 말한 뒤 5초간 조용히 기다리세요. 결과에서 목소리·게임·음악의 권장 음량 조정을 볼 수 있습니다. **추천 설정 적용**을 눌러야 음량이 바뀝니다.

## 설치 전에 알아둘 점

- 설치 파일과 플러그인에 디지털 서명이 없습니다. 설치기는 OBS 버전을 자동으로 확인하지 않습니다.
- 기존 **Loudness Balance Monitor**가 설치되어 있으면 이 버전이 그 자리에 설치됩니다. 설치 창의 **설치 위치와 세부 정보**에서 이전 파일의 백업 위치를 확인할 수 있습니다.
- 한 입력에 이미 섞인 소리는 나눌 수 없습니다. **녹화 영상** 설정은 PC에 저장하는 녹화에 적용되며, 방송 사이트의 다시보기 설정은 별도입니다.

[자세한 사용법](docs/TRUEITEM-사용안내.md) · [문제 신고](https://github.com/ttaem00/CVA-VEIL-Auto-Sound-Setting/issues/new)

## 소스와 라이선스

[AllegroMoltoV의 Loudness Balance Monitor v1.0.2](https://github.com/AllegroMoltoV/obs-loudness-balance-monitor/releases/tag/v1.0.2)를 바탕으로 한 GPL-2.0 수정판입니다. 배포판과 대응하는 소스 ZIP과 SHA-256은 [릴리스](https://github.com/ttaem00/CVA-VEIL-Auto-Sound-Setting/releases/tag/v1.3.0)에서 받을 수 있습니다.

[수정 내역](MODIFICATIONS.md) · [빌드 방법](tools/README.md) · [검증 범위](docs/design/ADVERSARIAL-1.3.md) · [라이선스](LICENSE) · [원작 및 제3자 고지](THIRD-PARTY-NOTICES.md)
