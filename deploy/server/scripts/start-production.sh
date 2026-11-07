#!/usr/bin/env bash
# Installed as start.sh at the root of the production Compose artifact.
set -euo pipefail

bundle_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
env_file="$bundle_dir/compose.env"
server_image=$(awk -F= '$1 == "SERVER_IMAGE" { print $2 }' "$env_file")
: "${server_image:?Missing SERVER_IMAGE in compose.env}"

# The base Compose file expects the server image to already be available.
docker pull "$server_image"

BUILDX_NO_DEFAULT_ATTESTATIONS=1 exec docker compose \
    --env-file "$env_file" \
    -f "$bundle_dir/deploy/server/compose.yaml" \
    -f "$bundle_dir/workers.compose.json" \
    --profile website \
    up --build --detach --remove-orphans --wait "$@"
