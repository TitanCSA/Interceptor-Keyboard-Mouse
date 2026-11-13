param([string]$Manifest,[string]$Fixture)
$ErrorActionPreference='Stop'
Import-Module $Manifest
Test-ModuleManifest $Manifest | Out-Null
function Check($Value,[string]$Message){if(-not $Value){throw $Message}}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
public static class ProductFixture {
 [DllImport(@"D:\GithubRepo\Private\Operations\Package-Publication-20261112\input-family-builds\Interceptor-Keyboard-Mouse\fixture\Release\InterceptorKeyboardMouseBridge.dll", EntryPoint="interceptor_keyboard_mouse_test_mode", CallingConvention=CallingConvention.Cdecl)] public static extern void Mode(uint mode);
 public static Task<int> Receive(object client) {return Task.Run(()=>{var batch=client.GetType().GetMethod("Receive").Invoke(client,new object[]{2000});return (int)batch.GetType().GetProperty("Count").GetValue(batch);});}
}
'@
$allowed=@('keyboard','mouse')
$client=Connect-InterceptorKeyboardMouseSDK
try {
 Check (Initialize-InterceptorKeyboardMouseSDK -Client $client).Ready 'Initialize failed'
 $info=Get-InterceptorKeyboardMouseSDKInfo -Client $client
 Check ($info.InputMask -eq 3 -and $info.ControllerMask -eq 0) 'Wrong product masks'
 $devices=@(Get-InterceptorKeyboardMouseSDKDevice -Client $client);Check ($devices.Count -eq $allowed.Count) 'Wrong devices'
 $batch=Receive-InterceptorKeyboardMouseSDKInput -Client $client -Timeout 0
 Check ($batch.Count -eq $allowed.Count) 'Wrong event families'
 foreach($event in $batch) {
  Check ($allowed -contains $event.Kind) 'Unsupported family received'
  Check ($event.Sequence -gt [ulong]::Parse('9223372036854775808')) 'uint64 lost'
  if($event.Kind -eq 'keyboard'){$event.State['scan_code']=0x30}
  if($event.Kind -eq 'mouse'){$event.State['y']=-7}
 }
 Check ((Send-InterceptorKeyboardMouseSDKBatch -Client $client -Batch $batch) -eq $allowed.Count) 'Send failed'
 Check ((Send-InterceptorKeyboardMouseSDKBatch -Client $client -Batch $batch) -eq 0) 'Duplicate send'
 $current=Receive-InterceptorKeyboardMouseSDKInput -Client $client -Timeout 0;$stale=$false
 try {Send-InterceptorKeyboardMouseSDKBatch -Client $client -Batch $batch | Out-Null}catch{$stale=$true};Check $stale 'Stale batch accepted'
 $current[0].Blocked=$true
 Check ((Send-InterceptorKeyboardMouseSDKBatch -Client $client -Batch $current) -eq ($allowed.Count-1)) 'Blocking failed'
 $retry=Receive-InterceptorKeyboardMouseSDKInput -Client $client -Timeout 0;[ProductFixture]::Mode(2);$partial=$false
 try {Send-InterceptorKeyboardMouseSDKBatch -Client $client -Batch $retry | Out-Null}catch{$errorObject=$_.Exception;while($errorObject.InnerException){$errorObject=$errorObject.InnerException};$partial=($errorObject.Code -eq 170 -and $errorObject.Accepted -eq 1)}
 Check $partial 'Partial send error lost'
 Check ((Send-InterceptorKeyboardMouseSDKBatch -Client $client -Batch $retry) -eq ($allowed.Count-1)) 'Partial retry failed';[ProductFixture]::Mode(0)
 $discard=Receive-InterceptorKeyboardMouseSDKInput -Client $client -Timeout 0;Remove-InterceptorKeyboardMouseSDKBatch -Client $client -Batch $discard
 [ProductFixture]::Mode(1);$wait=[ProductFixture]::Receive($client);Start-Sleep -Milliseconds 50
 Stop-InterceptorKeyboardMouseSDKWait -Client $client;Check ($wait.Wait(1000) -and $wait.Result -eq 0) 'Thread cancellation failed';[ProductFixture]::Mode(0)
}finally{Disconnect-InterceptorKeyboardMouseSDK -Client $client}
Write-Output 'Interceptor-Keyboard-Mouse PowerShell SDK: product masks, uint64, edits, block, partial retry, stale batch and threaded cancellation passed'
