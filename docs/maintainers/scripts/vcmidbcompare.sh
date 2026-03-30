#!/usr/bin/env bash

set -euo pipefail

# Configuration (edit)
PROD_DB="~/.local/share/vcmi/vcmiLobby.db"
SCHEMA_SQL_FILE="/usr/share/vcmi/config/lobby.sql"
WORK_DIR="$(dirname -- "$PROD_DB")"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
UPGRADE_SQL="${WORK_DIR}/lobbyUpgrade.sql"

# Create temporary sqlite DB from schema SQL
TMP_SCHEMA_DB="${WORK_DIR}/lobbyUpgrade.db"
sqlite3 "$TMP_SCHEMA_DB" < "$SCHEMA_SQL_FILE"

# Generate upgrade SQL using sqldiff (schema changes)
sqldiff --schema "$PROD_DB" "$TMP_SCHEMA_DB" > "$UPGRADE_SQL" || {
  echo "sqldiff failed" >&2
  rm -f "$TMP_SCHEMA_DB"
  exit 1
}

if [[ ! -s "$UPGRADE_SQL" ]]; then
  echo "No schema changes detected"
else
  echo "Upgrade script generated: $UPGRADE_SQL"
fi

# Open for review/edit
nano "$UPGRADE_SQL"
