# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | interceptor-keyboard-mouse |
| CPP/conan | public_recipe_builds_locally |  [Source](packaging/native/) |
| CPP/nuget | paused_pending_owner_email_confirmation | Interceptor-Keyboard-Mouse.Native |
| NET/nuget | built_verified_publication_on_hold_by_owner | InterceptorKeyboardMouse.SDK [Source](Interceptor-Keyboard-Mouse-NET-nuget/) |
| Python/pypi | built_fixture_and_production_verified | interceptor-keyboard-mouse [Source](Interceptor-Keyboard-Mouse-Python-pypi/) |
| Python/conda-forge | built_tested_requires_official_review | interceptor-keyboard-mouse [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [interceptor-keyboard-mouse](https://crates.io/crates/interceptor-keyboard-mouse) [Source](Interceptor-Keyboard-Mouse-Rust-cargo/) |
| JavaScript/npm | awaiting_browser_publication_authentication | [interceptor-keyboard-mouse-sdk](https://www.npmjs.com/package/interceptor-keyboard-mouse-sdk) [Source](Interceptor-Keyboard-Mouse-JavaScript-npm/) |
| Java/maven | published_install_verified | [interceptor-keyboard-mouse](https://central.sonatype.com/artifact/io.github.titancsa/interceptor-keyboard-mouse/1.0.0) [Source](Interceptor-Keyboard-Mouse-Java-maven/) |
| Java/jitpack | published_install_verified | [interceptor-keyboard-mouse](https://jitpack.io/#TitanCSA/Interceptor-Keyboard-Mouse/java-sdk-v1.0.0) [Source](Interceptor-Keyboard-Mouse-Java-maven/) |
| Go/modules | published_install_verified | [interceptor-keyboard-mouse](https://pkg.go.dev/github.com/TitanCSA/Interceptor-Keyboard-Mouse/Interceptor-Keyboard-Mouse-Go-modules@v1.0.0) [Source](Interceptor-Keyboard-Mouse-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/TitanCSA/Interceptor-Keyboard-Mouse/Interceptor-Keyboard-Mouse-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [InterceptorKeyboardMouse](https://www.powershellgallery.com/packages/InterceptorKeyboardMouse/1.0.0) [Source](Interceptor-Keyboard-Mouse-PowerShell-gallery/) |
| Dart/pub-dev | published_install_verified | [interceptor_keyboard_mouse](https://pub.dev/packages/interceptor_keyboard_mouse) [Source](Interceptor-Keyboard-Mouse-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [interceptor-keyboard-mouse](https://rubygems.org/gems/interceptor-keyboard-mouse) [Source](Interceptor-Keyboard-Mouse-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/interceptor-keyboard-mouse](https://packagist.org/packages/titancsa/interceptor-keyboard-mouse) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
