#!/bin/sh
# build-module.sh <module dir> [make args...]
# Builds the out-of-tree modules in <module dir> against the tree from make-ack-tree.sh and checks each .ko.
#   KDIR           kernel tree (default ./ack13)
#   CROSS_COMPILE  default aarch64-linux-gnu-
set -e
here=$(cd "$(dirname "$0")" && pwd)
[ -n "$1" ] || { echo "usage: $0 <module dir> [make args]"; exit 1; }
M=$(cd "$1" && pwd); shift
KDIR=${KDIR:-$here/ack13}
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
[ -f "$KDIR/Module.symvers" ] || [ -f "$KDIR/include/generated/autoconf.h" ] || { echo "no prepared tree at $KDIR - run ./make-ack-tree.sh first"; exit 1; }
make -C "$KDIR" ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" M="$M" modules "$@"
# exports of the other modules built here (e.g. lte needs usbnet's)
extra=$(find "$here/modules" -name Module.symvers -size +0 2>/dev/null)
rc=0
for ko in "$M"/*.ko; do
	[ -f "$ko" ] || continue
	"$here/tools/check-module.sh" "$ko" $extra || rc=1
done
exit $rc
