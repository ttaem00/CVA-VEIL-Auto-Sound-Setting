// CVA VEIL modification, 2026-11-09. GPL-2.0; see LICENSE.
// A small Windows GUI host for the shared installer script. No console or
// second copy of the UI is needed. PowerShell policy is process-local only.
using System;
using System.Collections.Generic;
using System.IO;
using System.Management.Automation;
using System.Management.Automation.Runspaces;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;

[assembly: AssemblyTitle("CVA-VEIL-Auto Sound Setting Setup")]
[assembly: AssemblyProduct("CVA-VEIL-Auto Sound Setting")]
[assembly: AssemblyVersion("1.3.0.0")]
[assembly: AssemblyFileVersion("1.3.0.0")]

internal static class SetupHost
{
    [STAThread]
    private static int Main(string[] args)
    {
        string language = "ko-KR";
        for (int index = 0; index + 1 < args.Length; index++)
            if (String.Equals(args[index].TrimStart('-'), "Language", StringComparison.OrdinalIgnoreCase)) language = args[index + 1];
        try
        {
            string folder = AppDomain.CurrentDomain.BaseDirectory;
            string script = Path.Combine(folder, "CVA-VEIL-Setup.ps1");
            if (!File.Exists(script)) throw new FileNotFoundException("Extract the complete ZIP before opening the installer.", script);
            var parameters = new Dictionary<string, object>(StringComparer.OrdinalIgnoreCase);
            for (int i = 0; i < args.Length; i++)
            {
                string name = args[i].TrimStart('-');
                if (String.Equals(name, "PreviewOnly", StringComparison.OrdinalIgnoreCase)) parameters[name] = true;
                else if (String.Equals(name, "Language", StringComparison.OrdinalIgnoreCase) ||
                         String.Equals(name, "PackageRoot", StringComparison.OrdinalIgnoreCase) ||
                         String.Equals(name, "Destination", StringComparison.OrdinalIgnoreCase))
                {
                    if (++i >= args.Length) throw new ArgumentException("Missing value for " + name);
                    parameters[name] = args[i];
                }
                else throw new ArgumentException("Unknown installer option: " + name);
            }
            var initial = InitialSessionState.CreateDefault();
            initial.ExecutionPolicy = Microsoft.PowerShell.ExecutionPolicy.Bypass;
            using (var runspace = RunspaceFactory.CreateRunspace(initial))
            {
                runspace.ApartmentState = ApartmentState.STA;
                runspace.ThreadOptions = PSThreadOptions.UseCurrentThread;
                runspace.Open();
                using (var shell = PowerShell.Create())
                {
                    shell.Runspace = runspace;
                    shell.AddCommand(script);
                    foreach (var parameter in parameters) shell.AddParameter(parameter.Key, parameter.Value);
                    shell.Invoke();
                    // Only a terminating script failure should open a startup
                    // error. Expected, suppressed probe errors are not failures.
                    if (shell.InvocationStateInfo.State == PSInvocationState.Failed)
                    {
                        string message = shell.Streams.Error.Count > 0 ? shell.Streams.Error[0].ToString() : "The installer could not start.";
                        throw new InvalidOperationException(message);
                    }
                }
            }
            return 0;
        }
        catch (Exception error)
        {
            string message = language == "en-US" ? "The installer could not start. Extract the entire ZIP to a new folder and try again." :
                language == "ja-JP" ? "インストーラーを開けませんでした。ZIP全体を新しいフォルダーに展開して、もう一度お試しください。" :
                "설치 창을 열지 못했어요. 압축 파일 전체를 새 폴더에 풀고 다시 열어 주세요.";
            try
            {
                string logs = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "CVA-VEIL", "InstallerSessions");
                Directory.CreateDirectory(logs);
                File.WriteAllText(Path.Combine(logs, "last-host-error.txt"), error.ToString());
            }
            catch { /* Showing the recovery message must not depend on log storage. */ }
            MessageBox.Show(message, "CVA-VEIL-Auto Sound Setting", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}
