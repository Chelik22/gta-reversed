#!/usr/bin/env python3
"""bin_diff.py -- compiled-code verification for gta_sa_reversed.

For every reversed function, compare OUR COMPILED MACHINE CODE (from
gta_reversed.asi, located via its PDB with dbghelp) against the ORIGINAL
gta_sa.exe binary at the known install address.

Scores per function:
  * mnemonic-sequence similarity (normalized opcode classes, difflib ratio)
  * immediate/float constant multiset overlap (Dice coefficient)
  * call-target set overlap in ORIGINAL address space (Jaccard). Our call
    targets are resolved to symbols via the ASI PDB, then mapped back to
    original addresses through the repo's RH_ScopedInstall source map.
  * branch / loop structure delta

Usage:
  python bin_diff.py dump-pdb                 # one-time full PDB symbol dump -> pdb_symbols.json
  python bin_diff.py scan-repo                # scan repo for enabled installs -> targets_repo.json
  python bin_diff.py run [--targets FILE] [--limit N] [--only SUBSTR]
                                              # default targets: final_push batches ∩ enabled installs

Outputs (next to this script): report.json, report.md, console table sorted worst-first.
"""
import bisect
import collections
import ctypes
import difflib
import json
import os
import re
import struct
import sys
import time
from ctypes import wintypes

HERE = os.path.dirname(os.path.abspath(__file__))

DEFAULTS = {
    "exe": r"D:\projects\keep\ghidra\gta_sa.exe",
    "asi": r"D:\projects\gta_sa_reversed\bin\Release\gta_reversed.asi",
    "pdb": r"D:\projects\gta_sa_reversed\bin\Release\gta_reversed.pdb",
    "repo": r"D:\projects\gta_sa_reversed",
    "batches": r"D:\projects\gta_sa_tester\final_push",
    "targets": os.path.join(HERE, "targets.json"),
    "pdb_cache": os.path.join(HERE, "pdb_symbols.json"),
}
ORIG_BASE = 0x400000          # gta_sa.exe preferred image base
ASI_DISASM_BASE = 0x10000000  # canonical base used to load the ASI into dbghelp
MAX_FN_BYTES = 4096           # cap on original-function window
RAW_MODE = False              # --raw: disable MSVC boilerplate stripping

# ----------------------------------------------------------------------------
# minimal PE reader
# ----------------------------------------------------------------------------
class PE:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        d = self.data
        if d[:2] != b"MZ":
            raise ValueError("%s: not MZ" % path)
        e_lfanew = struct.unpack_from("<I", d, 0x3C)[0]
        if d[e_lfanew:e_lfanew + 4] != b"PE\0\0":
            raise ValueError("%s: not PE" % path)
        nsec = struct.unpack_from("<H", d, e_lfanew + 6)[0]
        opt_size = struct.unpack_from("<H", d, e_lfanew + 20)[0]
        opt = e_lfanew + 24
        magic = struct.unpack_from("<H", d, opt)[0]
        if magic != 0x10B:
            raise ValueError("%s: not PE32" % path)
        self.imagebase = struct.unpack_from("<I", d, opt + 28)[0]
        self.sections = []
        sec = opt + opt_size
        for i in range(nsec):
            o = sec + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", d, o + 8)
            self.sections.append((name, vaddr, vsize, rawptr, rawsize))
        self.sec_starts = [s[1] for s in self.sections]

    def rva_to_off(self, rva):
        i = bisect.bisect_right(self.sec_starts, rva) - 1
        if i < 0:
            return None
        name, vaddr, vsize, rawptr, rawsize = self.sections[i]
        if vaddr <= rva < vaddr + max(vsize, rawsize):
            off = rawptr + (rva - vaddr)
            if off < len(self.data):
                return off
        return None

    def in_image(self, va):
        rva = va - self.imagebase
        return self.rva_to_off(rva) is not None

    def read_va(self, va, n):
        off = self.rva_to_off(va - self.imagebase)
        if off is None:
            return None
        return self.data[off:off + n]

# ----------------------------------------------------------------------------
# dbghelp: full PDB symbol dump (cached)
# ----------------------------------------------------------------------------
U64 = ctypes.c_uint64
k32 = ctypes.windll.kernel32
dbghelp = ctypes.WinDLL("dbghelp")

dbghelp.SymInitializeW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, wintypes.BOOL]
dbghelp.SymLoadModuleExW.argtypes = [wintypes.HANDLE, wintypes.HANDLE, wintypes.LPCWSTR,
                                     wintypes.LPCWSTR, U64, wintypes.DWORD, ctypes.c_void_p,
                                     wintypes.DWORD]
dbghelp.SymEnumSymbolsW.argtypes = [wintypes.HANDLE, U64, wintypes.LPCWSTR, ctypes.c_void_p,
                                    wintypes.LPVOID]
