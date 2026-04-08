#!/bin/sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BIN_PATH="${SQL_PROCESSOR_BIN:-$ROOT_DIR/sql_processor}"
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/sql-processor.XXXXXX")

cleanup() {
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT INT TERM

mkdir -p "$TMP_DIR/db"

cat > "$TMP_DIR/db/users.schema" <<'EOF'
id
name
age
EOF

cat > "$TMP_DIR/insert_partial.sql" <<'EOF'
INSERT INTO users (name, id) VALUES ('Bob', 2);
EOF

"$BIN_PATH" -d "$TMP_DIR/db" -f "$ROOT_DIR/queries/insert_users.sql" > "$TMP_DIR/insert1.out"
"$BIN_PATH" -d "$TMP_DIR/db" -f "$TMP_DIR/insert_partial.sql" > "$TMP_DIR/insert2.out"
"$BIN_PATH" -d "$TMP_DIR/db" -f "$ROOT_DIR/queries/select_users.sql" > "$TMP_DIR/select_all.out"
"$BIN_PATH" -d "$TMP_DIR/db" -f "$ROOT_DIR/queries/select_user_names.sql" > "$TMP_DIR/select_partial.out"

cat > "$TMP_DIR/expected.data" <<'EOF'
1|Alice|20
2|Bob|
EOF

cmp "$TMP_DIR/db/users.data" "$TMP_DIR/expected.data"
grep -q "INSERT 1" "$TMP_DIR/insert1.out"
grep -q "INSERT 1" "$TMP_DIR/insert2.out"
grep -q "Alice" "$TMP_DIR/select_all.out"
grep -q "Bob" "$TMP_DIR/select_all.out"
grep -q "2 rows selected" "$TMP_DIR/select_all.out"
grep -q "name" "$TMP_DIR/select_partial.out"
grep -q "id" "$TMP_DIR/select_partial.out"
grep -q "Alice" "$TMP_DIR/select_partial.out"
grep -q "Bob" "$TMP_DIR/select_partial.out"
grep -q "2 rows selected" "$TMP_DIR/select_partial.out"

echo "integration: OK"
