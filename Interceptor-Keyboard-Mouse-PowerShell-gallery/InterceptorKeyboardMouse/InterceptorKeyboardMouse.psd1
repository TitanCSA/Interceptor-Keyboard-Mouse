@{
RootModule = 'InterceptorKeyboardMouse.psm1'
ModuleVersion = '1.0.0'
GUID = '82ef742b-d0d1-5eec-8ef6-43541ee5715c'
Author = 'TitanCSA'
Description = 'Windows x64 Interceptor-Keyboard-Mouse input SDK for PowerShell 7.4+. Driver installed separately.'
PowerShellVersion = '7.4'
CompatiblePSEditions = @('Core')
FunctionsToExport = @('Connect-InterceptorKeyboardMouseSDK','Initialize-InterceptorKeyboardMouseSDK','Get-InterceptorKeyboardMouseSDKInfo','Get-InterceptorKeyboardMouseSDKDevice','Receive-InterceptorKeyboardMouseSDKInput','Send-InterceptorKeyboardMouseSDKBatch','Remove-InterceptorKeyboardMouseSDKBatch','Stop-InterceptorKeyboardMouseSDKWait','Get-InterceptorKeyboardMouseSDKDescriptor','Send-InterceptorKeyboardMouseSDKInput','Disconnect-InterceptorKeyboardMouseSDK')
CmdletsToExport = @()
VariablesToExport = @()
AliasesToExport = @()
PrivateData = @{ PSData = @{ Tags = @('Windows','SDK','Keyboard','Mouse','Gamepad'); LicenseUri = 'https://github.com/TitanCSA/Interceptor-Keyboard-Mouse/blob/main/LICENSE'; ProjectUri = 'https://github.com/TitanCSA/Interceptor-Keyboard-Mouse'; ReleaseNotes = 'First PowerShell SDK; docs and native bridge included.' } }
}