dbghelp.UnDecorateSymbolNameW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD,
                                          wintypes.DWORD]

NAME_BUF = 2048

class SYMBOL_INFOW(ctypes.Structure):
    _fields_ = [("SizeOfStruct", wintypes.ULONG), ("TypeIndex", wintypes.ULONG),
                ("Reserved", U64 * 2), ("Index", wintypes.ULONG), ("Size", wintypes.ULONG),
                ("ModBase", U64), ("Flags", wintypes.ULONG), ("Value", U64), ("Address", U64),
                ("Register", wintypes.ULONG), ("Scope", wintypes.ULONG), ("Tag", wintypes.ULONG),
                ("NameLen", wintypes.ULONG), ("MaxNameLen", wintypes.ULONG),
                ("Name", ctypes.c_wchar * NAME_BUF)]

SYM_TAG_FUNCTION = 5
SYM_TAG_PUBLIC = 3
UNDNAME_COMPLETE = 0

def _undecorate(decorated):
    buf = ctypes.create_unicode_buffer(8192)
    n = dbghelp.UnDecorateSymbolNameW(decorated, buf, 8192, UNDNAME_COMPLETE)
    if not n:
        return ""
    s = buf.value
    # "public: static void __cdecl CCam::LookBehind(int)" -> "CCam::LookBehind"
    for kw in ("public: ", "private: ", "protected: ", "static ", "virtual ",
               "__cdecl ", "__thiscall ", "__stdcall ", "__fastcall ", "__clrcall ",
               "class ", "struct ", "enum "):
        while s.startswith(kw):
            s = s[len(kw):]
    oparen = s.find("operator()")
    if oparen != -1:
        s = s[:oparen + len("operator()")]
    else:
        s = s.split("(")[0]
    return s.strip()

def dump_pdb(pdb_path=None, asi_path=None, cache_path=None):
    pdb_path = pdb_path or DEFAULTS["pdb"]
    asi_path = asi_path or DEFAULTS["asi"]
    cache_path = cache_path or DEFAULTS["pdb_cache"]
    st = os.stat(pdb_path)
    meta = {"pdb": pdb_path, "size": st.st_size, "mtime": st.st_mtime}

    if os.path.exists(cache_path):
        try:
            cached = json.load(open(cache_path, encoding="utf-8"))
            if cached.get("meta") == meta:
                print("[pdb] using cached dump: %d symbols (%s)"
                      % (len(cached["functions"]), cache_path))
                return cached
        except Exception as e:
            print("[pdb] cache unreadable (%s), redumping" % e)

    pdb_dir = os.path.dirname(pdb_path)
    h = k32.GetCurrentProcess()
    if not dbghelp.SymInitializeW(h, pdb_dir, False):
        print("[pdb] SymInitializeW failed err=%d" % k32.GetLastError())
        sys.exit(1)
    base = dbghelp.SymLoadModuleExW(h, None, asi_path, None, ASI_DISASM_BASE, 0, None, 0)
    if not base:
        print("[pdb] SymLoadModuleExW failed err=%d" % k32.GetLastError())
        sys.exit(1)
    print("[pdb] module base 0x%X" % base)

    SYMENUMPROCW = ctypes.WINFUNCTYPE(wintypes.BOOL, ctypes.c_void_p, wintypes.ULONG,
                                      wintypes.LPVOID)
    funcs = []
    count = [0]

    def on_sym(psi, size, ctx):
        si = ctypes.cast(psi, ctypes.POINTER(SYMBOL_INFOW)).contents
        count[0] += 1
        if count[0] % 50000 == 0:
            print("[pdb] scanned %d symbols, kept %d functions" % (count[0], len(funcs)))
        if si.Tag == SYM_TAG_FUNCTION and si.NameLen:
            name = si.Name[:si.NameLen]
            rva = si.Address - base
            if 0 < rva < 0x10000000:
                funcs.append({"r": rva, "s": int(si.Size), "n": name})
        return True

    cb = SYMENUMPROCW(on_sym)
    t0 = time.time()
    if not dbghelp.SymEnumSymbolsW(h, base, "*!*", cb, None):
        # some dbghelp versions want plain mask
        if not dbghelp.SymEnumSymbolsW(h, base, "*", cb, None):
            print("[pdb] SymEnumSymbolsW failed err=%d" % k32.GetLastError())
            sys.exit(1)
    print("[pdb] enumerated %d symbols in %.1fs, %d function symbols"
          % (count[0], time.time() - t0, len(funcs)))

    # undecorate (one-time cost) and keep plain names
    out = []
    for fn in funcs:
        fn["d"] = _undecorate(fn["n"])
        out.append(fn)
    res = {"meta": meta, "base": base, "functions": out}
    with open(cache_path, "w", encoding="utf-8") as f:
        json.dump(res, f)
    print("[pdb] cached %d functions -> %s" % (len(out), cache_path))
    return res

