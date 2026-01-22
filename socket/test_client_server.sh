#!/bin/sh
# Minimal client/server test harness with readable output for students.
# This script runs 5 tests against the C client/server pair:
# 1) Short text message end-to-end.
# 2) Long alphanumeric text payload (large input handling).
# 3) Long binary payload (non-text data handling).
# 4) Sequential short messages from separate client connections.
# 5) Concurrent clients sending the same message.
#
# Usage:
#   ./test_client_server.sh <server-port>
# Optional:
#   VERBOSE=0 ./test_client_server.sh <server-port>   # Only sizes, no previews
set -eu

# Single argument: server port to listen on and connect to.
PORT="${1:-}"
if [ -z "$PORT" ]; then
    echo "Usage: $0 <server-port>" >&2
    exit 1
fi

# VERBOSE=1 prints concise content previews; VERBOSE=0 prints only sizes/hashes.
VERBOSE="${VERBOSE:-1}"

# Ensure background server is always terminated on exit.
SERVER_PID=""
cleanup() {
    if [ -n "${SERVER_PID}" ]; then
        kill "$SERVER_PID" >/dev/null 2>&1 || true
        wait "$SERVER_PID" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

if [ ! -x ./client ] || [ ! -x ./server ]; then
    if [ -f Makefile ]; then
        make
    else
        echo "Missing client/server binaries and no Makefile found." >&2
        exit 1
    fi
fi

mktemp_file() {
    # Portable temp file helper for macOS/Linux.
    mktemp -t socket_test.XXXXXX
}

file_size() {
    # Byte size for a file (portable to macOS/Linux).
    wc -c < "$1" | tr -d ' '
}

preview_text() {
    # Preview first and last chunks of a text file without flooding output.
    FILE="$1"
    SIZE=$(file_size "$FILE")
    LIMIT=240
    if [ "$SIZE" -le "$LIMIT" ]; then
        cat "$FILE"
        return
    fi
    head -c 120 "$FILE"
    printf "\n... (truncated, showing first/last 120 bytes) ...\n"
    tail -c 120 "$FILE"
}

print_file_info() {
    # Print size; optionally include a preview when VERBOSE=1.
    FILE="$1"
    MODE="$2"
    SIZE=$(file_size "$FILE")
    echo "size_bytes: $SIZE"
    if [ "$VERBOSE" -eq 0 ]; then
        return
    fi
    if [ "$MODE" = "text" ]; then
        echo "content_preview:"
        preview_text "$FILE"
    else
        # Binary preview: first 64 bytes, hex+ASCII.
        echo "hex_preview:"
        hexdump -C -n 64 "$FILE" || true
    fi
}

report_io() {
    # Print labeled input/output summaries for a test.
    LABEL="$1"
    INPUT="$2"
    OUTPUT="$3"
    MODE="$4"
    echo "=== $LABEL client_input ==="
    print_file_info "$INPUT" "$MODE"
    echo "=== $LABEL server_output ==="
    print_file_info "$OUTPUT" "$MODE"
}

start_server() {
    # Start server in background; ensure it stays alive after launch.
    OUT_FILE="$1"
    ERR_FILE="$2"
    ./server "$PORT" >"$OUT_FILE" 2>"$ERR_FILE" &
    SERVER_PID=$!
    sleep 0.2
    if ! kill -0 "$SERVER_PID" >/dev/null 2>&1; then
        echo "Server failed to start on port $PORT." >&2
        if [ -s "$ERR_FILE" ]; then
            echo "=== server stderr ===" >&2
            cat "$ERR_FILE" >&2
        fi
        exit 1
    fi
}

stop_server() {
    # Stop background server cleanly.
    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
}

compare_files() {
    # Compare expected vs actual output; optionally show diff for text.
    LABEL="$1"
    EXPECTED="$2"
    ACTUAL="$3"
    TEXT_DIFF="$4"

    if cmp -s "$EXPECTED" "$ACTUAL"; then
        echo "$LABEL: PASS"
        return 0
    fi

    echo "$LABEL: FAIL"
    if [ "$TEXT_DIFF" = "yes" ]; then
        diff -u "$EXPECTED" "$ACTUAL" || true
    fi
    return 1
}

run_client() {
    # Run client with stdin already set by the caller.
    ./client 127.0.0.1 "$PORT" 2>>"$CLIENT_ERR"
}

summarize_test() {
    # Show result + concise diagnostics to help students debug quickly.
    LABEL="$1"
    EXPECTED="$2"
    ACTUAL="$3"
    MODE="$4"
    TEXT_DIFF="$5"
    CLIENT_FAIL="$6"
    CLIENT_STATUS="$7"
    SERVER_ERR="$8"
    CLIENT_ERR_FILE="$9"

    compare_files "$LABEL" "$EXPECTED" "$ACTUAL" "$TEXT_DIFF"
    RESULT=$?
    report_io "$LABEL" "$EXPECTED" "$ACTUAL" "$MODE"

    if [ "$RESULT" -ne 0 ] || [ "$CLIENT_FAIL" -ne 0 ] || [ -s "$SERVER_ERR" ] || [ -s "$CLIENT_ERR_FILE" ]; then
        echo "=== $LABEL diagnostics ==="
        if [ "$CLIENT_FAIL" -ne 0 ]; then
            echo "client_exit_status: $CLIENT_STATUS"
        fi
        if [ -s "$CLIENT_ERR_FILE" ]; then
            echo "client_stderr:"
            cat "$CLIENT_ERR_FILE"
        fi
        if [ -s "$SERVER_ERR" ]; then
            echo "server_stderr:"
            cat "$SERVER_ERR"
        fi
        if [ "$RESULT" -ne 0 ]; then
            EXP_SIZE=$(file_size "$EXPECTED")
            ACT_SIZE=$(file_size "$ACTUAL")
            if [ "$ACT_SIZE" -eq 0 ]; then
                echo "reason: server produced no output"
            fi
            if [ "$EXP_SIZE" -ne "$ACT_SIZE" ]; then
                echo "reason: size mismatch expected=$EXP_SIZE actual=$ACT_SIZE"
            fi
        fi
    fi
}

TEST_OUT=$(mktemp_file)
TEST_ERR=$(mktemp_file)
CLIENT_ERR=$(mktemp_file)

# Test 1: short message.
# Steps:
# - Create a temp file with a small text payload.
# - Start the server and capture its stdout/stderr to temp files.
# - Pipe the payload into the client (client connects, sends, exits).
# - Stop the server after a short delay to flush output.
# - Compare server output to the original payload and print diagnostics.
MSG1=$(mktemp_file)
printf 'Go Tigers!\n' >"$MSG1"
: >"$TEST_OUT"
: >"$TEST_ERR"
: >"$CLIENT_ERR"
start_server "$TEST_OUT" "$TEST_ERR"
CLIENT_FAIL=0
CLIENT_STATUS=0
if cat "$MSG1" | run_client; then
    CLIENT_FAIL=0
else
    CLIENT_FAIL=1
    CLIENT_STATUS=$?
fi
sleep 0.2
stop_server
summarize_test "Test 1" "$MSG1" "$TEST_OUT" text yes "$CLIENT_FAIL" "$CLIENT_STATUS" "$TEST_ERR" "$CLIENT_ERR"

# Test 2: long alphanumeric text payload.
# Steps:
# - Generate a large (100000 bytes) alphanumeric-only payload.
# - Start the server and capture its stdout/stderr to temp files.
# - Stream the payload through the client to test chunked send logic.
# - Stop the server and compare output bytes to the input payload.
MSG2=$(mktemp_file)
LC_ALL=C tr -dc 'A-Za-z0-9' < /dev/urandom | head -c 100000 >"$MSG2"
: >"$TEST_OUT"
: >"$TEST_ERR"
: >"$CLIENT_ERR"
start_server "$TEST_OUT" "$TEST_ERR"
CLIENT_FAIL=0
CLIENT_STATUS=0
if cat "$MSG2" | run_client; then
    CLIENT_FAIL=0
else
    CLIENT_FAIL=1
    CLIENT_STATUS=$?
fi
sleep 0.2
stop_server
summarize_test "Test 2" "$MSG2" "$TEST_OUT" text no "$CLIENT_FAIL" "$CLIENT_STATUS" "$TEST_ERR" "$CLIENT_ERR"

# Test 3: long binary payload.
# Steps:
# - Generate a 65536-byte binary payload from /dev/urandom.
# - Start the server and capture its stdout/stderr to temp files.
# - Stream the binary payload through the client.
# - Stop the server and compare raw bytes (no text diff).
MSG3=$(mktemp_file)
head -c 65536 /dev/urandom >"$MSG3"
: >"$TEST_OUT"
: >"$TEST_ERR"
: >"$CLIENT_ERR"
start_server "$TEST_OUT" "$TEST_ERR"
CLIENT_FAIL=0
CLIENT_STATUS=0
if cat "$MSG3" | run_client; then
    CLIENT_FAIL=0
else
    CLIENT_FAIL=1
    CLIENT_STATUS=$?
fi
sleep 0.2
stop_server
summarize_test "Test 3" "$MSG3" "$TEST_OUT" binary no "$CLIENT_FAIL" "$CLIENT_STATUS" "$TEST_ERR" "$CLIENT_ERR"

# Test 4: sequential short messages from separate clients.
# Steps:
# - Build an expected output file by appending 5 short lines.
# - Start the server and capture its stdout/stderr to temp files.
# - For each line, start a new client connection and send one line.
# - Stop the server and compare the concatenated output to expected.
MSG4=$(mktemp_file)
: >"$MSG4"
: >"$TEST_OUT"
: >"$TEST_ERR"
: >"$CLIENT_ERR"
start_server "$TEST_OUT" "$TEST_ERR"
i=1
CLIENT_FAIL=0
CLIENT_STATUS=0
while [ $i -le 5 ]; do
    printf 'seq-%d\n' "$i" >>"$MSG4"
    if printf 'seq-%d\n' "$i" | run_client; then
        :
    else
        CLIENT_FAIL=1
        CLIENT_STATUS=$?
    fi
    i=$((i + 1))
done
sleep 0.2
stop_server
summarize_test "Test 4" "$MSG4" "$TEST_OUT" text yes "$CLIENT_FAIL" "$CLIENT_STATUS" "$TEST_ERR" "$CLIENT_ERR"

# Test 5: concurrent clients sending the same message.
# Steps:
# - Create a one-line message and an expected output file with N copies.
# - Start the server and capture its stdout/stderr to temp files.
# - Launch N clients in parallel, each sending the same message once.
# - Wait for all client PIDs to finish, then stop the server.
# - Compare the server output to the expected concatenation.
MSG5=$(mktemp_file)
printf 'concurrent-message\n' >"$MSG5"
EXPECTED5=$(mktemp_file)
: >"$EXPECTED5"
N=5
i=1
while [ $i -le $N ]; do
    cat "$MSG5" >>"$EXPECTED5"
    i=$((i + 1))
done
: >"$TEST_OUT"
: >"$TEST_ERR"
: >"$CLIENT_ERR"
start_server "$TEST_OUT" "$TEST_ERR"
i=1
CLIENT_FAIL=0
CLIENT_STATUS=0
CLIENT_PIDS=""
while [ $i -le $N ]; do
    (cat "$MSG5" | run_client) || CLIENT_FAIL=1 &
    CLIENT_PIDS="$CLIENT_PIDS $!"
    i=$((i + 1))
done
wait $CLIENT_PIDS
sleep 0.2
stop_server
summarize_test "Test 5" "$EXPECTED5" "$TEST_OUT" text no "$CLIENT_FAIL" "$CLIENT_STATUS" "$TEST_ERR" "$CLIENT_ERR"

rm -f "$MSG1" "$MSG2" "$MSG3" "$MSG4" "$MSG5" "$EXPECTED5" "$TEST_OUT" "$TEST_ERR" "$CLIENT_ERR"
