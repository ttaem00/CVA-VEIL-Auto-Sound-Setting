# CVA VEIL modification, 2026-11-09. GPL-2.0; see LICENSE.
[CmdletBinding()]
param(
    [ValidateSet('ko-KR','en-US','ja-JP')][string]$Language='ko-KR',
    [string]$PackageRoot='',
    [string]$Destination=(Join-Path $env:PROGRAMDATA 'obs-studio\plugins\loudness-balance-monitor'),
    [switch]$PreviewOnly
)
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($PackageRoot)) { $PackageRoot=$PSScriptRoot }
. (Join-Path $PSScriptRoot 'CVA-VEIL-InstallCore.ps1')
Add-Type -AssemblyName PresentationFramework,PresentationCore,WindowsBase
$script:ui = @{ Language=$Language; Phase='Checking'; Action='Retry'; Detail=''; Worker=$null; ResultPath=$null; BackupPath=$null; Elevated=$false }
$script:copy = @{
    'ko-KR'=@{
        Title='CVA-VEIL-Auto Sound Setting 설치'; Product='CVA · VEIL'; Steps='1 파일 확인     2 설치     3 OBS에서 시작';
        Intro='방송 소리 설정을 시작해요'; Subtitle='설치한 뒤 목소리·게임·음악을 쉽게 나눌 수 있어요.';
        Checking='설치 파일을 확인하고 있어요'; CheckingBody='잠시만 기다려 주세요.';
        Ready='설치할 준비가 됐어요'; ReadyBody='기존 플러그인을 보관한 뒤 새 버전을 설치해요. OBS의 소리 설정은 그대로 유지돼요.';
        ObsRunning='먼저 OBS를 닫아 주세요'; ObsRunningBody='방송과 녹화를 끝내고 OBS를 완전히 닫은 뒤 다시 확인해 주세요.';
        Installing='설치하고 있어요'; InstallingBody='파일을 확인하고 이전 버전을 안전하게 보관해요.';
        Done='설치가 끝났어요'; DoneBody='OBS를 열고 CVA-VEIL 창의 「채널 설정」에서 사용할 소리를 골라 주세요.';
        Permission='Windows 권한이 필요해요'; PermissionBody='아래 버튼을 누르고 Windows의 권한 요청을 허용해 주세요.';
        PackageMissing='설치 파일을 찾지 못했어요'; PackageMissingBody='압축 파일 전체를 새 폴더에 푼 뒤 「설치 시작」을 다시 열어 주세요.';
        PackageInvalid='설치 파일을 다시 확인해 주세요'; PackageInvalidBody='일부 파일이 없거나 바뀌었어요. 압축 파일을 새 폴더에 다시 풀어 주세요.';
        Failed='설치하지 못했어요'; FailedBody='세부 정보에서 원인을 확인한 뒤 다시 시도해 주세요.';
        Restored='설치가 멈춰 이전 버전으로 돌렸어요'; RestoredBody='기존 플러그인을 복원했어요. 세부 정보를 확인한 뒤 다시 시도해 주세요.';
        RollbackFailed='이전 버전 복원을 확인해 주세요'; RollbackFailedBody='자동 복원을 마치지 못했어요. 세부 정보의 백업 위치에서 이전 파일을 확인해 주세요.';
        Preview='설치 화면 미리보기'; PreviewBody='설치 파일 확인만 했어요. 이 화면에서는 파일을 바꾸지 않아요.';
        Install='설치하기'; Retry='다시 확인'; Grant='권한 허용하고 설치'; OpenObs='OBS 열기'; Close='닫기'; Details='설치 위치와 세부 정보'; Location='설치 위치'; Backup='이전 버전 보관 위치'; Help='설치 후: OBS → CVA-VEIL 창 → 채널 설정'; Language='표시 언어';
        UacCancelled='권한 요청을 취소했어요. 준비되면 다시 설치할 수 있어요.'; ObsMissing='OBS를 찾지 못했어요. 평소 사용하는 방법으로 OBS를 열어 주세요.';
        Busy='다른 설치가 진행 중이에요'; BusyBody='다른 설치 창이 끝난 뒤 다시 확인해 주세요.'
    };
    'en-US'=@{
        Title='Install CVA-VEIL-Auto Sound Setting'; Product='CVA · VEIL'; Steps='1 Check files     2 Install     3 Start in OBS';
        Intro='Set up your stream sound'; Subtitle='Connect your voice, game and music after installation.';
        Checking='Checking the installation files'; CheckingBody='This will only take a moment.';
        Ready='Ready to install'; ReadyBody='We will keep a copy of the previous plugin and install this version. Your OBS sound settings stay as they are.';
        ObsRunning='Close OBS first'; ObsRunningBody='Finish your stream or recording and close OBS completely, then check again.';
        Installing='Installing'; InstallingBody='Checking files and keeping the previous version safe.';
        Done='Installation complete'; DoneBody='Open OBS and choose Channel setup in the CVA-VEIL window to connect your sounds.';
        Permission='Windows permission is needed'; PermissionBody='Use the button below and allow the Windows permission request.';
        PackageMissing='Installation files are missing'; PackageMissingBody='Extract the entire ZIP to a new folder, then open the installer again.';
        PackageInvalid='Check the installation files'; PackageInvalidBody='Some files are missing or have changed. Extract the ZIP to a new folder and try again.';
        Failed='Installation did not finish'; FailedBody='Check the details, then try again.';
        Restored='The previous version was restored'; RestoredBody='Installation stopped and the previous plugin was restored. Check the details before trying again.';
        RollbackFailed='Check the previous version backup'; RollbackFailedBody='Automatic restore did not finish. Use the backup location in the details to recover the previous files.';
        Preview='Installer preview'; PreviewBody='The files were checked. This preview does not change any files.';
        Install='Install'; Retry='Check again'; Grant='Allow permission and install'; OpenObs='Open OBS'; Close='Close'; Details='Location and details'; Location='Install location'; Backup='Previous version backup'; Help='After installing: OBS → CVA-VEIL → Channel setup'; Language='Display language';
        UacCancelled='The permission request was cancelled. You can try again when ready.'; ObsMissing='OBS was not found. Open it the way you usually do.';
        Busy='Another installation is running'; BusyBody='Wait for the other installer to finish, then check again.'
    };
    'ja-JP'=@{
        Title='CVA-VEIL-Auto Sound Setting のインストール'; Product='CVA · VEIL'; Steps='1 ファイル確認     2 インストール     3 OBSで開始';
        Intro='配信の音を設定しましょう'; Subtitle='インストール後に声・ゲーム・音楽を分けられます。';
        Checking='インストールファイルを確認しています'; CheckingBody='少しお待ちください。';
        Ready='インストールの準備ができました'; ReadyBody='以前のプラグインを保管してから更新します。OBSの音声設定はそのままです。';
        ObsRunning='先にOBSを閉じてください'; ObsRunningBody='配信や録画を終え、OBSを完全に閉じてから再確認してください。';
        Installing='インストールしています'; InstallingBody='ファイルを確認し、以前のバージョンを保管しています。';
        Done='インストールが完了しました'; DoneBody='OBSを開き、CVA-VEILの「チャンネル設定」で使う音を選んでください。';
        Permission='Windowsの許可が必要です'; PermissionBody='下のボタンを押し、Windowsの許可を求める画面で許可してください。';
        PackageMissing='インストールファイルが見つかりません'; PackageMissingBody='ZIP全体を新しいフォルダーに展開し、インストーラーを開き直してください。';
        PackageInvalid='インストールファイルを確認してください'; PackageInvalidBody='一部のファイルがないか、変更されています。ZIPを新しいフォルダーに展開してください。';
        Failed='インストールできませんでした'; FailedBody='詳細を確認してから、もう一度お試しください。';
        Restored='以前のバージョンに戻しました'; RestoredBody='インストールを中断し、以前のプラグインを復元しました。詳細を確認してください。';
        RollbackFailed='以前のバージョンの復元を確認してください'; RollbackFailedBody='自動復元が完了しませんでした。詳細のバックアップ先から以前のファイルを確認してください。';
        Preview='インストーラーのプレビュー'; PreviewBody='ファイルを確認しました。この画面ではファイルを変更しません。';
        Install='インストール'; Retry='再確認'; Grant='許可してインストール'; OpenObs='OBSを開く'; Close='閉じる'; Details='場所と詳細'; Location='インストール先'; Backup='以前のバージョンの保存先'; Help='インストール後：OBS → CVA-VEIL → チャンネル設定'; Language='表示言語';
        UacCancelled='許可の要求をキャンセルしました。準備ができたらもう一度お試しください。'; ObsMissing='OBSが見つかりません。いつもの方法でOBSを開いてください。';
        Busy='別のインストールが進行中です'; BusyBody='別のインストールが終わってから再確認してください。'
    }
}
function T([string]$Key) {
    $table=$script:copy[$script:ui.Language]
    if ($table.ContainsKey($Key)) { return [string]$table[$Key] }
    return [string]$script:copy['ko-KR'][$Key]
}
[xml]$xaml=@'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" Width="610" Height="640" MinWidth="500" MinHeight="520" WindowStartupLocation="CenterScreen" Background="#15191e" Foreground="#f1f4f6" FontFamily="Segoe UI" FontSize="14">
 <Window.Resources>
  <Style TargetType="TextBlock"><Setter Property="TextWrapping" Value="Wrap"/></Style>
  <Style TargetType="Button"><Setter Property="MinHeight" Value="46"/><Setter Property="Padding" Value="18,8"/><Setter Property="FontSize" Value="14"/><Setter Property="Background" Value="#29323d"/><Setter Property="Foreground" Value="#f1f4f6"/><Setter Property="BorderBrush" Value="#485564"/><Setter Property="BorderThickness" Value="1"/><Setter Property="Cursor" Value="Hand"/></Style>
 </Window.Resources>
 <Grid Margin="28">
  <Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="*"/><RowDefinition Height="Auto"/></Grid.RowDefinitions>
  <DockPanel Grid.Row="0" Margin="0,0,0,28">
   <TextBlock x:Name="Brand" Text="CVA · VEIL" Foreground="#b0bac5" FontSize="12" FontWeight="SemiBold" VerticalAlignment="Center"/>
   <ComboBox x:Name="LanguageBox" DockPanel.Dock="Right" HorizontalAlignment="Right" Width="132" Height="36" Foreground="#102a23" Background="#eef4f5">
    <ComboBoxItem Tag="ko-KR" Content="한국어"/><ComboBoxItem Tag="en-US" Content="English"/><ComboBoxItem Tag="ja-JP" Content="日本語"/>
   </ComboBox>
  </DockPanel>
  <ScrollViewer Grid.Row="1" VerticalScrollBarVisibility="Auto" HorizontalScrollBarVisibility="Disabled">
   <StackPanel>
    <TextBlock x:Name="Steps" Foreground="#b0bac5" FontSize="12" Margin="0,0,0,20"/>
    <TextBlock x:Name="Intro" FontSize="27" FontWeight="SemiBold" Margin="0,0,0,10"/>
    <TextBlock x:Name="Subtitle" Foreground="#b0bac5" Margin="0,0,0,26"/>
    <Border Background="#20262e" BorderBrush="#343e49" BorderThickness="1" CornerRadius="12" Padding="22" Margin="0,0,0,18">
     <StackPanel>
      <TextBlock x:Name="StatusTitle" FontSize="21" FontWeight="SemiBold" Margin="0,0,0,12"/>
      <TextBlock x:Name="StatusBody" Foreground="#c1ccd7" LineHeight="23"/>
      <ProgressBar x:Name="Progress" Height="5" Margin="0,20,0,0" IsIndeterminate="True" Visibility="Collapsed" Foreground="#98ead1"/>
     </StackPanel>
    </Border>
    <Expander x:Name="Details" Foreground="#b0bac5" Margin="0,0,0,20">
     <TextBlock x:Name="DetailText" FontSize="12" Foreground="#b0bac5" Margin="0,12,0,0"/>
    </Expander>
   </StackPanel>
  </ScrollViewer>
  <StackPanel Grid.Row="2" Margin="0,18,0,0">
   <TextBlock x:Name="Help" Foreground="#b0bac5" FontSize="12" Margin="0,0,0,14"/>
   <Grid>
    <Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="12"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
    <Button x:Name="Primary" Grid.Column="0" Background="#98ead1" Foreground="#102a23" BorderBrush="#98ead1" FontWeight="SemiBold" IsDefault="True"/>
    <Button x:Name="CloseButton" Grid.Column="2" MinWidth="84"/>
   </Grid>
  </StackPanel>
 </Grid>
