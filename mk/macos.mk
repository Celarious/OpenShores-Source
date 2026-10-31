# Native Mac builds are opt-in and do not change the Linux/Windows Docker paths.
MACOS_BUILD_DIR ?= build/native-macos-$(CONFIG)
MACOS_QT_PREFIX ?= $(shell brew --prefix qt@5 2>/dev/null)
MACOS_DEPENDENCY_PREFIX ?= $(shell brew --prefix 2>/dev/null)

.PHONY: _mac-configure _mac-audio mac-client-build mac-client mac-package mac-dmg
_mac-configure:
	@test "$$(uname -s)" = Darwin || { echo 'Mac targets require macOS.' >&2; exit 1; }
	$(CMAKE) -S . -B "$(MACOS_BUILD_DIR)" -G "$(GENERATOR)" \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		-DCMAKE_PREFIX_PATH="$(MACOS_QT_PREFIX);$(MACOS_DEPENDENCY_PREFIX)/opt/sdl2" \
		-DAU_OPENGL_VERSION=41

_mac-audio: _mac-configure
	$(CMAKE) --build "$(MACOS_BUILD_DIR)" --target hashpack --parallel "$(PARALLEL)"
	python3 tools/macos/audio.py --hashpack "$(MACOS_BUILD_DIR)/bin/hashpack"

mac-client-build: _mac-audio
	$(CMAKE) --build "$(MACOS_BUILD_DIR)" --target AuSceneClient --parallel "$(PARALLEL)"

mac-client: mac-client-build
	python3 tools/macos/package.py --build-dir "$(MACOS_BUILD_DIR)" \
		--qt-prefix "$(MACOS_QT_PREFIX)" --dependency-prefix "$(MACOS_DEPENDENCY_PREFIX)"

mac-package:
	python3 tools/macos/package.py --build-dir "$(MACOS_BUILD_DIR)" \
		--qt-prefix "$(MACOS_QT_PREFIX)" --dependency-prefix "$(MACOS_DEPENDENCY_PREFIX)"

# Reuse a fully packaged app without recompiling or redeploying dependencies.
mac-dmg:
	python3 tools/macos/dmg.py --app "$(MACOS_BUILD_DIR)/bin/OpenShores.app" \
		--output "build/packages/soh-macos-$$(uname -m).dmg"
