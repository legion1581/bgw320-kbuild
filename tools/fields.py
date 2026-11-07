import gdb
def dump(name):
    t = gdb.lookup_type(name)
    print("## %s size=%d" % (name, t.sizeof))
    def rec(t, base, prefix):
        for f in t.fields():
            off = base + (f.bitpos // 8 if hasattr(f,'bitpos') else 0)
            nm = f.name or "<anon>"
            ft = f.type.strip_typedefs()
            if f.bitsize:
                print("0x%04x %4d %s%s:%d" % (off, 0, prefix, nm, f.bitsize)); continue
            if f.name is None and ft.code in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION):
                rec(ft, off, prefix); continue
            print("0x%04x %4d %s%s" % (off, ft.sizeof, prefix, nm))
    rec(t, 0, "")
import os
for n in os.environ.get("STRUCTS","struct net_device").split(","):
    dump(n)
