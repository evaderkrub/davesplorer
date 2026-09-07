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
    -Unregister   Removes all of the above.

.PARAMETER ExePath
    Path to davesplorer.exe. Defaults to the exe beside this script (the
    staged build folder), then to the release build location.

.EXAMPLE
    .\register.ps1                  # context menu only
    .\register.ps1 -Default -WinE   # the full "replace Explorer" setup
    .\register.ps1 -Unregister
#>
[CmdletBinding()]
param(
    [switch]$Default,
    [switch]$WinE,
    [switch]$Unregister,
    [string]$ExePath
)

$ErrorActionPreference = 'Stop'

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

if ($Unregister) {
    Remove-Verb 'Directory'
    Remove-Verb 'Directory\Background'
    Remove-Verb 'Drive'
    if (Test-Path $explorerClsid) { Remove-Item -Path $explorerClsid -Recurse -Force }
    Write-Host 'Davesplorer unregistered: context menu, default folder handler and Win+E are back to Explorer.'
    return
}

Set-Verb 'Directory' '%1'
Set-Verb 'Drive' '%1'
Set-Verb 'Directory\Background' '%V'
Write-Host "Context menu: 'Open in Davesplorer' on folders, drives and folder backgrounds."

if ($Default) {
    foreach ($class in 'Directory', 'Drive') {
        Set-ItemProperty -Path "$classes\$class\shell" -Name '(default)' -Value $verb
    }
    Write-Host 'Default: folders and drives now open in Davesplorer.'
}

if ($WinE) {
    $cmd = "$explorerClsid\shell\opennewwindow\command"
    New-Item -Path $cmd -Force | Out-Null
    Set-ItemProperty -Path $cmd -Name '(default)' -Value "`"$ExePath`""
    # The verb normally delegates to Explorer's own COM handler; an empty
    # DelegateExecute makes the shell run the command line instead.
    Set-ItemProperty -Path $cmd -Name 'DelegateExecute' -Value ''
    Write-Host 'Win+E and the taskbar File Explorer button now start Davesplorer.'
}

Write-Host "Using $ExePath. Undo with: .\register.ps1 -Unregister"
