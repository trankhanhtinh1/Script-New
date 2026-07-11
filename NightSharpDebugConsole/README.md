# NightSharp Debug Console / Crash Monitor

`console.exe` receives NightSharp logs and crash telemetry over:

```text
\\.\pipe\NightSharpDebugConsole
```

It creates full dumps outside the game process and configures Windows Error
Reporting LocalDumps as a fallback. It does not attach with
`DebugActiveProcess`.

## Build

```powershell
& 'E:\Visual Studio\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'NightSharpDebugConsole\NightSharpDebugConsole.vcxproj' `
  /t:Build /p:Configuration=Release /p:Platform=x64 `
  /p:PlatformToolset=v145 /m /v:minimal
```

Output:

```text
NightSharp\bin\Release\console.exe
```

## Usage

1. Run `NightSharp\bin\Release\console.exe` as Administrator.
2. Confirm it prints `C:\Users\Public\NightSharpDumps` and waits for
   NightSharp.
3. Inject `NightSharp.dll` normally.
4. Confirm the console shows PID, module base, phase, stage, and heartbeat.
5. After a crash, inspect the newest `.txt`, then open its `.dmp` in Visual
   Studio with the matching `NightSharp.pdb` beside `console.exe`.

Artifacts are written to:

```text
C:\Users\Public\NightSharpDumps
```

The monitor retains five finalized NightSharp incident sets. WER also uses a
per-application `DumpCount=5` setting for `League of Legends.exe`.

Remove only the WER values still matching NightSharp's configuration:

```powershell
NightSharp\bin\Release\console.exe --cleanup-wer
```

## Test Mode

Test mode skips HKLM/WER changes:

```powershell
NightSharp\bin\Release\console.exe --test-mode --no-wer `
  --dump-folder C:\Temp\NightSharpCrashTest
```

Run the deterministic fixture matrix:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File NightSharpDebugConsole\tests\run_crash_integration.ps1 `
  -Scenario handled-av
```

Supported fixture scenarios are `handled-av`, `unhandled-av`, `failfast`,
`forced`, and `wait`.

## Limits

`TerminateProcess`, anti-cheat termination, shutdown, power loss, and kernel
termination can destroy the process without a user-mode exception context. In
that case the report intentionally says
`forced-termination-no-exception` and includes only the real exit code and last
telemetry. It never invents an access violation or call stack.

Full dumps may contain sensitive process memory. They remain local and are
never uploaded automatically.
