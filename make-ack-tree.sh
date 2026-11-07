#!/bin/sh
# make-ack-tree.sh [dir]
#
# Builds a kernel tree that matches the stock AT&T BGW320-500 5.15.167 kernel closely enough to build
# loadable modules for it: Android Common Kernel android13-5.15 (the base the stock kernel was built from)
# + the Broadcom struct-layout patches in patches/ + the stock kernel config. Result: ./ack13 (or [dir]),
# prepared for `make M=... modules` (use ./build-module.sh).
#
# Environment:
#   ACK_GIT   where to clone from (default Google's server; a local mirror or clone works too)
#   CROSS_COMPILE  default aarch64-linux-gnu-
set -e
here=$(cd "$(dirname "$0")" && pwd)
T=${1:-$here/ack13}
TAG=android13-5.15-2024-11_r1
ACK_GIT=${ACK_GIT:-https://android.googlesource.com/kernel/common}
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}

command -v "${CROSS_COMPILE}gcc" >/dev/null || { echo "need ${CROSS_COMPILE}gcc (e.g. apt install gcc-aarch64-linux-gnu)"; exit 1; }

if [ ! -d "$T/.git" ]; then
	echo ">> cloning $TAG (shallow, about 1.5 GB)"
	git clone --depth 1 -b "$TAG" "$ACK_GIT" "$T"
fi
cd "$T"

for p in "$here"/patches/0*.patch; do
	if git apply --check "$p" 2>/dev/null; then
		echo ">> applying $(basename "$p")"; git apply "$p"
	elif git apply --check -R "$p" 2>/dev/null; then
		echo ">> already applied: $(basename "$p")"
	else
		echo "!! $(basename "$p") does not apply (tree is not a clean $TAG?)"; exit 1
	fi
done

cp "$here/config/bgw320-stock-5.15.167.config" .config
: > .scmversion     # no "+" suffix from the patched (dirty) git tree: vermagic must be exactly 5.15.167
make ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" olddefconfig modules_prepare
rel=$(make -s ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" kernelrelease)
echo ">> kernelrelease: $rel"
[ "$rel" = 5.15.167 ] || { echo "!! expected 5.15.167"; exit 1; }
echo ">> ready: $T"
