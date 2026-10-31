# Native OpenShores Mac client

Upstream is [Celarious/SoH-source](https://github.com/Celarious/SoH-source),
mirrored at [yamamushi/OpenShores-Source](https://github.com/yamamushi/OpenShores-Source).
The `mac-client` branch starts from upstream `main` commit
`849c43ba5d2a9960a38e3b180301033545902c10` and uses that source's protocol
definitions and default service hosts.

Mac-only viewport restoration accounts for Retina framebuffer pixel dimensions,
preventing HUD icons/text from alternating between half and full size. Mac mouse
look samples on the GUI thread and uses event-free Core Graphics recentering;
the existing sensitivity setting is retained. Other platforms keep their
existing viewport and mouse-input paths.

Build prerequisites are Apple command-line developer tools, CMake, Ninja,
Python 3.9 or newer, Qt 5.15 and SDL2. Homebrew SDL2-compat additionally needs SDL3.
The build defaults to Homebrew's Qt 5 and dependency prefixes, which can be
overridden with `MACOS_QT_PREFIX` and `MACOS_DEPENDENCY_PREFIX`.

```sh
make mac-client CONFIG=debug PARALLEL=6
```

`CONFIG` accepts the same `debug`, `release` (default), and `relwithdebinfo`
values as Linux and Windows. The Mac build uses OpenGL 4.1 and is native to
the build machine's architecture. It does not invoke Docker or modify system
dependency installations.

- Native intermediates and binaries: `build/native-macos-<config>/`
- Standalone app: `build/native-macos-<config>/bin/OpenShores.app`
- Shareable disk image: `build/packages/soh-macos-<architecture>.dmg`
- SHA-256: beside the DMG, with a `.sha256` suffix

`make mac-client-build` builds without packaging; `make mac-package` packages
an existing build. Pass the same `CONFIG` when using these commands.
All generated artifacts are ignored under `build/`.

The compressed, read-only DMG contains `OpenShores.app` and an Applications
shortcut. Open the image and drag the app into Applications to install it.
To create a DMG from an already packaged app without rebuilding dependencies:

```sh
make mac-dmg CONFIG=debug
```

Packaging deploys Qt and its plugins, SDL and its runtime dependencies,
Hazeron resource libraries, textures, sound effects, blueprints and game data.
The app uses the Windows executable's icon artwork converted to Mac icon
sizes. The source ICO has only a 32x32 image, so larger sizes are scaled.
Data resides in `Contents/Resources` with relative links for legacy lookups.

The package checks the dependency graph, removes absolute development rpaths,
sets its minimum macOS version from the bundled binaries, and signs locally
with an ad-hoc identity. It is not Developer ID signed or Apple notarized.
Recipients need no Homebrew installation or source checkout. The current
Homebrew SDL libraries require macOS 26; Intel or older macOS support needs
separately built and verified dependencies.

## Connecting

```sh
open -n build/native-macos-debug/bin/OpenShores.app --args -log
```

The source defaults to `login.openshores.net:16757`,
`mail.openshores.net:16758`, and `scene.openshores.net:16759`.
Do not pass the website hostname or obsolete `play.openshores.net` with `-a`:
that argument overrides all three game hosts. The server can then redirect
the client to worker ports, so those must also be reachable.

Enter account details in the client. Never put passwords in commands or logs.
`-log` enables connection diagnostics in the app's `soh.txt` cache log.
Protocol versions and stream definitions come from OpenShores itself;
the Mac tooling does not spoof versions or alter shared protocol code.

Linux and Windows keep their existing Docker, asset, and package targets.
Mac-specific CMake settings are guarded by `APPLE`, and Make includes an
additional module whose recipes run only for explicitly selected Mac targets.

## Validation

Against the pinned upstream `main` revision:

- Configured the native arm64 Debug build and built hashpack/audio assets.
- Compiled the Mac mouse-look implementation, a viewport-reset caller, and the
  scene-client entry point using CMake-generated compiler commands.
- Compared both modified C++ files after excluding macOS code: identical to
  upstream `main` apart from whitespace. Python packaging scripts pass syntax checks.

DMG creation, read-only mounting, app signatures, checksum verification, and
an offline version check were tested with a packaged OpenShores app. A full
client link, new DMG build, and online gameplay against this `main` base have
not yet been repeated. Full Linux/Windows compilation was not rerun on this Mac.