# ----------------------------------------------------------------------------
# repo scan: RH_Scoped*Install source map
# ----------------------------------------------------------------------------
RE_CTX_CLASS = re.compile(r"RH_Scoped(?:Class|VirtualClass)\((\w+)[,)]")
RE_CTX_NSNAME = re.compile(r'RH_ScopedNamespaceName\("([^"]+)"\)')
RE_CTX_NS = re.compile(r"RH_ScopedNamespace(?:Name)?\((\w+)\)")
RE_INSTALL = re.compile(r"RH_Scoped(\w*)Install\(([^;]*)\);")
RE_ADDR = re.compile(r"0x[0-9A-Fa-f]{6,8}")
RE_NAME = re.compile(r"^\s*([A-Za-z_~][^,]*?)\s*,")
RE_STR = re.compile(r'"([^"]*)"')

def scan_repo(repo=None):
    """-> installs: list of dicts {addr, qual, name, key, enabled, file}"""
    repo = repo or DEFAULTS["repo"]
    installs = []
    src = os.path.join(repo, "source")
    for root, _, fns in os.walk(src):
        for fn in fns:
            if not fn.endswith(".cpp"):
                continue
            path = os.path.join(root, fn)
            prefix = ""
            ns = ""
            try:
                lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
            except OSError:
                continue
            for i, line in enumerate(lines):
                m = RE_CTX_CLASS.search(line)
                if m:
                    prefix = m.group(1) + "::"
                    ns = ""
                    continue
                # RH_ScopedCategory(...) / RH_ScopedCategoryGlobal(): UI grouping only,
                # does not affect symbol names -> ignored
                m = RE_CTX_NSNAME.search(line) or RE_CTX_NS.search(line)
                if m:
                    ns = m.group(1) + "::"
                    prefix = ""
                    continue
                m = RE_INSTALL.search(line)
                if not m:
                    continue
                qual, args = m.group(1), m.group(2)
                am = RE_ADDR.search(args)
                if not am:
                    continue
                addr = int(am.group(0), 16)
                nm = RE_NAME.match(args)
                if not nm:
                    continue
                name = nm.group(1).strip()
                name = re.sub(r"\s+", "", name)
                if qual.startswith(("Global", "Named")):
                    full = ns + name          # global-scope install (ns may add a namespace)
                elif prefix:
                    full = prefix + name      # class-scoped install
                else:
                    full = ns + name          # namespace-scoped (e.g. VideoPlayer::Shutdown)
                enabled = "reversed = false" not in args and "reversed=false" not in args
                installs.append({"addr": addr, "qual": qual, "name": full,
                                 "enabled": enabled, "file": path, "line": i + 1})
    return installs

def build_target_list_from_batches(batches_dir=None, repo=None):
    """final_push batch entries intersected with currently-enabled installs."""
    batches_dir = batches_dir or DEFAULTS["batches"]
    import glob
    installs = scan_repo(repo)
    by_addr = {}
    for ins in installs:
        by_addr.setdefault(ins["addr"], ins)
    targets = []
    seen = set()
    for bf in sorted(glob.glob(os.path.join(batches_dir, "batch_*.json"))):
        for e in json.load(open(bf, encoding="utf-8")):
            addr = int(e["addr"], 16)
            if addr in seen:
                continue
            ins = by_addr.get(addr)
            if ins is None or not ins["enabled"]:
                continue
            seen.add(addr)
            name = "%s::%s" % (e.get("class", ""), e["fn"].rstrip("-"))
            targets.append({"name": name, "orig_addr": addr, "key": e.get("key", name)})
    return targets

# ----------------------------------------------------------------------------
# disassembly + normalization
# ----------------------------------------------------------------------------
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_MEM

JCC = {"ja", "jae", "jb", "jbe", "je", "jg", "jge", "jl", "jle", "jne", "jno", "jns",
       "jo", "jp", "jnp", "js", "jcxz", "jecxz", "jrcxz"}

def op_class(insn):
    m = insn.mnemonic
    if m in JCC:
        return "jcc"
    if m.startswith("cmov"):
        return "cmov"
    if m.startswith("rep") or m.startswith("lock"):
        return m.split(None, 1)[0] + ":" + op_class_verb(m)
    return m

def op_class_verb(m):
    parts = m.split()
    return parts[-1] if parts else m

