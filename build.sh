#!/usr/bin/env bash

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${GREEN}[BUILD]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

mkdir -p output

print_status "Building analyzer (main)"
if ! gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2 -o output/analyzer \
  main.c \
  -ldl -lpthread; then
    print_error "Failed to build main analyzer"
    exit 1
fi

print_status "Building sync unit tests"
if ! gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2 -Iplugins -o output/monitor_test \
  plugins/sync/monitor_test.c plugins/sync/monitor.c -lpthread; then
    print_error "Failed to build monitor unit tests"
    exit 1
fi

if ! gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2 -Iplugins -o output/consumer_producer_test \
  plugins/sync/consumer_producer_test.c \
  plugins/sync/monitor.c plugins/sync/consumer_producer.c -lpthread; then
    print_error "Failed to build consumer-producer unit tests"
    exit 1
fi

print_status "Building plugins"
plugins=(logger uppercaser rotator flipper expander typewriter)
for plugin_name in "${plugins[@]}"; do
  src_file="plugins/${plugin_name}.c"
  if [[ ! -f "$src_file" ]]; then
    print_warning "Skipping missing plugin source $src_file"
    continue
  fi
  
  print_status "Building plugin: $plugin_name"
  if ! gcc -std=c11 -D_POSIX_C_SOURCE=200809L -fPIC -shared -Wall -Wextra -O2 -o output/${plugin_name}.so \
    plugins/${plugin_name}.c \
    plugins/plugin_common.c \
    plugins/sync/monitor.c \
    plugins/sync/consumer_producer.c \
    -ldl -lpthread; then
    print_error "Failed to build plugin: $plugin_name"
    exit 1
  fi
done

# Quick verification that everything works
print_status "Running unit tests to verify build"

print_status "Testing monitor implementation"
if ! timeout 10 ./output/monitor_test >/dev/null 2>&1; then
    print_error "Monitor unit tests failed"
    exit 1
fi

print_status "Testing consumer-producer queue"
if ! timeout 10 ./output/consumer_producer_test >/dev/null 2>&1; then
    print_error "Consumer-producer unit tests failed"
    exit 1
fi

print_status "Running smoke test"
if ! echo -e 'test\n<END>' | timeout 10 ./output/analyzer 5 logger >/dev/null 2>&1; then
    print_error "Smoke test failed"
    exit 1
fi

print_status "Build complete - all components built and tested successfully"