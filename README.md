# bgw320-kbuild

Build loadable kernel modules for the **AT&T BGW320-500**'s stock kernel: **Linux 5.15.167** from AT&T
firmware **6.35.8**, which ships without source or headers.

The stock kernel is enough to run OpenWrt on the BGW320 (see the article series below). But it has no USB
networking, no LTE modem drivers and no fq_codel. A module built against plain upstream 5.15.167 either refuses
to load or calls the wrong function pointer as its init. This repository rebuilds a kernel tree that matches the
stock one where modules care:

- **The right base:** the stock kernel is Google's **Android Common Kernel** (`android13-5.15`), not upstream Linux.
- **Broadcom's struct changes:** `net_device` is 256 bytes bigger, `sk_buff` is a different 576-byte struct, and
  qdisc statistics have an extra counter. All of it was measured from the stock binaries ([docs/LAYOUT.md](docs/LAYOUT.md)).
- **The stock kernel config.**

Modules built this way load on the stock kernel and match the stock modules' struct offsets. The modules here
that run on the router come out **byte-identical** (code and data) to the builds that have been running there
since October 2026.

> 👉 The story behind it: [OpenWrt on BGW320-500 — Modules for a Kernel Nobody Gave You](https://robolegion.com/openwrt-bgw320-kernel-modules/)

## What's inside

| path | what |
|---|---|
| `make-ack-tree.sh` | clones ACK `android13-5.15-2024-11_r1`, applies the patches and the stock config, runs `modules_prepare` |
| `build-module.sh` | builds a module directory against that tree and checks every `.ko` |
| `patches/0001` | stubs for Broadcom's kernel Kconfig symbols (374 `BCM_*`) and the Broadcom kernel headers the stock config needs |
| `patches/0002` | the stock `net_device` and `sk_buff` layout |
| `patches/0003` | the stock `gnet_stats_queue` layout (qdisc modules only) |
| `patches/upstream-only/` | the `struct module` pad for a plain upstream tree (not needed on ACK; kept for reference) |
| `config/` | the stock kernel configuration |
| `data/stock_exports.txt` | every symbol the stock kernel and its stock modules export (used by the checker) |
| `tools/check-module.sh` | vermagic, `struct module` layout and symbol checks for a `.ko` |
| `tools/` (Python) | the layout tools: offset comparison, disassembly with relocations, export extraction |
| `openwrt/mkstub-kmod.sh` | makes empty `kmod-*` packages so OpenWrt's `opkg` accepts packages that depend on kernel modules |
| `modules/` | the modules below |

## Modules

| directory | what | on the router |
|---|---|---|
| `hello` | prints `sizeof(net_device)` and `sizeof(sk_buff)`: the first test of a new tree | build check |
| `usbnet` | USB tethering: `mii`, `usbnet`, `cdc_ether`, `rndis_host`, `cdc_ncm`, `ipheth` with the CDC NCM mode backported (current iPhones) | ✅ iPhone tethering (Android RNDIS / NCM drivers load, not tested with a phone) |
| `lte` | LTE / 5G modems: `option`, `qcserial`, `sierra`, `usb_wwan`, `cdc-acm` (on the stock `usbserial`), `cdc-wdm`, `qmi_wwan`, `cdc_mbim`, `huawei_cdc_ncm` (on `usbnet`) | ✅ loaded; a Quectel EC20 binds (AT ports + QMI), no data test yet (no SIM) |
| `xt_statistic` | the iptables `statistic` match (mwan3 load balancing) | ✅ |
| `slicx` | register and RAM access to the phone line's Si32291 SLIC through Broadcom's `slicslac` driver (tones, DTMF) | ✅ |
| `nsfix` | `nfct_nsfix`: makes network namespaces (containers) possible; Broadcom's conntrack sysctls crashed the box | ✅ |
| `warnfix` | `bgw_warnfix`: one-instruction runtime patch so a kernel `WARN()` logs instead of resetting the box; `test/` triggers a warning | ✅ |
| `sqm` | `sch_fq_codel`, `sch_cake` | ✅ loaded and tested, not used (they need the hardware acceleration off) |
| `dpidbg` | logs Broadcom flow-cache events with the DPI state of each connection (a debugging tool) | ✅ bench only |
| `probes/ndoff`, `probes/phyprobe` | read-only probes: `net_device` offsets and flags, clause-45 PHY registers | ✅ bench only |

## Quick start

On an x86-64 Linux PC (tested on Ubuntu 22.04):

```bash
sudo apt install git make gcc gcc-aarch64-linux-gnu flex bison bc libssl-dev libelf-dev
pip install pyelftools capstone          # only for the layout tools

git clone https://github.com/legion1581/bgw320-kbuild && cd bgw320-kbuild
./make-ack-tree.sh                       # about 1.5 GB download (shallow clone)
./build-module.sh modules/hello
```

Expected:

```
>> kernelrelease: 5.15.167
...
hello.ko: vermagic ok
hello.ko: struct module ok (init@0x140, exit@0x248)
hello.ko: all symbols resolve
```

On the router:

```bash
scp -O modules/hello/hello.ko root@192.168.1.1:/tmp/   # your router's address
ssh root@192.168.1.1 'insmod /tmp/hello.ko; dmesg | tail -1; rmmod hello'
# hello: BGW320 module loaded, sizeof(net_device)=2240 sizeof(sk_buff)=576
```

Build your own module the same way: a directory with the source and a one-line `Makefile`
(`obj-m := mymod.o`), then `./build-module.sh path/to/dir`. Modules that use another module's symbols
(like `lte` on `usbnet`) need that one built first.

## Checks before you insmod

The stock kernel has **no `MODVERSIONS` and no module signing**. It loads anything with the right vermagic,
even a module whose struct offsets are wrong, so nothing stops a bad module before it crashes the box.
`tools/check-module.sh` (run by `build-module.sh`) is the safety net:

1. **vermagic** must be exactly `5.15.167 SMP preempt mod_unload aarch64`.
2. **struct module layout**: `init_module` must be at offset `0x140` and `cleanup_module` at `0x248` in
   `.gnu.linkonce.this_module`. On the wrong tree they land at `0x138` / `0x240`, and the kernel calls a wrong
   pointer as init.
3. **Symbols**: every undefined symbol must be exported by the stock kernel or a stock module
   (`data/stock_exports.txt`). Many kernel helpers are built in but not exported. Data symbols do not appear in
   `/proc/kallsyms` (text only).

Load a new module **with a serial console attached** the first time. A bad struct offset shows up as an oops,
and on this kernel an oops can mean a watchdog reset.

## Loading modules on OpenWrt

- OpenWrt's `kmodloader` (`/etc/modules.d`) only finds modules at the top level of `/lib/modules/5.15.167/`.
  Modules in subdirectories (like the stock `kernel/...` tree) must be loaded with `insmod` from a boot script,
  in dependency order: `mii`, then `usbnet`, then `cdc_ether` / `cdc_ncm` / `rndis_host` / `ipheth`.
- OpenWrt's `kmod-*` packages are built for its own kernel and cannot be installed. If a userspace package
  depends on one, make an empty stand-in package instead. For example, kmod-usb-net with an autoload file:

  ```bash
  openwrt/mkstub-kmod.sh /tmp kmod-usb-net "USB network (built here)" 60-usb-net usbnet
  ```

## What is not here

- **No stock binaries:** not the kernel `Image`, not Broadcom's modules, not firmware. To compare layouts against
  your own router, use the stock firmware from your own BGW320.
- `config/` is the stock kernel's configuration as shipped; it contains no device-specific data.
- The Broadcom kernel headers in patch 0001 are licensed GPL-2.0 by Broadcom; their license headers are kept.

## License

GPL-2.0 (see `LICENSE`), like the kernel code it builds on. The ACK sources come from Google's public repository
under their own (GPL-2.0) licence.