FLOATISH = set("""
fld flds fldl fild fst fsts fstl fstp fstps fstpl fist fistp fistps fistpll
fadd fadds faddp faddl fsub fsubs fsubp fsubr fsubrs fmul fmuls fmulp fmull
fdiv fdivs fdivp fdivr fdivrs fcomp fcomps fcom fcoms fcomm fucomp fucomps
fiadd fimul fisub fisubr fidiv fidivr fabs fchs fsqrt
movss movsd movaps movapd movups movupd movlps movlpd movhps movhpd
addss addsd subss subsd mulss mulsd divss divsd
comiss comisd ucomiss ucomisd maxss maxsd minss minsd sqrtss sqrtsd
cvtsi2ss cvtsi2sd cvtss2si cvtsd2si cvtss2sd cvtsd2ss cvttss2si cvttsd2si
""".split())

def is_floatish(m):
    if m in FLOATISH:
        return True
    return m.startswith("fld") or m.startswith("fst") or m.startswith("fi")

def disasm(pe, va, end_va, stop_at_boundary=True):
    """Linear disassembly [va, end_va). Stops at int3, invalid bytes, or a direct
    unconditional jmp whose target leaves the window (tail call / jump-over)."""
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    off = pe.rva_to_off(va - pe.imagebase)
    if off is None:
        return []
    n = min(end_va - va, MAX_FN_BYTES)
    code = pe.data[off:off + n]
    out = []
    for insn in md.disasm(code, va):
        if insn.mnemonic == "int3" or insn.bytes[0] == 0xCC:
            break
        out.append(insn)
        end = insn.address + insn.size
        if end >= end_va:
            break
        if insn.mnemonic == "jmp" and insn.operands and insn.operands[0].type == X86_OP_IMM:
            tgt = insn.operands[0].imm
            if tgt < va or tgt >= end_va:
                break
    return out

def normalize_seq(insns):
    return [op_class(i) for i in insns]

def strip_boilerplate(insns):
    """Remove unambiguous MSVC build-config boilerplate (not translation logic):
    frame-pointer prologue and /GS security-cookie setup/check. Original gta_sa.exe
    was built without frame pointers; gta_reversed RelWithDebInfo uses /Oy- + /GS,
    so without this the mnemonic sequence is polluted by compiler config."""
    def is_fp_prologue(seq):
        return (len(seq) >= 2 and seq[0].mnemonic == "push" and seq[0].op_str == "ebp"
                and seq[1].mnemonic == "mov" and seq[1].op_str.startswith("ebp, esp"))
    def is_gs_prologue(seq):
        return (len(seq) >= 3 and seq[0].mnemonic == "mov"
                and seq[1].mnemonic == "xor" and seq[1].op_str.endswith(", ebp")
                and seq[2].mnemonic == "mov" and "[ebp - 4]" in seq[2].op_str
                and seq[0].op_str.split(",")[0] == seq[1].op_str.split(",")[0]
                and seq[0].op_str.split(",")[0] == seq[2].op_str.split(",")[0].split("[")[0].strip())
    out = list(insns)
    if is_fp_prologue(out):
        out = out[2:]
    if is_gs_prologue(out):
        out = out[3:]
    # epilogue: ... mov REG,[ebp-4]; xor REG,ebp; [cmp REG,ebp;] [jne X;] call chk | ret
    def tail_matches(n):
        if len(out) < n:
            return False
        t = out[-n:]
        has_mov = any(i.mnemonic == "mov" and "[ebp - 4]" in i.op_str for i in t)
        has_xor = any(i.mnemonic == "xor" and i.op_str.endswith(", ebp") for i in t)
        return has_mov and has_xor
    for n in (5, 4, 3):
        if tail_matches(n):
            del out[-n:]
            break
    return out

def collect_constants(insns, pe, float_resolve=True, drop_orig_image_addrs=None):
    """immediate + float constants. Absolute-address immediates are excluded
    (they are relocated pointers, meaningless across binaries).
    drop_orig_image_addrs: (lo, hi) VA range of the ORIGINAL exe image; immediates
    inside it on OUR side are StaticRef<T> template tags (original data addresses
    baked in as placeholders), not arithmetic constants."""
    ints = collections.Counter()
    flts = collections.Counter()
    for insn in insns:
        for op in insn.operands:
            if op.type == X86_OP_IMM:
                if insn.mnemonic in ("call", "jmp", "loop", "loope", "loopne"):
                    continue
                v = op.imm
                if abs(v) >= pe.imagebase:   # address-like
                    continue
                if drop_orig_image_addrs and drop_orig_image_addrs[0] <= v < drop_orig_image_addrs[1]:
                    continue
                ints[v] += 1
            elif op.type == X86_OP_MEM and float_resolve and is_floatish(insn.mnemonic):
                m = op.mem
                if m.base == 0 and m.index == 0 and m.disp > 0 and op.size in (4, 8):
                    va = m.disp & 0xFFFFFFFF
                    raw = pe.read_va(va, op.size)
                    if raw and len(raw) == op.size:
                        val = struct.unpack("<f" if op.size == 4 else "<d", raw)[0]
                        flts["%.7g" % val] += 1
    return ints, flts

