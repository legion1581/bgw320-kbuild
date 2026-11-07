#!/usr/bin/env python3
"""Exported symbol names of the stock BGW320 kernel + its modules.

Kernel: the raw arm64 Image (not compressed). Export tables are arrays of PREL32 entries
{int value_off; int name_off; int ns_off} whose name points into the __ksymtab_strings block. Two passes:
  1. entries whose name starts right after a NUL (a plain string), and
  2. entries that sit in a run of consecutive 12-byte entries (the tables are contiguous arrays). Pass 2 is needed
     because the linker merges string tails: "_printk" is stored as the end of "__warn_printk", so it has no NUL
     before it and pass 1 alone misses it (and strcmp, init_net, ...).
Modules: the names in each .ko's __ksymtab_strings section.
Output: "<name> <owner>" (owner = vmlinux or the module file name).
Usage: stock_exports.py <Image> [<stock modules dir>...] > data/stock_exports.txt
"""
import sys, struct, re, os, subprocess

img = open(sys.argv[1], 'rb').read()
anchor = img.find(b'\x00alloc_netdev_mqs\x00')          # any well-known export inside __ksymtab_strings
if anchor < 0:
    sys.exit('no __ksymtab_strings found (is this the uncompressed Image?)')
lo = anchor
while lo > 0 and (32 <= img[lo-1] < 127 or img[lo-1] == 0): lo -= 1
hi = anchor
while hi < len(img) and (32 <= img[hi] < 127 or img[hi] == 0): hi += 1

ident = re.compile(rb'^[A-Za-z_][A-Za-z0-9_.$]*$')
plain, anyname = set(), {}
for i in range(0, len(img) - 12, 4):
    no = struct.unpack_from('<i', img, i + 4)[0]
    n = i + 4 + no
    if not (lo <= n < hi): continue
    e = img.find(b'\x00', n)
    s = img[n:e]
    if not (2 <= len(s) < 128 and ident.match(s)): continue
    anyname[i] = s.decode()
    if img[n-1] == 0: plain.add(s.decode())
inrun = {s for i, s in anyname.items() if (i - 12 in anyname) or (i + 12 in anyname)}
for n in sorted(plain | inrun): print(n, 'vmlinux')

for d in sys.argv[2:]:
    for root, _, fs in os.walk(d):
        for f in sorted(fs):
            if not f.endswith('.ko'): continue
            o = subprocess.run(['aarch64-linux-gnu-readelf', '-p', '__ksymtab_strings', os.path.join(root, f)],
                               capture_output=True, text=True).stdout
            for l in o.splitlines():
                m = re.match(r'\s*\[\s*[0-9a-f]+\]\s+(\S+)$', l)
                if m and ident.match(m.group(1).encode()): print(m.group(1), f)
