#!/usr/bin/env bash

### TODO:
### REVIEW
### AI GENERATED SCRIPT
### EXPECT SOME SLOP

set -euo pipefail

# Usage: ./db_upgrade.sh path/to/prod.db schema.sql
PROD_DB="${1:-}"
SCHEMA_SQL="${2:-}"

if [[ -z "$PROD_DB" || -z "$SCHEMA_SQL" ]]; then
  echo "Usage: $0 path/to/prod.db path/to/schema.sql"
  exit 2
fi

# Tools check
for cmd in sqlite3 sqldiff nano mktemp date; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Required tool not found: $cmd" >&2
    exit 3
  fi
done

# Create temp files and cleanup on exit
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT
TMP_DB="$TMP_DIR/temp_schema.db"
DIFF_SQL="$TMP_DIR/upgrade.sql"
BACKUP_DIR="${PROD_DB%.*}_backups"
TIMESTAMP="$(date +%Y%m%d%H%M%S)"

echo "Creating temporary DB from schema..."
sqlite3 "$TMP_DB" < "$SCHEMA_SQL"

echo "Computing diff (producing upgrade SQL)..."
# sqldiff arguments: sqldiff prod_db tmp_db
# It prints SQL statements to transform prod_db -> tmp_db
sqldiff "$PROD_DB" "$TMP_DB" > "$DIFF_SQL" || true

if [[ ! -s "$DIFF_SQL" ]]; then
  echo "No differences found. Production DB already matches schema."
  exit 0
fi

# Let user inspect and edit upgrade script
echo "Opening upgrade SQL in nano for review: $DIFF_SQL"
nano "$DIFF_SQL"

# Confirm proceed
read -r -p "Proceed with applying the upgrade to '$PROD_DB'? (y/N): " CONFIRM
if [[ "${CONFIRM,,}" != "y" ]]; then
  echo "Upgrade aborted by user."
  exit 0
fi

# Verify DB is not open by other processes (best-effort)
if command -v lsof >/dev/null 2>&1; then
  if lsof "$PROD_DB" >/dev/null 2>&1; then
    echo "Warning: $PROD_DB appears to be opened by another process. Aborting." >&2
    lsof "$PROD_DB"
    exit 4
  fi
elif command -v fuser >/dev/null 2>&1; then
  if fuser "$PROD_DB" >/dev/null 2>&1; then
    echo "Warning: $PROD_DB appears to be in use by another process. Aborting." >&2
    fuser "$PROD_DB" || true
    exit 4
  fi
else
  echo "Warning: cannot check whether DB file is open (no lsof/fuser). Proceeding with caution."
fi

# Create backup
mkdir -p "$BACKUP_DIR"
BACKUP_FILE="$BACKUP_DIR/$(basename "$PROD_DB").backup.$TIMESTAMP"
echo "Creating backup: $BACKUP_FILE"
cp -- "$PROD_DB" "$BACKUP_FILE"

# Optionally run integrity check before applying
echo "Running integrity_check on production DB..."
sqlite3 "$PROD_DB" 'PRAGMA integrity_check;' | grep -q '^ok$' || { echo "Integrity check failed. Aborting."; exit 5; }

# Apply upgrade inside a transaction
echo "Applying upgrade..."
# Ensure the SQL ends with a newline
: >> "$DIFF_SQL"

# Run the upgrade within sqlite3; fail on error
sqlite3 "$PROD_DB" <<EOF
BEGIN;
.read $DIFF_SQL
COMMIT;
EOF

echo "Upgrade applied successfully. Backup is at: $BACKUP_FILE"
