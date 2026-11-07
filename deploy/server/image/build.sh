#!/usr/bin/env bash
set -euo pipefail

if (($# < 3 || $# > 5)); then
    echo "usage: $0 CONFIG IMAGE TARGET [TOOLCHAIN_IMAGE] [CACHE_VOLUME]" >&2
    exit 2
fi

config=$1
image=$2
target=$3
toolchain_image=${4:-hazeron-linux-qt5}
cache_volume=${5:-hazeron-linux-${config}-cache}

case "$config" in
    debug|release|relwithdebinfo) ;;
    *) echo "error: unsupported build configuration: $config" >&2; exit 2;;
esac
case "$target" in
    server-runtime|server-debug) ;;
    *) echo "error: unsupported server image target: $target" >&2; exit 2;;
esac

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
source "$repo_root/docker/support/staging.sh"
artifacts="$repo_root/build/linux-$config"

HAZERON_LINUX_IMAGE="$toolchain_image" \
HAZERON_LINUX_CACHE_VOLUME="$cache_volume" \
HAZERON_BUILD_CONFIG="$config" \
    bash "$repo_root/docker/linux/build.sh" servers

create_staging_directory "$repo_root/build/server" "image-context-$config"
context=$staging_dir
mkdir -p "$context/bin" "$context/lib"
cp "$artifacts/bin/AuLoginServer" "$context/bin/"
cp "$artifacts/bin/AuMailServer" "$context/bin/"
cp "$artifacts/bin/AuSceneServer" "$context/bin/"
cp -a "$artifacts"/lib/*.so* "$context/lib/"
cp "$repo_root/deploy/server/runtime/motds" "$context/motds"
cp "$repo_root/deploy/server/image/health-port.sh" "$context/health-port.sh"

docker build \
    --provenance=false \
    --target "$target" \
    --tag "$image" \
    --file "$repo_root/deploy/server/image/Dockerfile" \
    "$context"
