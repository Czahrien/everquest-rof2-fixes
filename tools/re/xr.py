import pefile, capstone, sys, pickle, os, collections
from capstone import x86
def load(f):
    pk=f+'.idx'
    pe=pefile.PE(f)
    base=pe.OPTIONAL_HEADER.ImageBase; img=pe.get_memory_mapped_image()
    imps={}
    for e in pe.DIRECTORY_ENTRY_IMPORT:
        for i in e.imports: imps[i.address]=(e.dll.decode()+'!'+(i.name.decode() if i.name else str(i.ordinal)))
    if os.path.exists(pk): refs=pickle.load(open(pk,'rb'))
    else:
        md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail=True; md.skipdata=True
        refs=collections.defaultdict(list)
        s=pe.sections[0]; start=base+s.VirtualAddress
        code=img[s.VirtualAddress:s.VirtualAddress+s.Misc_VirtualSize]
        for i in md.disasm(code,start):
            if i.id==0: continue
            for op in i.operands:
                v=None
                if op.type==x86.X86_OP_IMM: v=op.imm & 0xffffffff
                elif op.type==x86.X86_OP_MEM and op.mem.base==0: v=op.mem.disp & 0xffffffff
                if v is not None and base<=v<base+len(img): refs[v].append(i.address)
        refs=dict(refs); pickle.dump(refs,open(pk,'wb'))
    return pe,base,img,imps,refs
def dis(img,base,a,n=0x80,imps={}):
    md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    out=[]
    for i in md.disasm(img[a-base:a-base+n],a):
        c=''
        for k,v in imps.items():
            if f'{k:#x}' in i.op_str: c='  ; '+v
        out.append(f"{i.address:08x}: {i.mnemonic} {i.op_str}{c}")
    return '\n'.join(out)
def funcstart(img,base,a):
    # walk back to int3/ret padding
    o=a-base
    while o>0:
        if img[o-1] in (0xcc,) and img[o-2]==0xcc: return base+o
        if img[o-1]==0xc3 and img[o-2] in (0xcc,0x5d,0x5b,0x5e,0x5f) : pass
        o-=1
    return base
