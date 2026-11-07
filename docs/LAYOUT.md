# What Broadcom changed in the BGW320 kernel (and how it was measured)

The BGW320-500's stock kernel (AT&T firmware 6.35.8, `uname -r` = `5.15.167`) comes without source and
without headers. Everything below was measured from the binaries: the kernel `Image` and the stock modules
(`.ko`) of the firmware. `final_layout.txt` has the full field list.

## The base is Android Common Kernel, not upstream Linux

The base is **ACK `android13-5.15`**, tag `android13-5.15-2024-11_r1`
(<https://android.googlesource.com/kernel/common>), merged to 5.15.167, plus Broadcom's `BCM_KF` patches.

The evidence:
- The stock kernel config has `CONFIG_RCU_BOOT_END_DELAY`, `CONFIG_DM_USER` and
  `CONFIG_ARCH_SUPPORTS_SPECULATIVE_PAGE_FAULT`. All three exist only in ACK, and android14 markers are absent.
- `struct module` is 8 bytes longer before `init` than in upstream: init is at `0x140`, exit at `0x248`, against
  upstream's `0x138` / `0x240`. Those 8 bytes are ACK's `const char *scmversion`. On a plain upstream tree they
  need a hack (`patches/upstream-only/module-h-bgw-pad.patch`); on the ACK tree they come out right by themselves.
- The kernel exports `del_timer_sync`, not `timer_delete_sync`. In upstream 5.15.167 `del_timer_sync()` is an inline wrapper around `timer_delete_sync`, so a usbnet built there asks for a symbol the stock kernel lacks and does not load;
  one built on ACK does.

## net_device: 1984 -> 2240 bytes (`netdev_priv()` +256)

- `CONFIG_BCM_KF_EXTSTATS`: 48 bytes (6 `unsigned long`) appended to `net_device_stats`, so `stats` (at `0x128`)
  grows to 232 bytes.
- `CONFIG_BCM_KF_NETDEV_EXT`: a 184-byte `struct bcm_netdev_ext` at `0x210`, right after the stats. It holds the
  device id, `iff_flags`, the netdev path, Blog statistics, xmit arguments, the MAC limit and callbacks.
- Everything after it moves. `_tx` is cache-line aligned, so from `0x380` up every field is +256, and so is
  `netdev_priv()`.
- The sizes of `net_device_ops`, `ethtool_ops` and `usb_driver`, and every function-pointer slot in them, match
  the stock kernel.

## sk_buff: fully replaced, 576 bytes

Broadcom replaces it (`CONFIG_BCM_KF_SKB_EXT`, `BCM_KF_NBUFF`):

| field | offset in the stock layout |
|---|---|
| embedded `fkbuff` block | `0x40` (data `0x50`, len `0x58`, mark/queue `0x60`, recycle hook `0x70`) |
| `bcm_ext` (176 bytes) | `0x80` |
| `data_len` | `0x150` |
| `cb` (64 bytes, not 48) | `0x198` |
| `end` / `head` / `truesize` | `0x1e0` / `0x1e8` / `0x1f0` |

`skb_shared_info` has a `__u8 *dirty_p` added at the front, so everything in it is +8 (`nr_frags` at `0x10`,
`frags` at `0x38`).

## gnet_stats_queue: a 64-bit counter at +8

Qdisc statistics: `qlen`, `backlog`, then a `u64`, then `drops` at `0x10`, `requeues` at `0x14` and
`overlimits` at `0x18`; the size is `0x20`. It was read from the stock `__gnet_stats_copy_queue` and
`qdisc_tree_reduce_backlog`. So `qstats` sits at `0xc8` and `qdisc_priv()` at `0x180` in `struct Qdisc`.
Patch 0003 is needed only for qdisc modules (fq_codel, cake).

## How the offsets were found

1. **Rebuild a stock module from source and diff it.** `tools/build_witness.sh` builds modules that also exist in
   the stock firmware (tun, ifb, sch_*, act_*, cls_*, usblp, uas, ...) from the reconstructed tree.
2. **Compare struct offsets per function.** `tools/compare.py stock.ko built.ko -v` disassembles both, collects
   the load/store displacements and `add` immediates of every common function, and prints where they disagree.
   An offset that is +256 in the stock module and appears in every `net_device` access is a layout change, not a
   compiler difference.
3. **Read the stock code where it touches the struct.** `tools/kodis.py` disassembles a function of a `.ko` with
   its relocations resolved; `tools/relocs.py` compares relocations per object; `tools/fields.py` (a gdb script)
   dumps the layout of the rebuilt tree for comparison.
4. **Patch the header, rebuild, compare again.** The result is in patches 0002 and 0003.

Measured improvement: `tun.ko` went from 28 to 59 of 72 functions with identical offsets; over 27 modules, the
offset match rose from 72.3 % to 86.8 %. The rest are compiler differences (the stock kernel was built with
GCC 13.2 from Buildroot) and traffic-control structs that only matter for qdiscs (patch 0003 covers the one that
was needed).

## What is not known

- A few `bcm_ext` fields inside `sk_buff` are named by guess; their positions and sizes are pinned. Two of them
  are not identified: 8 bytes at `0x140` and a `u32` at `0x128`. Generic drivers do not touch them.
- `struct nf_conn` has Broadcom fields after the upstream ones (for example the DPI block at `ct + 312`). The
  headers here do not describe them; `modules/dpidbg` reads them by offset.
- `CONFIG_BCM_KF_USBNET` / `BCM_USBNET_ACCELERATION`: the stock tree has flow-cache hooks in usbnet that our build
  lacks. Tethering works without them.
