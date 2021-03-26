#!/usr/bin/env bash
set -e

bazel build //compiler:interpret

examples=$(realpath $(dirname "$BASH_SOURCE"))
root=$(dirname "$examples")
binary="$root/bazel-bin/compiler/interpret"
export ICARUS_MODULE_PATH="$root/stdlib"

$binary "$examples/enum.ic" >/dev/null && echo OK
$binary "$examples/factorial.ic" >/dev/null && echo OK
$binary "$examples/fibonacci.ic" >/dev/null && echo OK
# file.ic is broken; errors on full flat_hash_map
$binary "$examples/fizzbuzz.ic" >/dev/null && echo OK
$binary "$examples/function_calls.ic" >/dev/null && echo OK
$binary "$examples/ping_pong.ic" | head >/dev/null && echo OK
$binary "$examples/primes.ic" >/dev/null && echo OK
