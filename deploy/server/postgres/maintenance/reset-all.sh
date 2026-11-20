#!/usr/bin/env bash
set -euo pipefail

# Resets databases managed outside the Docker deployment. For the local
# Compose stack, use `make server-reset` instead.
# The current user must have database superuser privileges.
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
schema_dir="$script_dir/../schema"
#
# Typical usage:
#  bash ./resetALL >resetLog 2>&1

# Login Database
	#dropdb auLogin
	#createdb -T template0 -O aumaster auLogin
	#psql auLogin -U aumaster -f initlogin.sql

	# Just reset unit ids in login database so all the login accounts don't have to be recreated.
	psql auLogin -U aumaster -c 'UPDATE g_PLayer SET idu=0;'

# Mail Database
	dropdb auMail
	createdb -T template0 -O aumaster auMail
	psql auMail -U aumaster -f "$schema_dir/initmail.sql"

# Universe Database
	dropdb auUniverse
	createdb -T template0 -O aumaster auUniverse
	psql auUniverse -U aumaster -f "$schema_dir/inituniverse.sql"
	psql auUniverse -U aumaster -f "$schema_dir/initstatic.sql"
	psql auUniverse -U aumaster -v ON_ERROR_STOP=1 -f "$schema_dir/legacyspacecraft.sql"
	psql auUniverse -U aumaster -v ON_ERROR_STOP=1 -f "$schema_dir/gamegeneration.sql"
	psql auUniverse -U aumaster -v ON_ERROR_STOP=1 -f "$schema_dir/legacycities.sql"
	psql auUniverse -U aumaster -v ON_ERROR_STOP=1 -f "$schema_dir/initlegacystatic.sql"
