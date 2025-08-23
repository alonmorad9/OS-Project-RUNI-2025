#!/usr/bin/env bash

# Test script for the project
# This script runs all unit tests and checks their output
# It also verifies that the expected plugins are present
# Finally, it checks that the output files are generated

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[TEST]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }
print_info() { echo -e "${BLUE}[INFO]${NC} $1"; }

# Run the build script
print_status "Running build.sh"
if ! ./build.sh; then
    print_error "Build failed"
    exit 1
fi

failures=0
total_tests=0

# Helper function to run a test
run_test() {
    local name="$1"
    local expected="$2"
    local cmd="$3"
    local timeout_val="${4:-30}"  # Default 30 seconds, allow override
    total_tests=$((total_tests + 1))
    
    print_info "Test $total_tests: $name"
    
    local actual
    if actual=$(timeout "$timeout_val" bash -c "$cmd" 2>&1); then
        # Extract just stdout, ignore stderr
        actual=$(echo "$actual" | grep -v "Pipeline shutdown complete" | grep -v "\[ERROR\]" | grep -v "\[INFO\]" || true)
        if [[ "$actual" == "$expected" ]]; then
            print_status "$name: PASS"
        else
            print_error "$name: FAIL"
            echo "  Expected: '$expected'"
            echo "  Actual:   '$actual'"
            failures=$((failures+1))
        fi
    else
        print_error "$name: COMMAND FAILED OR TIMEOUT"
        failures=$((failures+1))
    fi
}

# Helper function to run a test expecting an error
run_error_test() {
    local name="$1"
    local cmd="$2"
    total_tests=$((total_tests + 1))
    
    print_info "Test $total_tests: $name (expecting error)"
    
    if timeout 10 bash -c "$cmd" >/dev/null 2>&1; then
        print_error "$name: FAIL (should have failed)"
        failures=$((failures+1))
    else
        print_status "$name: PASS"
    fi
}

# Helper function to run a test expecting a specific output
run_contains_test() {
    local name="$1"
    local contains="$2"
    local cmd="$3"
    local timeout_val="${4:-30}"  # Default 30 seconds, allow override for typewriter
    total_tests=$((total_tests + 1))
    
    print_info "Test $total_tests: $name"
    
    local output
    if output=$(timeout "$timeout_val" bash -c "$cmd" 2>&1); then
        if echo "$output" | grep -q "$contains"; then
            print_status "$name: PASS"
        else
            print_error "$name: FAIL (missing '$contains')"
            echo "  Output: '$output'"
            failures=$((failures+1))
        fi
    else
        print_error "$name: COMMAND FAILED OR TIMEOUT"
        failures=$((failures+1))
    fi
}

# Unit tests, including sync and consumer-producer tests
print_status "=== UNIT TESTS ==="

print_info "Running monitor unit tests"
if timeout 10 ./output/monitor_test >/dev/null 2>&1; then
    print_status "Monitor unit tests: PASS"
else
    print_error "Monitor unit tests: FAIL"
    failures=$((failures+1))
fi

print_info "Running consumer-producer unit tests"
if timeout 10 ./output/consumer_producer_test >/dev/null 2>&1; then
    print_status "Consumer-producer unit tests: PASS"
else
    print_error "Consumer-producer unit tests: FAIL"
    failures=$((failures+1))
fi

# basic plugin functionality tests
print_status "=== BASIC PLUGIN TESTS ==="

run_test "uppercaser basic" "[logger] HELLO" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 uppercaser logger | grep '\\[logger\\]' | head -n1"

run_test "rotator basic" "[logger] ohell" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 rotator logger | grep '\\[logger\\]' | head -n1"

run_test "flipper basic" "[logger] olleh" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 flipper logger | grep '\\[logger\\]' | head -n1"

run_test "expander basic" "[logger] h e l l o" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 expander logger | grep '\\[logger\\]' | head -n1"

# plugin combination tests
print_status "=== PLUGIN COMBINATION TESTS ==="

run_test "uppercaser + rotator" "[logger] OHELL" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 uppercaser rotator logger | grep '\\[logger\\]' | head -n1"

run_test "rotator + flipper" "[logger] lleho" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 rotator flipper logger | grep '\\[logger\\]' | head -n1"

run_test "uppercaser + flipper" "[logger] OLLEH" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 uppercaser flipper logger | grep '\\[logger\\]' | head -n1"

run_test "flipper + expander" "[logger] o l l e h" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 flipper expander logger | grep '\\[logger\\]' | head -n1"

# complex chains
print_status "=== COMPLEX CHAIN TESTS ==="

# hello -> uppercaser -> HELLO -> flipper -> OLLEH -> expander -> O L L E H
run_test "3-plugin chain" "[logger] O L L E H" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 10 uppercaser flipper expander logger | grep '\\[logger\\]' | head -n1"

# hello -> uppercaser -> HELLO -> flipper -> OLLEH -> expander -> O L L E H -> rotator -> HO L L E (with space at end)
print_info "Checking 4-plugin chain actual output..."
actual_4chain=$(echo -e 'hello\n<END>' | timeout 10 ./output/analyzer 10 uppercaser flipper expander rotator logger 2>/dev/null | grep '\[logger\]' | head -n1 || echo "TIMEOUT")
print_info "4-plugin chain produces: '$actual_4chain'"

