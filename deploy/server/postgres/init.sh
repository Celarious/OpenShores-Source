#!/usr/bin/env bash
set -euo pipefail

: "${AU_DB_OWNER_PASSWORD:?AU_DB_OWNER_PASSWORD is required}"
: "${AU_DB_PASSWORD:?AU_DB_PASSWORD is required}"
: "${SERVER_LOGIN_PASSWORD:?SERVER_LOGIN_PASSWORD is required}"
: "${CREATE_LOCAL_PLAYER:=false}"
: "${LOCAL_PLAYER_NAME:=}"
: "${LOCAL_PLAYER_PASSWORD:=}"

server_hash=$(printf '%s' "$SERVER_LOGIN_PASSWORD" | sha1sum | awk '{print $1}')

psql --username "$POSTGRES_USER" --dbname postgres --set=ON_ERROR_STOP=1 \
    --set=owner_password="$AU_DB_OWNER_PASSWORD" \
    --set=app_password="$AU_DB_PASSWORD" <<'SQL'
CREATE ROLE aumaster LOGIN PASSWORD :'owner_password';
CREATE ROLE audog LOGIN PASSWORD :'app_password';
CREATE DATABASE "auLogin" OWNER aumaster;
CREATE DATABASE "auMail" OWNER aumaster;
CREATE DATABASE "auUniverse" OWNER aumaster;
SQL

psql --username aumaster --dbname auLogin --set=ON_ERROR_STOP=1 \
    --file /opt/openshores/sql/initlogin.sql
psql --username aumaster --dbname auMail --set=ON_ERROR_STOP=1 \
    --file /opt/openshores/sql/initmail.sql
psql --username aumaster --dbname auUniverse --set=ON_ERROR_STOP=1 \
    --file /opt/openshores/sql/inituniverse.sql
psql --username aumaster --dbname auUniverse --set=ON_ERROR_STOP=1 \
    --file /opt/openshores/sql/initstatic.sql
psql --username aumaster --dbname auUniverse --set=ON_ERROR_STOP=1 \
    --file /opt/openshores/sql/legacyspacecraft.sql
psql --username aumaster --dbname auUniverse --set=ON_ERROR_STOP=1 \
    --file /opt/openshores/sql/gamegeneration.sql
psql --username aumaster --dbname auUniverse --set=ON_ERROR_STOP=1 \
    --file /opt/openshores/sql/legacycities.sql
psql --username aumaster --dbname auUniverse --set=ON_ERROR_STOP=1 \
    --file /opt/openshores/sql/initlegacystatic.sql

# Keep the checked-in server accounts, but do not carry historical personal
# accounts into newly initialized deployments.
psql --username aumaster --dbname auLogin --set=ON_ERROR_STOP=1 \
    --set=server_hash="$server_hash" <<'SQL'
DELETE FROM g_Player WHERE status <> 2;
UPDATE g_Player
SET passcrypt = decode(:'server_hash', 'hex')
WHERE status = 2;
SQL

if [[ "$CREATE_LOCAL_PLAYER" == "true" ]]; then
    : "${LOCAL_PLAYER_NAME:?LOCAL_PLAYER_NAME is required}"
    : "${LOCAL_PLAYER_PASSWORD:?LOCAL_PLAYER_PASSWORD is required}"
    player_hash=$(printf '%s' "$LOCAL_PLAYER_PASSWORD" | sha1sum | awk '{print $1}')
    psql --username aumaster --dbname auLogin --set=ON_ERROR_STOP=1 \
        --set=player_hash="$player_hash" \
        --set=player_name="$LOCAL_PLAYER_NAME" <<'SQL'
INSERT INTO g_Player
    (name, passcrypt, timeCreate, status, activated, neverExpire)
VALUES
    (:'player_name', decode(:'player_hash', 'hex'), CURRENT_TIMESTAMP, 5,
     CURRENT_TIMESTAMP, true);
SQL
fi
