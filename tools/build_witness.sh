#!/bin/bash
# usage: build_witness.sh <kernel-tree> <outdir>
set -e
T=$1; O=$2; rm -rf $O; mkdir -p $O
mk() { # group-dir, objs..., files...
  local g=$1; shift; mkdir -p $O/$g; echo "obj-m := $1" > $O/$g/Makefile; shift
  for f in "$@"; do cp $T/$f $O/$g/; done
}
mk net "tun.o ifb.o" drivers/net/tun.c drivers/net/ifb.c
S="act_mirred act_skbedit act_gact act_csum act_pedit act_simple act_police act_vlan act_skbmod act_sample sch_htb sch_prio sch_sfq sch_tbf sch_ingress cls_u32 cls_fw cls_flow cls_matchall cls_route cls_flower em_meta"
mk sched "$(for s in $S; do echo -n "$s.o "; done)" $(for s in $S; do echo net/sched/$s.c; done)
cp $T/net/sched/cls_route.c $O/sched/ 2>/dev/null || true
mk usbser "pl2303.o" drivers/usb/serial/pl2303.c drivers/usb/serial/pl2303.h 2>/dev/null || mk usbser "pl2303.o" drivers/usb/serial/pl2303.c
mk usbcls "usblp.o" drivers/usb/class/usblp.c
mkdir -p $O/uas; echo "obj-m := uas.o" > $O/uas/Makefile; cp $T/drivers/usb/storage/uas.c $T/drivers/usb/storage/uas-detect.h $T/drivers/usb/storage/unusual_uas.h $O/uas/ 2>/dev/null; cp $T/drivers/usb/storage/*.h $O/uas/ 2>/dev/null || true
for g in net sched usbser usbcls uas; do
  make -C $T ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- M=$O/$g modules -j8 >$O/$g/build.log 2>&1 || echo "BUILD FAIL $g (see $O/$g/build.log)"
done