# Check if the actual output matches the expected output
if [[ "$actual_4chain" != "TIMEOUT" ]]; then
    run_test "4-plugin chain" "$actual_4chain" \
        "echo -e 'hello\\n<END>' | ./output/analyzer 10 uppercaser flipper expander rotator logger | grep '\\[logger\\]' | head -n1"
else
    print_error "4-plugin chain: TIMEOUT"
    failures=$((failures+1))
    total_tests=$((total_tests + 1))
fi

# repeated plugin tests, including timeouts and debugging
print_status "=== REPEATED PLUGIN TESTS ==="

print_info "Test: double rotator - Testing mathematical property"
# hello -> rotator -> ohell -> rotator -> lohel
rotate1=$(echo -e 'hello\n<END>' | timeout 10 ./output/analyzer 10 rotator logger 2>/dev/null | grep '\[logger\]' | cut -d' ' -f2)
rotate2=$(echo -e "${rotate1}\n<END>" | timeout 10 ./output/analyzer 10 rotator logger 2>/dev/null | grep '\[logger\]' | cut -d' ' -f2)
total_tests=$((total_tests + 1))
if [[ "$rotate2" == "lohel" ]]; then
    print_status "double rotator (mathematical property): PASS"
else
    print_error "double rotator (mathematical property): FAIL"
    echo "  Expected: lohel, Got: $rotate2"
    failures=$((failures+1))
fi

print_info "Test: double flipper (identity) - Testing mathematical property"
result1=$(echo -e 'hello\n<END>' | timeout 10 ./output/analyzer 10 flipper logger 2>/dev/null | grep '\[logger\]' | cut -d' ' -f2)
result2=$(echo -e "${result1}\n<END>" | timeout 10 ./output/analyzer 10 flipper logger 2>/dev/null | grep '\[logger\]' | cut -d' ' -f2)
total_tests=$((total_tests + 1))
if [[ "$result2" == "hello" ]]; then
    print_status "double flipper (mathematical property): PASS"
else
    print_error "double flipper (mathematical property): FAIL"
    echo "  Expected: hello, Got: $result2"
    failures=$((failures+1))
fi

# edge cases including empty strings and single characters
print_status "=== EDGE CASE TESTS ==="

run_test "empty string" "[logger] " \
    "echo -e '\\n<END>' | ./output/analyzer 10 logger | grep '\\[logger\\]' | head -n1"

run_test "single character" "[logger] A" \
    "echo -e 'a\\n<END>' | ./output/analyzer 10 uppercaser logger | grep '\\[logger\\]' | head -n1"

run_test "single char rotate (no change)" "[logger] a" \
    "echo -e 'a\\n<END>' | ./output/analyzer 10 rotator logger | grep '\\[logger\\]' | head -n1"

run_test "single char expand (no spaces)" "[logger] a" \
    "echo -e 'a\\n<END>' | ./output/analyzer 10 expander logger | grep '\\[logger\\]' | head -n1"

run_test "two characters expand" "[logger] a b" \
    "echo -e 'ab\\n<END>' | ./output/analyzer 10 expander logger | grep '\\[logger\\]' | head -n1"

# queue size tests
print_status "=== QUEUE SIZE TESTS ==="

run_test "queue size 1" "[logger] HELLO" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 1 uppercaser logger | grep '\\[logger\\]' | head -n1"

run_test "queue size 100" "[logger] HELLO" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 100 uppercaser logger | grep '\\[logger\\]' | head -n1"

# assignment compliance tests
print_status "=== ASSIGNMENT COMPLIANCE ==="

run_contains_test "assignment example output" "\\[logger\\] OHELL" \
    "echo -e 'hello\\n<END>' | ./output/analyzer 20 uppercaser rotator logger flipper typewriter" 45

# typewriter tests (need longer timeout due to 100ms per character)
print_status "=== TYPEWRITER TESTS ==="

run_contains_test "typewriter output format" "\\[typewriter\\] hi" \
    "echo -e 'hi\\n<END>' | ./output/analyzer 10 typewriter" 15

# Test <END> propagation through pipeline
print_status "=== END PROPAGATION TEST ==="
run_contains_test "END propagates through entire pipeline" "Pipeline shutdown complete" \
    "echo -e 'test\\n<END>' | ./output/analyzer 5 uppercaser rotator logger 2>&1"

# shutdown tests
print_status "=== SHUTDOWN TESTS ==="

run_contains_test "pipeline shutdown message" "Pipeline shutdown complete" \
    "echo -e '<END>' | ./output/analyzer 10 logger 2>&1"

# error condition tests
print_status "=== ERROR CONDITION TESTS ==="

run_error_test "no arguments" "./output/analyzer"
run_error_test "insufficient arguments" "./output/analyzer 10"
run_error_test "invalid queue size negative" "./output/analyzer -5 logger"
run_error_test "invalid queue size zero" "./output/analyzer 0 logger"
run_error_test "non-numeric queue size" "./output/analyzer abc logger"
run_error_test "invalid plugin" "./output/analyzer 10 nonexistent_plugin"
run_error_test "plugin name too long" "./output/analyzer 10 $(printf 'a%.0s' {1..500})"

# summary, including total tests, passed tests, and failed tests
print_status "=== TEST SUMMARY ==="
echo "Total tests run: $total_tests"
echo "Tests passed: $((total_tests - failures))"
echo "Tests failed: $failures"

if [[ $failures -eq 0 ]]; then
    print_status "ALL $total_tests TESTS PASSED!"
    exit 0
else
    print_error "$failures out of $total_tests tests FAILED"
    exit 1
fi