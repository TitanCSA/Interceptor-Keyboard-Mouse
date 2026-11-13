Set-StrictMode -Version Latest
if (-not $IsWindows -or [IntPtr]::Size -ne 8) { throw 'Interceptor requires Windows x64.' }
Add-Type -Path (Join-Path $PSScriptRoot 'lib/InterceptorKeyboardMouseSDKPowerShell.dll')

function Connect-InterceptorKeyboardMouseSDK {
    <#
.SYNOPSIS
Opens a lazy SDK context without installing or capturing from a driver.
#>
    [CmdletBinding()] param()
    [TitanCSA.Products.InterceptorKeyboardMouse.PowerShell.Interceptor]::new()
}
function Initialize-InterceptorKeyboardMouseSDK {
    <#
.SYNOPSIS
Associates supported devices using an already installed driver.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client)
    $Client.Initialize()
}
function Get-InterceptorKeyboardMouseSDKInfo {
    <#
.SYNOPSIS
Returns driver status and SDK information.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client)
    $Client.GetInfo()
}
function Get-InterceptorKeyboardMouseSDKDevice {
    <#
.SYNOPSIS
Lists devices exposed by the matching installed driver.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client)
    $Client.Devices()
}
function Receive-InterceptorKeyboardMouseSDKInput {
    <#
.SYNOPSIS
Receives a complete input batch; forward it or explicitly discard it.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client, [ValidateRange(-1,2147483647)][int]$Timeout = -1)
    Write-Output -NoEnumerate ($Client.Receive($Timeout))
}
function Send-InterceptorKeyboardMouseSDKBatch {
    <#
.SYNOPSIS
Forwards an owned received batch, including edits and blocked flags.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client, [Parameter(Mandatory)]$Batch)
    $Client.Send($Batch)
}
function Remove-InterceptorKeyboardMouseSDKBatch {
    <#
.SYNOPSIS
Deliberately discards the received batch instead of forwarding it.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client, [Parameter(Mandatory)]$Batch)
    $Client.Discard($Batch)
}
function Stop-InterceptorKeyboardMouseSDKWait {
    <#
.SYNOPSIS
Cancels an outstanding native receive operation.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client)
    $Client.Cancel()
}
function Get-InterceptorKeyboardMouseSDKDescriptor {
    <#
.SYNOPSIS
Returns common descriptor data or the native HID report descriptor.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client, [Parameter(Mandatory)][string]$Token, [switch]$Report)
    $Client.GetDescriptor($Token, $Report.IsPresent)
}
function Send-InterceptorKeyboardMouseSDKInput {
    <#
.SYNOPSIS
Injects named state fields into an existing supported input source.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client,
        [Parameter(Mandatory)][ValidateSet('keyboard','mouse','hid','xinput','xinputhid','gip')][string]$Kind,
        [Parameter(Mandatory)][System.Collections.IDictionary]$State, [string]$Token, [byte[]]$Report)
    $values = [System.Collections.Generic.Dictionary[string,long]]::new()
    foreach ($entry in $State.GetEnumerator()) { $values.Add([string]$entry.Key, [long]$entry.Value) }
    $Client.Inject($Kind, $values, $Token, $Report)
}
function Disconnect-InterceptorKeyboardMouseSDK {
    <#
.SYNOPSIS
Cancels pending waits and disposes the SDK context.
#>
    [CmdletBinding()] param([Parameter(Mandatory)]$Client)
    $Client.Dispose()
}
