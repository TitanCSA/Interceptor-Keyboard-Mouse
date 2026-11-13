# Interceptor-Keyboard-Mouse Dart / Flutter Windows SDK

`dart pub add interceptor_keyboard_mouse` or `flutter pub add interceptor_keyboard_mouse`

Windows x64 and Dart 3.4+. Direct FFI to the product SDK; no Python dependency or driver installer. Supported families: keyboard, mouse.

```dart
import 'package:interceptor_keyboard_mouse/interceptor_keyboard_mouse.dart';
Future<void> main() async {
  final client = await Interceptor.open();
  try { print(client.info()); } finally { client.close(); }
}
```

Calls are synchronous: use a worker isolate for a Flutter UI. Send `client.cancellationToken` to another isolate to cancel an active receive; the token contains only an immutable library path and session number. See [API documentation](doc/API.md).

For compiled Dart and deployed Flutter Windows apps, bundle `lib/native/InterceptorKeyboardMouseBridge.dll` next to the executable, or provide its absolute `libraryPath`. This plain Dart package does not automatically alter a Flutter build. Native licensing, trial limits and driver requirements are unchanged.
