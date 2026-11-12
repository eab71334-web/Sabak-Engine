#!/usr/bin/env python3
"""Pack MakeHuman (CC0) base mesh, skeleton, weights and targets into one compact file: assets/human.bin
usage: mh2bin.py <makehuman/makehuman/data> <out.bin>"""
import sys,os,json,struct,glob
D=sys.argv[1];OUT=sys.argv[2]
def tgt(path):
    r=[]
    for l in open(path):
        l=l.strip()
        if not l or l[0]=='#':continue
        p=l.split()
        if len(p)>=4:r.append((int(p[0]),float(p[1]),float(p[2]),float(p[3])))
    return r
# ---- mesh
verts=[];uvs=[];faces=[];grp=None;body=False
for l in open(D+'/3dobjs/base.obj'):
    if l.startswith('v '):verts.append(tuple(map(float,l.split()[1:4])))
    elif l.startswith('vt '):uvs.append(tuple(map(float,l.split()[1:3])))
    elif l.startswith('g '):body=l.split()[1]=='body'
    elif l.startswith('f ') and body:faces.append([tuple(int(x)-1 for x in t.split('/')[:2]) for t in l.split()[1:]])
nV=len(verts)
out={};outl=[];idx=[]
for f in faces:
    ids=[]
    for (v,t) in f:
        k=(v,t)
        if k not in out:out[k]=len(outl);outl.append(k)
        ids.append(out[k])
    for i in range(1,len(ids)-1):idx+= [ids[0],ids[i],ids[i+1]]
# ---- skeleton
sk=json.load(open(D+'/rigs/default.mhskel'));W=json.load(open(D+'/rigs/default_weights.mhw'))['weights'];B=sk['bones'];J=sk['joints']
drop=('tongue','levator','oculi','orbicularis','oris','risorius','special','temporalis','metacarpal','toe','breast','spine02','spine04','neck02','neck03','upperarm02','lowerarm02','upperleg02','lowerleg02','shoulder01')
def keep(n):
    if n.startswith(drop):return False
    if n.startswith('pelvis.'):return False
    return True
def anc(n):
    while n and not keep(n):n=B[n]['parent']
    return n
bones=[n for n in B if keep(n)]
# order: parents first
order=[];seen=set()
def add(n):
    if n in seen:return
    p=B[n]['parent']
    while p and p not in bones:p=B[p]['parent']
    if p:add(p)
    seen.add(n);order.append(n)
for n in bones:add(n)
bi={n:i for i,n in enumerate(order)}
def par(n):
    p=B[n]['parent']
    while p and p not in bi:p=B[p]['parent']
    return bi[p] if p else -1
wv=[{} for _ in range(13380)]
for bn,lst in W.items():
    t=anc(bn)
    if not t:t='root'
    for v,w in lst:
        if v<13380:wv[v][bi[t]]=wv[v].get(bi[t],0)+w
# ---- targets
T=[]
def addT(name,path):
    if os.path.exists(path):T.append((name,tgt(path)))
for g in ['female','male']:
    for a in ['young','old']:
        for m in ['minmuscle','averagemuscle','maxmuscle']:
            for w in ['minweight','averageweight','maxweight']:
                addT(f'u-{g}-{a}-{m}-{w}',f'{D}/targets/macrodetails/universal-{g}-{a}-{m}-{w}.target')
for r in ['african','asian','caucasian']:
    for g in ['female','male']:
        for a in ['young','old']:addT(f'r-{r}-{g}-{a}',f'{D}/targets/macrodetails/{r}-{g}-{a}.target')
for d in ['head','forehead','eyes','nose','mouth','ears','chin','cheek','neck','torso','hip','stomach','buttocks']:
    for p in sorted(glob.glob(f'{D}/targets/{d}/*.target')):addT(d+'/'+os.path.basename(p)[:-7],p)
for p in sorted(glob.glob(f'{D}/targets/expression/units/caucasian/*.target')):addT('x/'+os.path.basename(p)[:-7],p)
# ---- write
b=bytearray()
def U(*a):
    for x in a:b.extend(struct.pack('<I',x))
def S(s):
    e=s.encode();b.append(len(e));b.extend(e)
b.extend(b'SKHM');U(1,nV)
for v in verts:b.extend(struct.pack('<3f',*v))
U(len(outl))
for (v,t) in outl:b.extend(struct.pack('<Iff',v,*uvs[t]))
U(len(idx));b.extend(struct.pack(f'<{len(idx)}I',*idx))
U(len(order))
for n in order:
    S(n);b.extend(struct.pack('<h',par(n)))
    h=J[B[n]['head']];t=J[B[n]['tail']]
    U(len(h));b.extend(struct.pack(f'<{len(h)}I',*h));U(len(t));b.extend(struct.pack(f'<{len(t)}I',*t))
for v in range(13380):
    items=sorted(wv[v].items(),key=lambda x:-x[1])[:4];s=sum(w for _,w in items) or 1
    ws=[round(255*w/s) for _,w in items];ws+=[0]*(4-len(ws))
    if sum(ws)!=255 and ws:ws[0]+=255-sum(ws)
    js=[j for j,_ in items]+[0]*(4-len(items))
    b.extend(bytes(js));b.extend(bytes(ws))
U(len(T))
tot=0
for name,e in T:
    S(name);sc=max([max(abs(x),abs(y),abs(z)) for _,x,y,z in e] or [1e-6]) or 1e-6
    U(len(e));b.extend(struct.pack('<f',sc));tot+=len(e)
    for i,x,y,z in e:b.extend(struct.pack('<Hhhh',i,round(x/sc*32767),round(y/sc*32767),round(z/sc*32767)))
open(OUT,'wb').write(b)
print('bones',len(order),'targets',len(T),'entries',tot,'bytes',len(b),'outverts',len(outl))
