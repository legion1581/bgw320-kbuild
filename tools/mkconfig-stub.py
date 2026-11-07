import sys,re
known=set(open(sys.argv[2]).read().split())
out=[]
for line in open(sys.argv[1]):
    m=re.match(r'CONFIG_([A-Z0-9_]+)=(.*)',line.strip())
    if not m: continue
    n,v=m.groups()
    if n in known: continue
    if v in('y',): t='bool'; d='y'
    elif v=='m': t='tristate'; d='m'
    elif v.startswith('"'): t='string'; d=v
    elif v.startswith('0x'): t='hex'; d=v
    elif re.match(r'-?\d+$',v): t='int'; d=v
    else: continue
    out.append('config %s\n\t%s "%s"\n\tdefault %s\n'%(n,t,n,d))
open(sys.argv[3],'w').write('menu "BCM stub symbols (layout reconstruction)"\n'+'\n'.join(out)+'endmenu\n')
print(len(out))