def collect_callees(insns, va, end_va):
    """Direct call targets + direct jmp targets leaving the function range."""
    tgts = []
    for insn in insns:
        if insn.mnemonic not in ("call", "jmp"):
            continue
        if insn.operands and insn.operands[0].type == X86_OP_IMM:
            t = insn.operands[0].imm
            if insn.mnemonic == "call" or t < va or t >= end_va:
                tgts.append(t)
    return tgts

def branch_stats(insns):
    n_jcc = n_back = n_jmp = 0
    lo = insns[0].address if insns else 0
    hi = (insns[-1].address + insns[-1].size) if insns else 0
    for insn in insns:
        if insn.mnemonic in JCC:
            n_jcc += 1
            if insn.operands and insn.operands[0].type == X86_OP_IMM \
                    and insn.operands[0].imm < insn.address:
                n_back += 1
        elif insn.mnemonic == "jmp":
            n_jmp += 1
    return {"jcc": n_jcc, "back_jcc": n_back, "jmp": n_jmp, "insns": len(insns)}

# ----------------------------------------------------------------------------
# symbol maps
# ----------------------------------------------------------------------------
class PdbMap:
    def __init__(self, dump):
        self.by_key = collections.defaultdict(list)   # plain key -> [(rva, size, decorated)]
        self.by_last = collections.defaultdict(list)  # last scope component -> [...]
        self.addr2name = {}                           # rva -> plain name (exact)
        for fn in dump["functions"]:
            key = re.sub(r"\s+", "", fn.get("d") or "")
            if not key:
                continue
            self.by_key[key].append((fn["r"], fn["s"], fn["n"]))
            self.by_last[key.split("::")[-1]].append((fn["r"], fn["s"], fn["n"]))
            self.addr2name.setdefault(fn["r"], fn.get("d") or fn["n"])
        self.sorted_rvas = sorted(self.addr2name)

    def lookup(self, name):
        """-> list of (rva, size, decorated). Tries exact, then progressively
        relaxed variants (overload tags, ctor/dtor aliases, template args,
        last scope component for file-static symbols)."""
        key = re.sub(r"\s+", "", name)
        tries = [key]
        # overload tags from the batch lists: "Fn-suffix" -> "Fn"
        m = re.match(r"^(.+?)-[^:-]+$", key)
        if m:
            tries.append(m.group(1))
        # constructor/destructor aliases (repo calls them X::Constructor / X::Destructor)
        m = re.match(r"^(.+)::(Constructor|Destructor)$", key)
        if m:
            cls = m.group(1)
            tries.append(cls + "::" + (cls if m.group(2) == "Constructor" else "~" + cls))
        # strip template args (innermost-out, both sides may nest differently)
        for base in list(tries):
            t = base
            while "<" in t:
                t2 = re.sub(r"<[^<>]*>", "", t)
                if t2 == t:
                    break
                t = t2
            tries.append(t)
        for t in tries:
            hits = self.by_key.get(t)
            if hits:
                return hits
        # file-static symbols have no scope in the PDB: match last component only
        for t in tries:
            hits = self.by_last.get(t.split("::")[-1])
            if hits and len(hits) <= 4:
                return hits
        return []

    def nearest(self, rva, max_disp=0x100000):
        i = bisect.bisect_right(self.sorted_rvas, rva) - 1
        if i < 0:
            return None, rva
        a = self.sorted_rvas[i]
        return self.addr2name[a], rva - a

    def resolve(self, rva, asi=None, max_hops=5):
        """rva -> (name, disp). Follows MSVC ILT jmp-thunks (.textbss) when the
        target itself carries no function symbol."""
        r = rva
        for _ in range(max_hops):
            if r in self.addr2name:
                return self.addr2name[r], 0
            if asi is None:
                break
            ins = disasm(asi, asi.imagebase + r, asi.imagebase + r + 16)
            if ins and ins[0].mnemonic == "jmp" and ins[0].operands \
                    and ins[0].operands[0].type == X86_OP_IMM:
                r = ins[0].operands[0].imm - asi.imagebase
                if r < 0:
                    break
                continue
            break
        return self.nearest(r)

