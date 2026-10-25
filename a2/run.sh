#!/bin/sh
set -e

dir=$(dirname "$0")
make -C "$dir" build
exec "$dir/build/asan/shell" "$@"