</Window>
'@
$reader=New-Object Xml.XmlNodeReader($xaml)
$script:window=[Windows.Markup.XamlReader]::Load($reader)
$script:controls=@{}
foreach($name in @('Brand','LanguageBox','Steps','Intro','Subtitle','StatusTitle','StatusBody','Progress','Details','DetailText','Help','Primary','CloseButton')) { $script:controls[$name]=$script:window.FindName($name) }
function Update-CvaInstallerUi {
    $script:window.Title=T 'Title'
    foreach($name in @('Steps','Intro','Subtitle','Help')) { $script:controls[$name].Text=T $name }
    $script:controls.CloseButton.Content=T 'Close'
    $script:controls.Details.Header=T 'Details'
    $script:controls.StatusTitle.Text=T $script:ui.Phase
    $script:controls.StatusBody.Text=T ($script:ui.Phase+'Body')
    if ($script:ui.CustomBody) { $script:controls.StatusBody.Text=T $script:ui.CustomBody }
    $script:controls.Primary.Content=T $script:ui.Action
    $busy=$null -ne $script:ui.Worker
    $script:controls.Primary.IsEnabled=-not $busy
    $script:controls.CloseButton.IsEnabled=-not $busy
    $script:controls.LanguageBox.IsEnabled=-not $busy
    $script:controls.Progress.Visibility=$(if($busy){'Visible'}else{'Collapsed'})
    $lines=@((T 'Location'),$Destination)
    if ($script:ui.BackupPath) { $lines+=@('',(T 'Backup'),$script:ui.BackupPath) }
    if ($script:ui.Detail) { $lines+=@('',$script:ui.Detail) }
    $script:controls.DetailText.Text=$lines -join [Environment]::NewLine
    [Windows.Automation.AutomationProperties]::SetName($script:controls.LanguageBox,(T 'Language'))
}
function Set-CvaFailure([string]$Code,[string]$Detail,[string]$Rollback='') {
    $script:ui.CustomBody=$null
    $script:ui.Detail=$Detail
    $script:ui.Action='Retry'
    switch($Code) {
        'ObsRunning' { $script:ui.Phase='ObsRunning' }
        'Permission' { $script:ui.Phase='Permission'; $script:ui.Action='Grant' }
        'PackageMissing' { $script:ui.Phase='PackageMissing' }
        {$_ -in 'PackageInvalid','PackageChanged'} { $script:ui.Phase='PackageInvalid' }
        'RollbackFailed' { $script:ui.Phase='RollbackFailed' }
        'Busy' { $script:ui.Phase='Busy' }
        default { $script:ui.Phase=$(if($Rollback -eq 'Restored'){'Restored'}else{'Failed'}) }
    }
    Update-CvaInstallerUi
}
function Check-CvaInstaller {
    $script:ui.CustomBody=$null; $script:ui.Detail=''
    try {
        $preview=Invoke-CvaInstall -Root $PackageRoot -Target $Destination
        if ($preview.ObsRunning) { $script:ui.Phase='ObsRunning'; $script:ui.Action='Retry' }
        elseif($PreviewOnly) { $script:ui.Phase='Preview'; $script:ui.Action='Retry' }
        else { $script:ui.Phase='Ready'; $script:ui.Action='Install' }
        Update-CvaInstallerUi
    } catch { Set-CvaFailure (Get-CvaInstallErrorCode $_.Exception) $_.Exception.Message }
}
function Start-CvaInstallerWorker([bool]$Elevated) {
    try {
        if (Test-CvaObsRunning) { Set-CvaFailure 'ObsRunning' ''; return }
        $cli=Join-Path $PSScriptRoot 'CVA-VEIL-Install.ps1'
        if(-not(Test-Path -LiteralPath $cli)){ $cli=Join-Path $PSScriptRoot 'TRUEITEM-Install.ps1' }
        $sessionBase=Join-Path $env:LOCALAPPDATA 'CVA-VEIL\InstallerSessions'
        Assert-CvaOrdinaryPath $sessionBase
        [IO.Directory]::CreateDirectory($sessionBase) | Out-Null
        $script:ui.ResultPath=Join-Path $sessionBase ([Guid]::NewGuid().ToString('N')+'.json')
        $workerArgs='-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+$cli+'" -Apply -PackageRoot "'+$PackageRoot+'" -Destination "'+$Destination+'" -ResultPath "'+$script:ui.ResultPath+'"'
        $start=@{ FilePath=(Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'); ArgumentList=$workerArgs; PassThru=$true; WindowStyle='Hidden' }
        if($Elevated){$start.Verb='RunAs'}
        $script:ui.Worker=Start-Process @start
        $script:ui.Phase='Installing'; $script:ui.Detail=''; $script:ui.CustomBody=$null
        Update-CvaInstallerUi
        $script:poll.Start()
    } catch {
        $script:ui.Worker=$null
        $cancelled=$false; $errorNode=$_.Exception
        while($errorNode){if($errorNode -is [ComponentModel.Win32Exception] -and $errorNode.NativeErrorCode -eq 1223){$cancelled=$true;break};$errorNode=$errorNode.InnerException}
        if($cancelled){$script:ui.Phase='Permission';$script:ui.Action='Grant';$script:ui.CustomBody='UacCancelled';Update-CvaInstallerUi}
        else {Set-CvaFailure 'InstallFailed' $_.Exception.Message}
    }
}
$script:poll=New-Object Windows.Threading.DispatcherTimer
$script:poll.Interval=[TimeSpan]::FromMilliseconds(350)
$script:poll.Add_Tick({
    if(-not $script:ui.Worker -or -not $script:ui.Worker.HasExited){return}
    $script:poll.Stop(); $script:ui.Worker.Dispose(); $script:ui.Worker=$null
    try {
        if(-not(Test-Path -LiteralPath $script:ui.ResultPath)){throw 'The installation process did not return a result.'}
        $result=Get-Content -LiteralPath $script:ui.ResultPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if($result.Status -eq 'Installed'){$script:ui.Phase='Done';$script:ui.Action='OpenObs';$script:ui.BackupPath=$result.BackupPath;$script:ui.Detail='';Update-CvaInstallerUi}
        else { Set-CvaFailure ([string]$result.Code) ([string]$result.Detail) ([string]$result.Rollback) }
    } catch {Set-CvaFailure 'InstallFailed' $_.Exception.Message}
})
for($i=0;$i -lt $script:controls.LanguageBox.Items.Count;$i++){if($script:controls.LanguageBox.Items[$i].Tag -eq $Language){$script:controls.LanguageBox.SelectedIndex=$i}}
$script:controls.LanguageBox.Add_SelectionChanged({$script:ui.Language=[string]$script:controls.LanguageBox.SelectedItem.Tag;Update-CvaInstallerUi})
$script:controls.CloseButton.Add_Click({$script:window.Close()})
$script:controls.Primary.Add_Click({
    switch($script:ui.Action){
        'Install' {if(-not $PreviewOnly){Start-CvaInstallerWorker $false}}
        'Grant' {if(-not $PreviewOnly){Start-CvaInstallerWorker $true}}
        'OpenObs' {
            $obsExe=Join-Path $env:ProgramFiles 'obs-studio\bin\64bit\obs64.exe'
            if(Test-Path -LiteralPath $obsExe){Start-Process -FilePath $obsExe -WorkingDirectory (Split-Path -Parent $obsExe) | Out-Null;$script:window.Close()}
            else{$script:ui.CustomBody='ObsMissing';Update-CvaInstallerUi}
        }
        default {Check-CvaInstaller}
    }
})
$script:window.Add_Closing({param($sender,$eventArgs) if($script:ui.Worker){$eventArgs.Cancel=$true}})
Check-CvaInstaller
$script:window.ShowDialog() | Out-Null