# ----------------------------------------------------------------------------
# per-function comparison
# ----------------------------------------------------------------------------
def compare_one(entry, exe, asi, pmap, name2orig, known_orig_addrs):
    name = entry["name"]
    orig_va = entry["orig_addr"]
    res = {"name": name, "orig_addr": orig_va, "key": entry.get("key", name)}

    # ---- original side
    i = bisect.bisect_right(known_orig_addrs, orig_va)
    boundary = known_orig_addrs[i] if i < len(known_orig_addrs) else orig_va + MAX_FN_BYTES
    end_va = min(boundary, orig_va + MAX_FN_BYTES)
    if end_va <= orig_va:
        end_va = orig_va + MAX_FN_BYTES
    orig_insns = disasm(exe, orig_va, end_va)
    res["orig_end"] = hex(end_va)
    res["orig_bytes"] = sum(x.size for x in orig_insns)
    if not orig_insns:
        res["error"] = "original disassembly empty at 0x%X" % orig_va
        res["score"] = None
        return res

    # ---- our side (via PDB)
    cands = pmap.lookup(name)
    res["candidates"] = [{"rva": hex(r), "size": s} for r, s, _ in cands]
    if not cands:
        res["error"] = "symbol not found in PDB: %s" % name
        res["score"] = None
        return res
    if len(cands) == 1:
        rva, size, _dec = cands[0]
    else:
        # disambiguate by size closeness to the original
        res["ambiguous"] = True
        rva, size, _dec = min(cands, key=lambda c: abs(c[1] - res["orig_bytes"]))
    our_va = asi.imagebase + rva
    our_insns = disasm(asi, our_va, our_va + max(size, 1))
    res["our_rva"] = hex(rva)
    res["our_bytes"] = sum(x.size for x in our_insns)
    res["our_size_pdb"] = size
    if not our_insns:
        res["error"] = "our disassembly empty at asi rva 0x%X" % rva
        res["score"] = None
        return res

    # ---- normalized bodies (boilerplate stripping can be disabled with --raw)
    so = orig_insns if RAW_MODE else strip_boilerplate(orig_insns)
    su = our_insns if RAW_MODE else strip_boilerplate(our_insns)

    # ---- 1) mnemonic similarity
    a = normalize_seq(so)
    b = normalize_seq(su)
    mnem = difflib.SequenceMatcher(None, a, b, autojunk=False).ratio()

    # ---- 2) constants
    orig_img = (exe.imagebase, exe.imagebase + len(exe.data))
    oi, of = collect_constants(so, exe)
    ui, uf = collect_constants(su, asi, drop_orig_image_addrs=orig_img)
    oc = collections.Counter({("i", k): v for k, v in oi.items()})
    oc.update({("f", k): v for k, v in of.items()})
    uc = collections.Counter({("i", k): v for k, v in ui.items()})
    uc.update({("f", k): v for k, v in uf.items()})
    inter = sum((oc & uc).values())
    to, tu = sum(oc.values()), sum(uc.values())
    const_score = (2.0 * inter / (to + tu)) if (to + tu) else 1.0
    def msdiff(x, y):
        d = x - y
        return [(k[0] + ":" + str(k[1]), n) for k, n in d.most_common(25)]
    res["missing_constants"] = msdiff(oc, uc)   # in original, absent in ours
    res["extra_constants"] = msdiff(uc, oc)     # in ours, absent in original

    # ---- 3) call targets, mapped into original address space
    def resolve_ours(t_va):
        r = t_va - asi.imagebase
        nm, disp = pmap.resolve(r, asi)
        if nm is None or disp >= 0x100000:
            return ["unk@asi+%X" % r]
        oa = name2orig.get(re.sub(r"\s+", "", nm))
        if oa is not None and disp == 0:
            return [hex(oa)]
        return ["%s+%X" % (nm, disp) if disp else nm]

    def label_orig(t_va):
        return hex(t_va)

    orig_calls = collect_callees(so, orig_va, end_va)
    our_calls = collect_callees(su, our_va, our_va + max(size, 1))
    oset = set(label_orig(t) for t in orig_calls)
    uset = set()
    for t in our_calls:
        uset.update(resolve_ours(t))
    interc = len(oset & uset)
    call_score = (interc / len(oset | uset)) if (oset | uset) else 1.0
    res["callees"] = {
        "orig": sorted(oset), "ours": sorted(uset),
        "only_in_orig": sorted(oset - uset), "only_in_ours": sorted(uset - oset),
    }
    status = []
    if hex(orig_va) in uset:
        status.append("forwarder-to-original 0x%X (body not compiled in)" % orig_va)
    if any("CallMethod" in x for x in uset):
        status.append("plugin::CallMethod forwarder")
    if res["our_bytes"] <= 0.2 * res["orig_bytes"]:
        status.append("our body is <=20% of original size (stub/thunk/split)")
    res["status"] = status

    # ---- 4) branch / loop delta
    bo, bu = branch_stats(so), branch_stats(su)
    res["branches"] = {"orig": bo, "ours": bu}
    tot = max(1, bo["jcc"] + bo["jmp"])
    branch_delta = abs((bo["jcc"] + bo["jmp"]) - (bu["jcc"] + bu["jmp"]))
    branch_score = max(0.0, 1.0 - branch_delta / tot)

    score = 0.45 * mnem + 0.30 * const_score + 0.15 * call_score + 0.10 * branch_score
    res.update({
        "score": round(score, 4),
        "components": {"mnemonic": round(mnem, 4), "constants": round(const_score, 4),
                       "calls": round(call_score, 4), "branches": round(branch_score, 4)},
        "orig_insns": bo["insns"], "our_insns": bu["insns"],
    })
    return res

