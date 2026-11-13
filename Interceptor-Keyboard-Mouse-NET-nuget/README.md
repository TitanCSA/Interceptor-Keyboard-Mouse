# Interceptor-Keyboard-Mouse .NET SDK

.NET 8+ Windows x64. Package ID: `InterceptorKeyboardMouse.SDK`; Installation: `dotnet add package InterceptorKeyboardMouse.SDK --version 1.0.0`.

```csharp
using TitanCSA.Products.InterceptorKeyboardMouse;
using var client = new Interceptor();
var info = client.GetInfo();
```

Native DLL bundled as runtime asset. No driver installation or capture on construction. [API](docs/API.md). Original licensing/trial prerequisites remain unchanged.
