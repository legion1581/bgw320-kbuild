#!/usr/bin/env python3
"""kodis.py <ko> <func>... : disassemble functions of an aarch64 .ko, annotating relocations (call targets, data refs).
A function name may be a prefix ending in '*' (all matching symbols). Needs pyelftools + capstone."""
import sys
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

f = open(sys.argv[1], 'rb'); e = ELFFile(f); st = e.get_section_by_name('.symtab')
md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
syms = {s.name: s for s in st.iter_symbols() if s['st_info']['type'] == 'STT_FUNC' and s['st_size']}
rel = {}
for sec in e.iter_sections():
    if isinstance(sec, RelocationSection) and sec.name.startswith('.rela.text'):
        tgt = e.get_section(sec['sh_info']).name
        for r in sec.iter_relocations():
            s = st.get_symbol(r['r_info_sym']); n = s.name or e.get_section(s['st_shndx']).name
            rel[(tgt, r['r_offset'])] = '%s+0x%x' % (n, r['r_addend']) if r['r_addend'] else n
names = []
for a in sys.argv[2:]:
    names += [n for n in syms if n.startswith(a[:-1])] if a.endswith('*') else [a]
for fn in names:
    s = syms[fn]; sec = e.get_section(s['st_shndx']); print('=====', fn, 'size', hex(s['st_size']))
    d = sec.data()[s['st_value']:s['st_value'] + s['st_size']]
    for i in md.disasm(d, s['st_value']):
        print(f"{i.address:6x} {i.mnemonic:7s} {i.op_str:40s} {rel.get((sec.name, i.address), '')}")
