#!/usr/bin/env bash
# Installed as stop.sh at the root of the production Compose artifact.
set -euo pipefail

bundle_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

# Preserve the production database and web report volumes when stopping the stack.
BUILDX_NO_DEFAULT_ATTESTATIONS=1 exec docker compose \
    --env-file "$bundle_dir/compose.env" \
    -f "$bundle_dir/deploy/server/compose.yaml" \
    -f "$bundle_dir/workers.compose.json" \
    --profile website \
    down --remove-orphans "$@"
