$ErrorActionPreference='Stop'
function Assert-Administrator {
    if(!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Privileges administrateur requis.' }
    if(![Environment]::Is64BitProcess) { throw 'Utiliser Windows PowerShell x64.' }
    if([Environment]::OSVersion.Version.Build -lt 16299) { throw 'Windows 10 version 1709 ou plus recent, ou Windows 11 x64 requis.' }
}
function Add-InterceptorPackage([string]$Path) {
    if([Environment]::OSVersion.Version.Build -lt 14393) { & pnputil.exe -a $Path }
    else { & pnputil.exe /add-driver $Path }
    if($LASTEXITCODE -notin @(0,3010)) { throw "Ajout au magasin en echec: $Path ($LASTEXITCODE)" }
}
function Remove-InterceptorPackage([string]$Name) {
    # --restore is run first, so the legacy path need not forcibly remove a
    # still-bound package on Windows 10 1507/1511.
    if([Environment]::OSVersion.Version.Build -lt 14393) { & pnputil.exe -d $Name|ForEach-Object {Write-Host $_} }
    else { & pnputil.exe /delete-driver $Name /uninstall|ForEach-Object {Write-Host $_} }
    if($LASTEXITCODE -notin @(0,3010)) { throw "Suppression du paquet $Name en echec ($LASTEXITCODE). Fermez les clients et redemarrez." }
    $script:InterceptorNeedsReboot=$script:InterceptorNeedsReboot -or $LASTEXITCODE -eq 3010
}
function Start-InterceptorLog([string]$Name) {
    $folder=Join-Path $env:ProgramData 'InterceptorKeyboardMouse'
    Initialize-InterceptorStorage
    Start-Transcript -Path (Join-Path $folder ($Name+'-'+(Get-Date -Format yyyyMMdd-HHmmss)+'.log')) | Out-Null
}
function Get-InterceptorPackages {
    @(Get-WindowsDriver -Online | Where-Object {
        $_.ProviderName -eq 'TitanCSA Keyboard Mouse' -and
        [IO.Path]::GetFileName($_.OriginalFileName) -in @('interceptorkeyboardmousekeyboard.inf','interceptorkeyboardmousemouse.inf','interceptorkeyboardmousecontrol.inf')
    })
}
function Get-InterceptorSigningMode([string]$Payload) {
    $path=Join-Path $Payload 'Signing.json'
    if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw 'Package signing policy is missing.'}
    $policy=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if($policy.Schema -ne 1 -or $policy.Mode -notin @('Development','Microsoft')){throw 'Politique de signature du paquet invalide.'}
    return [string]$policy.Mode
}
function Assert-InterceptorPayload([string]$Payload,[switch]$AllowDevelopment) {
    $manifest=Get-Content -LiteralPath (Join-Path $Payload 'SHA256.json') -Raw | ConvertFrom-Json
    $mode=Get-InterceptorSigningMode $Payload
    $names=@('InterceptorKeyboardMouse.sys','InterceptorKeyboardMouseKeyboard.inf','InterceptorKeyboardMouseMouse.inf','InterceptorKeyboardMouseControl.inf','interceptorkeyboardmousekeyboard.cat','interceptorkeyboardmousemouse.cat','interceptorkeyboardmousecontrol.cat','InterceptorExample.exe')
    if($mode -eq 'Development' -and !$AllowDevelopment){throw 'This product has a new driver identity. Microsoft signing is required before installation. Submit the product CAB and import its signed return; no Windows setting was changed.'}
    if(Test-Path -LiteralPath (Join-Path $Payload 'Signing.json')){$names+= 'Signing.json'}
    foreach($name in $names) {
        $path=Join-Path $Payload $name
        if(!(Test-Path -LiteralPath $path -PathType Leaf) -or !$manifest.$name -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $manifest.$name){throw "Paquet incomplet ou modifie: $name"}
    }
    foreach($name in @('InterceptorKeyboardMouseKeyboard.inf','InterceptorKeyboardMouseMouse.inf','InterceptorKeyboardMouseControl.inf')){
        $inf=Get-Content -LiteralPath (Join-Path $Payload $name) -Raw
        if($inf -notmatch '(?im)^\s*AddService\s*=\s*InterceptorKeyboardMouse\s*,'){throw "Unexpected service identity: $name"}
    }
    if($mode -eq 'Microsoft') {
        foreach($name in @('InterceptorKeyboardMouse.sys','InterceptorKeyboardMouseKeyboard.cat','InterceptorKeyboardMouseMouse.cat','InterceptorKeyboardMouseControl.cat')) {
            $signature=Get-AuthenticodeSignature -LiteralPath (Join-Path $Payload $name)
            if($signature.Status -ne 'Valid' -or !$signature.SignerCertificate -or
               $signature.SignerCertificate.Subject -ne 'CN=Microsoft Windows Hardware Compatibility Publisher, O=Microsoft Corporation, L=Redmond, S=Washington, C=US') {
                throw "Signature Microsoft invalide: $name"
            }
        }
    }
    return $mode
}
. (Join-Path $PSScriptRoot 'Lifecycle.ps1')
