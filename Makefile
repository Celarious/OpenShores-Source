# Docker builds are the default. NATIVE=1 selects native Linux compilation.
.DEFAULT_GOAL := help

CONFIG ?= release
NATIVE ?= 0
CMAKE ?= cmake
GENERATOR ?= Ninja
PARALLEL ?= $(shell nproc 2>/dev/null || echo 4)

LINUX_IMAGE ?= hazeron-linux-qt5
WINDOWS_IMAGE ?= hazeron-mingw-qt5
LINUX_CACHE_VOLUME ?= hazeron-linux-$(CONFIG)-cache
WINDOWS_CACHE_VOLUME ?= hazeron-windows-$(CONFIG)-cache
ASSETS_CACHE_VOLUME ?= hazeron-assets-cache
NATIVE_BUILD_DIR ?= build/native-linux-$(CONFIG)

ifeq ($(CONFIG),debug)
CMAKE_BUILD_TYPE := Debug
else ifeq ($(CONFIG),release)
CMAKE_BUILD_TYPE := Release
else ifeq ($(CONFIG),relwithdebinfo)
CMAKE_BUILD_TYPE := RelWithDebInfo
else
$(error CONFIG must be debug, release, or relwithdebinfo)
endif

ifneq ($(NATIVE),0)
ifneq ($(NATIVE),1)
$(error NATIVE must be 0 or 1)
endif
endif

include mk/native.mk
include mk/macos.mk
include mk/build.mk
include mk/windows.mk
include mk/assets.mk
include mk/package.mk
include mk/deploy-server.mk
include mk/help.mk
