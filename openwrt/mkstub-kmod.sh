#!/bin/sh
# mkstub.sh <outdir> <package> <description> [<modules.d name> <module>...]
# Builds an OpenWrt kmod "stub" ipk for the BGW320: the real modules are the stock 5.15.167 ones (or ours built on
# this repo) shipped top-level in /lib/modules/5.15.167 of the image; the stub only satisfies opkg
# dependencies and carries the /etc/modules.d autoload file (if modules are given).
set -e
out=$1 pkg=$2 desc=$3; shift 3
ver=5.15.167-1 arch=aarch64_generic
w=$(mktemp -d); mkdir -p "$w/data" "$w/ctl"
if [ $# -gt 0 ]; then
	md=$1; shift
	mkdir -p "$w/data/etc/modules.d"
	for m in "$@"; do echo "$m"; done > "$w/data/etc/modules.d/$md"
fi
cat > "$w/ctl/control" <<C
Package: $pkg
Version: $ver
Depends: libc
Source: bgw320 stock kernel modules
Section: kernel
Architecture: $arch
Installed-Size: 0
Description: Stub: $desc
C
chmod -R u=rwX,go=rX "$w/data" "$w/ctl"
echo "2.0" > "$w/debian-binary"
(cd "$w/data" && tar --owner=0 --group=0 -czf ../data.tar.gz .)
(cd "$w/ctl" && tar --owner=0 --group=0 -czf ../control.tar.gz .)
(cd "$w" && tar --owner=0 --group=0 -czf "$out/${pkg}_${ver}_${arch}.ipk" ./debian-binary ./control.tar.gz ./data.tar.gz)
echo "$out/${pkg}_${ver}_${arch}.ipk"
