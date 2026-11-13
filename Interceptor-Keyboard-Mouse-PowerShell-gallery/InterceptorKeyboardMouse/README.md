# Interceptor-Keyboard-Mouse PowerShell SDK

`Install-Module InterceptorKeyboardMouse -Scope CurrentUser`

Requires PowerShell 7.4+ on Windows x64. Supported families: keyboard, mouse. The native product SDK and license/trial behavior are unchanged. No driver is installed on import or construction.

```powershell
Import-Module InterceptorKeyboardMouse
$client = Connect-InterceptorKeyboardMouseSDK
try { Get-InterceptorKeyboardMouseSDKInfo -Client $client }
finally { Disconnect-InterceptorKeyboardMouseSDK -Client $client }
```

See [API guide](docs/API.md) and `Get-Command -Module InterceptorKeyboardMouse`. The matching driver is installed separately.
