require 'interceptor_keyboard_mouse'
InterceptorKeyboardMouse::Interceptor.open { |client| p client.info }
