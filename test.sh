#!/usr/bin/env bash
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[TEST]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

print_status "Running build.sh"
./build.sh

failures=0

run_test() {
  local name="$1"; shift
  local expected="$1"; shift
  local cmd="$*"
  print_status "Test: $name"
  set +e
  local actual
  actual=$(bash -lc "$cmd")
  local rc=$?
  set -e
  if [[ $rc -ne 0 ]]; then
    print_error "$name: command failed with rc=$rc"
    echo "Output:"; echo "$actual"
    failures=$((failures+1))
    return
  fi
  if [[ "$actual" == "$expected" ]]; then
    print_status "$name: PASS"
  else
    print_error "$name: FAIL (Expected '$expected', got '$actual')"
    failures=$((failures+1))
  fi
}

# Unit tests binaries should run
print_status "Running unit tests (monitor)"
./output/monitor_test | cat >/dev/null
print_status "Running unit tests (consumer_producer)"
./output/consumer_producer_test | cat >/dev/null

# Functional tests
EXPECTED="[logger] HELLO"
ACTUAL_CMD="echo -e 'hello\\n<END>' | ./output/analyzer 10 uppercaser logger | grep '\\[logger\\]' | head -n1"
run_test "uppercaser + logger" "$EXPECTED" "$ACTUAL_CMD"

# Rotator after uppercaser -> OHELL (then logger shows it)
EXPECTED2="[logger] OHELL"
ACTUAL_CMD2="echo -e 'hello\\n<END>' | ./output/analyzer 10 uppercaser rotator logger | grep '\\[logger\\]' | head -n1"
run_test "uppercaser + rotator + logger" "$EXPECTED2" "$ACTUAL_CMD2"

# Incorrect usage: missing args
set +e
./output/analyzer | cat >/dev/null
rc=$?
set -e
if [[ $rc -ne 1 ]]; then
  print_error "Invalid usage exit code: expected 1 got $rc"
  failures=$((failures+1))
else
  print_status "Invalid usage exit code: PASS"
fi

if [[ $failures -ne 0 ]]; then
  print_error "$failures test(s) failed"
  exit 1
fi

print_status "All tests passed"

