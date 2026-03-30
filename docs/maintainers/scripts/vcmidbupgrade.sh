#!/usr/bin/env bash

set -euo pipefail

# Configuration (edit)
PROD_DB="~/.local/share/vcmi/vcmiLobby.db"
UPGRADE_SQL="${WORK_DIR}/lobbyUpgrade.sql"

# Ensure DB is not in use
if lsof -- "$PROD_DB" >/dev/null 2>&1; then
  echo "Production DB appears to be in use. Aborting." >&2
  lsof -- "$PROD_DB"
  exit 3
fi

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
BACKUP_FILE="${PROD_DB}.${TIMESTAMP}.bak"

cp -- "$PROD_DB" "$BACKUP_FILE"
sync
echo "Backup created: $BACKUP_FILE"

# Verify backup integrity
if ! sqlite3 "$BACKUP_FILE" "PRAGMA integrity_check;" | grep -qi '^ok$'; then
  echo "Backup integrity_check failed. Aborting." >&2
  exit 4
fi

# Apply upgrade
if ! sqlite3 "$PROD_DB" < "$UPGRADE_SQL"; then
  echo "Upgrade failed. Restoring backup." >&2
  cp -- "$BACKUP_FILE" "$PROD_DB"
  echo "Restored backup: $BACKUP_FILE"
  exit 5
fi

echo "Upgrade applied successfully. Backup retained at: $BACKUP_FILE"