# ----------------------------------------------------------------------------
# reports
# ----------------------------------------------------------------------------
def write_md(results, out_path, meta_lines):
    ok = [r for r in results if r.get("score") is not None]
    bad = [r for r in results if r.get("score") is None]
    ok.sort(key=lambda r: r["score"])
    lines = []
    lines.append("# bin_diff report -- compiled code vs original binary")
    lines.append("")
    lines += meta_lines
    lines.append("")
    lines.append("Compared: %d functions | scored: %d | unresolved: %d"
                 % (len(results), len(ok), len(bad)))
    bands = [("identical-ish (>=0.95)", lambda s: s >= 0.95),
             ("close (0.85-0.95)", lambda s: 0.85 <= s < 0.95),
             ("drifted (0.70-0.85)", lambda s: 0.70 <= s < 0.85),
             ("diverged (<0.70)", lambda s: s < 0.70)]
    lines.append("")
    for label, pred in bands:
        n = len([r for r in ok if pred(r["score"])])
        lines.append("- %s: %d" % (label, n))
    lines.append("")
    lines.append("## Worst 50")
    lines.append("")
    lines.append("| score | mnem | const | calls | branch | function | orig | bytes o/u |")
    lines.append("|---|---|---|---|---|---|---|---|")
    for r in ok[:50]:
        c = r["components"]
        lines.append("| %.3f | %.2f | %.2f | %.2f | %.2f | %s | 0x%X | %d/%d |"
                     % (r["score"], c["mnemonic"], c["constants"], c["calls"],
                        c["branches"], r["name"], r["orig_addr"],
                        r["orig_bytes"], r.get("our_bytes", 0)))
    lines.append("")
    lines.append("## Top-20 detail (worst)")
    for r in ok[:20]:
        c = r["components"]
        lines.append("")
        lines.append("### %s (orig 0x%X, score %.3f)" % (r["name"], r["orig_addr"], r["score"]))
        lines.append("")
        if r.get("status"):
            lines.append("- STATUS: %s" % "; ".join(r["status"]))
        lines.append("- components: mnemonic %.3f, constants %.3f, calls %.3f, branches %.3f"
                     % (c["mnemonic"], c["constants"], c["calls"], c["branches"]))
        lines.append("- bytes: original %d, ours %d (insns %d vs %d)"
                     % (r["orig_bytes"], r.get("our_bytes", 0), r["orig_insns"], r["our_insns"]))
        b = r["branches"]
        lines.append("- branches: orig jcc=%d back=%d jmp=%d | ours jcc=%d back=%d jmp=%d"
                     % (b["orig"]["jcc"], b["orig"]["back_jcc"], b["orig"]["jmp"],
                        b["ours"]["jcc"], b["ours"]["back_jcc"], b["ours"]["jmp"]))
        if r["missing_constants"]:
            lines.append("- missing constants (in original, absent in ours): %s"
                         % ", ".join("%s x%d" % (k, n) for k, n in r["missing_constants"][:12]))
        if r["extra_constants"]:
            lines.append("- extra constants (in ours only): %s"
                         % ", ".join("%s x%d" % (k, n) for k, n in r["extra_constants"][:12]))
        ca = r["callees"]
        if ca["only_in_orig"]:
            lines.append("- callees only in original: %s" % ", ".join(ca["only_in_orig"][:12]))
        if ca["only_in_ours"]:
            lines.append("- callees only in ours: %s" % ", ".join(ca["only_in_ours"][:12]))
        if r.get("ambiguous"):
            lines.append("- NOTE: name is ambiguous (%d candidates), picked closest-size one"
                         % len(r["candidates"]))
    if bad:
        lines.append("")
        lines.append("## Unresolved (%d)" % len(bad))
        lines.append("")
        for r in bad:
            lines.append("- %s (0x%X): %s" % (r["name"], r["orig_addr"], r.get("error", "?")))
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------
def main(argv):
    cmd = argv[1] if len(argv) > 1 else "run"
    args = argv[2:] if len(argv) > 1 else []

    def opt(flag, default=None, as_int=False):
        if flag in args:
            i = args.index(flag)
            v = args[i + 1]
            return int(v, 0) if as_int else v
        return default

    if cmd == "dump-pdb":
        dump_pdb()
        return 0

    if cmd == "scan-repo":
        installs = scan_repo()
        en = [i for i in installs if i["enabled"]]
        out = os.path.join(HERE, "targets_repo.json")
        json.dump([{"name": i["name"], "orig_addr": i["addr"], "key": "%s@%s"
                    % (i["name"], hex(i["addr"]))} for i in en], open(out, "w"), indent=1)
        print("repo installs: %d total, %d enabled -> %s" % (len(installs), len(en), out))
        return 0

    if cmd != "run":
        print(__doc__)
        return 2

    t0 = time.time()
    targets_path = opt("--targets", DEFAULTS["targets"])
    limit = opt("--limit", None, as_int=True)
    only = opt("--only")
    if "--raw" in args:
        global RAW_MODE
        RAW_MODE = True

    if os.path.exists(targets_path):
        targets = json.load(open(targets_path, encoding="utf-8"))
        print("[targets] %s: %d entries" % (targets_path, len(targets)))
    else:
        print("[targets] %s not found; building from final_push batches + repo scan" % targets_path)
        targets = build_target_list_from_batches()
        json.dump(targets, open(DEFAULTS["targets"], "w"), indent=1)
        print("[targets] wrote %s: %d entries" % (DEFAULTS["targets"], len(targets)))
    if only:
        targets = [t for t in targets if only in t["name"]]
    if limit:
        targets = targets[:limit]
    # dedupe by orig addr
    seen, uniq = set(), []
    for t in targets:
        if t["orig_addr"] in seen:
            continue
        seen.add(t["orig_addr"])
        uniq.append(t)
    targets = uniq

    exe = PE(DEFAULTS["exe"])
    asi = PE(DEFAULTS["asi"])
    print("[bin] exe base 0x%X (%d bytes) | asi base 0x%X (%d bytes)"
          % (exe.imagebase, len(exe.data), asi.imagebase, len(asi.data)))

    dump = dump_pdb()
    pmap = PdbMap(dump)

    installs = scan_repo()
    name2orig = {}
    name2orig_addr = {}
    for ins in installs:
        k = re.sub(r"\s+", "", ins["name"])
        name2orig.setdefault(k, ins["addr"])
        name2orig_addr.setdefault(ins["addr"], ins["name"])
    known = sorted(set(i["addr"] for i in installs) | set(t["orig_addr"] for t in targets))
    print("[repo] %d installs (%d enabled), %d known original addresses"
          % (len(installs), len([i for i in installs if i["enabled"]]), len(known)))

    results = []
    for n, t in enumerate(targets):
        try:
            r = compare_one(t, exe, asi, pmap, name2orig, known)
        except Exception as e:
            r = {"name": t["name"], "orig_addr": t["orig_addr"], "score": None,
                 "error": "exception: %r" % e}
        results.append(r)
        if (n + 1) % 25 == 0 or n + 1 == len(targets):
            print("[%d/%d] %.1fs" % (n + 1, len(targets), time.time() - t0))

    ok = [r for r in results if r.get("score") is not None]
    bad = [r for r in results if r.get("score") is None]
    ok.sort(key=lambda r: r["score"])
    out_json = os.path.join(HERE, "report.json")
    out_md = os.path.join(HERE, "report.md")
    meta = ["- original: %s (base 0x%X)" % (DEFAULTS["exe"], exe.imagebase),
            "- ours: %s (base 0x%X)" % (DEFAULTS["asi"], asi.imagebase),
            "- generated: %s" % time.strftime("%Y-%m-%d %H:%M:%S"),
            "- score = 0.45*mnemonic + 0.30*constants + 0.15*calls + 0.10*branches",
            "- normalization: opcode classes (registers stripped), MSVC frame-pointer/GS-cookie boilerplate stripped%s" % (" (--raw: off)" if RAW_MODE else "")]
    json.dump({"meta": meta, "results": results}, open(out_json, "w", encoding="utf-8"), indent=1)
    write_md(results, out_md, meta)

    # console table
    print("")
    print("=== %d compared, %d unresolved ===" % (len(ok), len(bad)))
    print("%-7s %-7s %-7s %-7s %-7s %-9s %s"
          % ("score", "mnem", "const", "calls", "brnch", "bytes o/u", "function"))
    for r in ok[:60]:
        c = r["components"]
        print("%.4f  %.2f    %.2f    %.2f    %.2f    %4d/%-4d  %s"
              % (r["score"], c["mnemonic"], c["constants"], c["calls"], c["branches"],
                 r["orig_bytes"], r.get("our_bytes", 0), r["name"]))
    if bad:
        print("")
        print("unresolved:")
        for r in bad[:20]:
            print("  %s (0x%X): %s" % (r["name"], r["orig_addr"], r.get("error", "?")))
    print("")
    print("reports: %s, %s" % (out_json, out_md))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
