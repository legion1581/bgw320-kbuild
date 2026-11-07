#!/bin/sh
# check-module.sh <module.ko> [extra exports file...]
# Static checks before a module goes near the router:
#   1. vermagic is exactly the stock one
#   2. struct module layout: init_module at 0x140, cleanup_module at 0x248 (the stock kernel's offsets)
#   3. every undefined symbol is exported by the stock kernel or a stock module (data/stock_exports.txt)
#      or by the extra export files given (e.g. a Module.symvers of modules you build yourself)
# Exit status 1 if any check fails.
here=$(cd "$(dirname "$0")/.." && pwd)
ko=$1; shift
P=${CROSS_COMPILE:-aarch64-linux-gnu-}
want_vm="5.15.167 SMP preempt mod_unload aarch64"
fail=0; name=$(basename "$ko")

vm=$("${P}objcopy" -O binary --only-section=.modinfo "$ko" /dev/stdout 2>/dev/null | tr '\0' '\n' | sed -n 's/^vermagic=//p')
if [ "$vm" = "$want_vm" ]; then echo "$name: vermagic ok"; else echo "$name: vermagic '$vm' != '$want_vm'"; fail=1; fi

rel=$("${P}readelf" -rW "$ko" | awk '/Relocation section .*\.rela\.gnu\.linkonce\.this_module/{f=1;next} /Relocation section/{f=0} f&&/R_AARCH64_ABS64/{print $1, $5}')
init=$(echo "$rel" | awk '$2=="init_module"{print $1}' | sed 's/^0*//'); exit_=$(echo "$rel" | awk '$2=="cleanup_module"{print $1}' | sed 's/^0*//')
if [ -z "$init" ] || [ "$init" = 140 ]; then
	if [ -z "$exit_" ] || [ "$exit_" = 248 ]; then echo "$name: struct module ok (init@0x${init:-none}, exit@0x${exit_:-none})"
	else echo "$name: cleanup_module at 0x$exit_, expected 0x248"; fail=1; fi
else echo "$name: init_module at 0x$init, expected 0x140 (wrong tree?)"; fail=1; fi

tmp=$(mktemp); trap 'rm -f "$tmp"' EXIT
awk '{print $1}' "$here/data/stock_exports.txt" > "$tmp"
for f in "$@"; do awk '{print ($1 ~ /^0x/) ? $2 : $1}' "$f" >> "$tmp"; done
missing=$("${P}nm" -u "$ko" | awk '{print $2}' | while read -r s; do grep -qxF "$s" "$tmp" || echo "$s"; done)
if [ -z "$missing" ]; then echo "$name: all symbols resolve"
else echo "$name: NOT exported by the stock kernel / modules:"; echo "$missing" | sed 's/^/    /'; fail=1; fi
exit $fail
