<#
.SYNOPSIS
    Registers Davesplorer with the Windows shell for the current user.

.DESCRIPTION
    Everything goes under HKCU\Software\Classes, so no administrator rights
    are needed and nothing system-wide changes. Each switch is independent
    and reversible with -Unregister.

    (no switch)   "Open in Davesplorer" on the right-click menu of folders,
                  drives and the folder background.
    -Default      Folders and drives open in Davesplorer on double-click and
                  when another program asks the shell to open a folder.
    -WinE         Win+E and the taskbar's File Explorer button start
                  Davesplorer (redirects the shell's "open new Explorer
                  window" verb). Explorer itself still opens when run
                  directly, which is what Davesplorer's own Explorer button
                  and "Show in Windows Explorer" do.
    -Interceptor  System-wide. Catches EVERY explorer.exe launch, including
                  apps that run "explorer.exe /select,file" by name (which
                  no verb can redirect). An interceptor shim inspects the
                  arguments: a folder or /select path goes to Davesplorer,
                  and everything else -- above all the desktop shell itself
                  -- runs the real Explorer, untouched. Requires
                  administrator rights (it writes HKLM Image File Execution
                  Options) and re-elevates on its own. This is the same
                  registry mechanism malware uses to hijack the shell, so
                  Windows Defender or an antivirus may flag it; that is
                  expected for this switch.
    -Unregister   Removes all of the above, the interceptor included.

.PARAMETER ExePath
    Path to davesplorer.exe. Defaults to the exe beside this script (the
    staged build folder), then to the release build location.

.EXAMPLE
    .\register.ps1                          # context menu only
    .\register.ps1 -Default -WinE           # the per-user "replace Explorer" setup
    .\register.ps1 -Default -WinE -Interceptor  # also catch by-name explorer.exe launches
    .\register.ps1 -Unregister
#>
[CmdletBinding()]
param(
    [switch]$Default,
    [switch]$WinE,
    [switch]$Interceptor,
    [switch]$Unregister,
    [string]$ExePath
)

$ErrorActionPreference = 'Stop'

$ifeoKey = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\explorer.exe'
$shimDir = Join-Path $env:LOCALAPPDATA 'Davesplorer'

# Tell the running shell and any open app to drop their cached file
# associations, so a new default handler takes effect without a restart.
function Broadcast-AssocChanged {
    try {
        Add-Type -Namespace Dsp -Name Shell -MemberDefinition `
            '[DllImport("shell32.dll")] public static extern void SHChangeNotify(int e, uint f, System.IntPtr a, System.IntPtr b);' `
            -ErrorAction SilentlyContinue
    } catch {}
    # SHCNE_ASSOCCHANGED, SHCNF_IDLIST
    [Dsp.Shell]::SHChangeNotify(0x08000000, 0, [System.IntPtr]::Zero, [System.IntPtr]::Zero)
}

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    (New-Object Security.Principal.WindowsPrincipal $id).IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

# The interceptor touches HKLM, so it needs elevation. Re-launch this same
# script elevated with the same switches; the user approves one UAC prompt.
function Assert-AdminOrElevate {
    if (Test-Admin) { return $true }
    $switches = @()
    if ($Default) { $switches += '-Default' }
    if ($WinE) { $switches += '-WinE' }
    if ($Interceptor) { $switches += '-Interceptor' }
    if ($Unregister) { $switches += '-Unregister' }
    if ($ExePath) { $switches += @('-ExePath', "`"$ExePath`"") }
    Write-Host 'The interceptor needs administrator rights; asking for elevation...'
    Start-Process powershell -Verb RunAs -ArgumentList (@(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`""
    ) + $switches)
    return $false
}

if (-not $ExePath) {
    $candidates = @(
        (Join-Path $PSScriptRoot 'davesplorer.exe'),
        'C:\buildfiles\davesplorer\release\stage\davesplorer.exe'
    )
    $ExePath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Unregister -and -not (Test-Path $ExePath)) {
    throw "davesplorer.exe not found. Pass -ExePath <path to davesplorer.exe>."
}
if ($ExePath) { $ExePath = (Resolve-Path $ExePath -ErrorAction SilentlyContinue).Path }

$classes = 'HKCU:\Software\Classes'
$verb = 'Davesplorer'
# The shell's "File Explorer" object; its opennewwindow verb is what Win+E
# and the taskbar button invoke.
$explorerClsid = "$classes\CLSID\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}"

function Set-Verb {
    param([string]$ClassKey, [string]$Argument)
    $key = "$classes\$ClassKey\shell\$verb"
    New-Item -Path "$key\command" -Force | Out-Null
    Set-ItemProperty -Path $key -Name '(default)' -Value 'Open in Davesplorer'
    Set-ItemProperty -Path $key -Name 'Icon' -Value "`"$ExePath`",0"
    Set-ItemProperty -Path "$key\command" -Name '(default)' -Value "`"$ExePath`" `"$Argument`""
}

function Remove-Verb {
    param([string]$ClassKey)
    $shell = "$classes\$ClassKey\shell"
    if (Test-Path "$shell\$verb") { Remove-Item -Path "$shell\$verb" -Recurse -Force }
    if (Test-Path $shell) {
        $current = (Get-ItemProperty -Path $shell -Name '(default)' -ErrorAction SilentlyContinue).'(default)'
        if ($current -eq $verb) { Remove-ItemProperty -Path $shell -Name '(default)' }
    }
}

# Point a class's "open" verb -- what apps request with
# ShellExecute("open", folder) -- at Davesplorer, and blank its DDE entry so
# the folder is not instead handed to the running desktop Explorer. Setting
# the default verb alone is not enough: most apps ask for "open" by name.
function Set-OpenVerb {
    param([string]$ClassKey)
    $open = "$classes\$ClassKey\shell\open"
    New-Item -Path "$open\command" -Force | Out-Null
    Set-ItemProperty -Path "$open\command" -Name '(default)' -Value "`"$ExePath`" `"%1`""
    New-Item -Path "$open\ddeexec" -Force | Out-Null
    Set-ItemProperty -Path "$open\ddeexec" -Name '(default)' -Value ''
}

function Remove-OpenVerb {
    param([string]$ClassKey)
    $open = "$classes\$ClassKey\shell\open"
    # Only our override lives in HKCU; removing it uncovers the built-in
    # Explorer open verb from HKLM again.
    if (Test-Path $open) { Remove-Item -Path $open -Recurse -Force }
}

if ($Unregister) {
    # The interceptor lives in HKLM; if it is present, this needs elevation.
    if ((Test-Path $ifeoKey) -and -not (Assert-AdminOrElevate)) { return }
    Remove-Verb 'Directory'
    Remove-Verb 'Directory\Background'
    Remove-Verb 'Drive'
    Remove-OpenVerb 'Directory'
    Remove-OpenVerb 'Drive'
    if (Test-Path $explorerClsid) { Remove-Item -Path $explorerClsid -Recurse -Force }
    if (Test-Path $ifeoKey) {
        Remove-ItemProperty -Path $ifeoKey -Name 'Debugger' -ErrorAction SilentlyContinue
        Remove-ItemProperty -Path $ifeoKey -Name 'Davesplorer' -ErrorAction SilentlyContinue
        # Leave the IFEO key only if something else populated it.
        if (-not (Get-Item $ifeoKey).Property) { Remove-Item -Path $ifeoKey -Force }
    }
    Broadcast-AssocChanged
    Write-Host 'Davesplorer unregistered: context menu, default handler, Win+E and the interceptor are all back to Explorer.'
    Write-Host 'Restart Explorer (or sign out and in) to drop the interceptor from the running shell.'
    return
}

Set-Verb 'Directory' '%1'
Set-Verb 'Drive' '%1'
Set-Verb 'Directory\Background' '%V'
Write-Host "Context menu: 'Open in Davesplorer' on folders, drives and folder backgrounds."

if ($Default) {
    foreach ($class in 'Directory', 'Drive') {
        # The default verb governs double-click; the open verb governs the
        # explicit ShellExecute("open", folder) that apps use. Set both.
        Set-ItemProperty -Path "$classes\$class\shell" -Name '(default)' -Value $verb
        Set-OpenVerb $class
    }
    Write-Host 'Default: folders and drives now open in Davesplorer (double-click and app "open folder").'
}

if ($WinE) {
    # Win+E invokes the object's opennewwindow verb; the taskbar pin invokes
    # its default verb, open. Both get the same command line.
    foreach ($v in 'open', 'opennewwindow') {
        $cmd = "$explorerClsid\shell\$v\command"
        New-Item -Path $cmd -Force | Out-Null
        Set-ItemProperty -Path $cmd -Name '(default)' -Value "`"$ExePath`""
        # The verb normally delegates to Explorer's own COM handler; an empty
        # DelegateExecute makes the shell run the command line instead.
        Set-ItemProperty -Path $cmd -Name 'DelegateExecute' -Value ''
    }
    Write-Host 'Win+E and the taskbar File Explorer button now start Davesplorer.'
    Write-Host 'If the taskbar button still opens Explorer, restart Explorer (or sign out and in): it caches this.'
}

if ($Interceptor) {
    if (-not (Assert-AdminOrElevate)) { return }

    $stagedShim = Join-Path (Split-Path $ExePath) 'explorer_shim.exe'
    if (-not (Test-Path $stagedShim)) {
        throw "explorer_shim.exe not found beside davesplorer.exe. Build first (it stages next to the exe)."
    }
    New-Item -ItemType Directory -Path $shimDir -Force | Out-Null
    $shim = Join-Path $shimDir 'explorer_shim.exe'
    Copy-Item -Path $stagedShim -Destination $shim -Force
    # A fresh private copy of the real Explorer for the shim to pass through
    # to; a different name means launching it does not come back to the shim.
    Copy-Item -Path (Join-Path $env:SystemRoot 'explorer.exe') `
              -Destination (Join-Path $shimDir 'winexplorer_real.exe') -Force

    New-Item -Path $ifeoKey -Force | Out-Null
    Set-ItemProperty -Path $ifeoKey -Name 'Debugger' -Value "`"$shim`""
    # Where the shim finds Davesplorer, readable before any user hive loads.
    Set-ItemProperty -Path $ifeoKey -Name 'Davesplorer' -Value $ExePath
    Write-Host "Interceptor installed. Every explorer.exe launch now goes through the shim;"
    Write-Host "folder and /select launches open in Davesplorer, everything else in Explorer."
    Write-Host "Recovery, if the desktop ever fails to appear: press Ctrl+Shift+Esc for Task"
    Write-Host "Manager, Run new task 'powershell', then: & '$PSCommandPath' -Unregister"
}

Broadcast-AssocChanged
# A long-running program that resolved the folder handler before this runs
# has it cached; the broadcast above refreshes most, but some only re-read
# on restart.
Write-Host 'Note: apps already running may need a restart to pick up the new folder handler.'
Write-Host "Using $ExePath. Undo with: .\register.ps1 -Unregister"
