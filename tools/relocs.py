import sys
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
def objrel(path, objs):
    e=ELFFile(open(path,'rb')); st=e.get_section_by_name('.symtab')
    syms={s.name:s for s in st.iter_symbols()}
    out={}
    for o in objs:
        s=syms.get(o)
        if not s: continue
        secidx=s['st_shndx']; lo=s['st_value']; hi=lo+s['st_size']
        lst=[]
        for sec in e.iter_sections():
            if isinstance(sec,RelocationSection) and sec['sh_info']==secidx:
                for r in sec.iter_relocations():
                    if lo<=r['r_offset']<hi:
                        t=st.get_symbol(r['r_info_sym'])
                        name=t.name or e.get_section(t['st_shndx']).name
                        lst.append((r['r_offset']-lo, name if not name.startswith('.') else name+'+'+hex(r['r_addend'])))
        out[o]=sorted(lst)
    return out
a=objrel(sys.argv[1],sys.argv[3:]); b=objrel(sys.argv[2],sys.argv[3:])
for o in sys.argv[3:]:
    A=dict(a.get(o,[])); B=dict(b.get(o,[]))
    same=sum(1 for k in A if k in B and (A[k]==B[k] or A[k].startswith('.')))
    print(o, "stock-relocs=%d built-relocs=%d same-offset=%d" % (len(A),len(B),same))
    for k in sorted(set(A)|set(B)):
        if A.get(k)!=B.get(k) and not (str(A.get(k)).startswith('.') and str(B.get(k)).startswith('.')): print("   %#x stock=%s built=%s"%(k,A.get(k),B.get(k)))
