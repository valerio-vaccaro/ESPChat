#!/usr/bin/env bash
# Host interop test: firmware Noise XX / packet / TLV code vs an independent Python implementation.
# Requires g++, zlib headers, python3 + `cryptography`, and the PlatformIO libdeps (pio pkg install).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CR="$(ls -d "$ROOT"/.pio/libdeps/*/Crypto | head -1)"
OUT="$ROOT/.pio/host_test"
mkdir -p "$(dirname "$OUT")"
cd "$ROOT/tools/host_test"
g++ -std=gnu++17 -O1 -Istubs -I"$CR" host_test.cpp compress_host.cpp rng_stub.cpp \
  "$ROOT"/src/proto/{packet,payloads,bytes}.cpp "$ROOT"/src/crypto/{noise,crypto_util}.cpp \
  "$CR"/{SHA256,SHA512,Hash,Curve25519,Ed25519,ChaChaPoly,ChaCha,Poly1305,BigNumberUtil,Crypto,Cipher,AuthenticatedCipher}.cpp \
  -lz -o "$OUT"
python3 interop_test.py "$OUT"
