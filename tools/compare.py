#!/usr/bin/env python3
"""Compare struct-offset immediates per function between two aarch64 .ko files."""
import sys, collections
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
from capstone.arm64 import ARM64_OP_MEM, ARM64_OP_IMM, ARM64_REG_SP, ARM64_REG_X29

md = Cs(CS_ARCH_ARM64, CS_MODE_ARM); md.detail = True

def funcs(path):
    f = open(path, 'rb'); e = ELFFile(f)
    st = e.get_section_by_name('.symtab')
    out = {}
    for s in st.iter_symbols():
        if s['st_info']['type'] != 'STT_FUNC' or s['st_size'] == 0: continue
        sec = e.get_section(s['st_shndx'])
        data = sec.data()[s['st_value']: s['st_value'] + s['st_size']]
        imms = []
        for ins in md.disasm(data, s['st_value']):
            m = ins.mnemonic
            for op in ins.operands:
                if op.type == ARM64_OP_MEM and op.mem.base not in (ARM64_REG_SP, ARM64_REG_X29) and op.mem.disp > 0:
                    imms.append(op.mem.disp)
            if m in ('add',) and len(ins.operands) == 3 and ins.operands[2].type == ARM64_OP_IMM:
                if ins.operands[1].reg not in (ARM64_REG_SP, ARM64_REG_X29) and ins.operands[2].imm > 0:
                    sh = ins.operands[2].shift.value if ins.operands[2].shift.type else 0
                    imms.append(ins.operands[2].imm << sh)
        out[s.name] = collections.Counter(imms)
    return out

def score(a, b):
    m = sum((a & b).values()); n = max(sum(a.values()), sum(b.values()), 1)
    return m, n

if __name__ == '__main__':
    A = funcs(sys.argv[1]); B = funcs(sys.argv[2]); verbose = '-v' in sys.argv
    tm = tn = exact = 0; common = sorted(set(A) & set(B))
    for name in common:
        m, n = score(A[name], B[name]); tm += m; tn += n
        if A[name] == B[name]: exact += 1
        if verbose and A[name] != B[name]:
            onlyA = sorted((A[name] - B[name]).elements()); onlyB = sorted((B[name] - A[name]).elements())
            print("%-40s %3d/%3d  stock-only=%s  built-only=%s" % (name, m, n, [hex(x) for x in onlyA], [hex(x) for x in onlyB]))
    print("functions common=%d exact-imm-multiset=%d  imm-match=%d/%d (%.1f%%)" % (len(common), exact, tm, tn, 100.0*tm/max(tn,1)))
