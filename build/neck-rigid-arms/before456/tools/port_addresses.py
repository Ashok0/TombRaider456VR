# port_addresses.py -- translate RVAs from one build of a PE to another.
#
#   python tools\port_addresses.py <reference.dll> <target.dll> 0x1B1CA0 0x2FD540 ...
#   python tools\port_addresses.py <reference> <target> --stats
#
# The TR4-6 binaries in circulation (the debug/PDB release this mod was written
# against, and the retail Steam release) are the same source built at different
# times, so their addresses do not differ by a constant. This recovers the map
# structurally instead of by hand:
#
#   1. Every function is taken from .pdata (x64 unwind table, so exact bounds)
#      and disassembled. Each instruction is normalised with RIP-relative
#      displacements and branch/call targets wildcarded -- the only bytes that
#      move when surrounding code moves -- while struct offsets, immediates and
#      stack layout are kept, so a match also says the FIELD OFFSETS agree.
#   2. Functions whose normalised stream is unique in both builds are paired.
#   3. Pairs propagate: the Nth call/branch target of a paired function pairs
#      with the Nth of its partner, and every RIP-relative operand votes for a
#      data/code address pairing. Repeated until nothing new is learnt.
#   4. Whatever is left is paired by nearest neighbour in address order between
#      two already-paired anchors, accepted only above a similarity threshold.
#
# A code address inside a function maps by instruction index when the pair is
# exact, and by aligned instruction (difflib) otherwise. A data address maps by
# vote; the vote count and any dissent are printed so a thin result is visible.
import sys, struct, hashlib, bisect, difflib, collections
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_IMM, X86_REG_RIP

class Image:
    def __init__(self, path):
        self.path = path
        pe = pefile.PE(path, fast_load=True)
        pe.parse_data_directories(directories=[
            pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.stamp = pe.FILE_HEADER.TimeDateStamp
        self.mem = pe.get_memory_mapped_image()
        self.sections = [(s.Name.rstrip(b'\0').decode(), s.VirtualAddress,
                          s.Misc_VirtualSize) for s in pe.sections]
        text = next(s for s in self.sections if s[0] == '.text')
        self.text = (text[1], text[1] + text[2])
        # .pdata entries; unwind-chained fragments are separate regions, which
        # is what we want -- each is a contiguous run of code.
        funcs = {}
        for e in getattr(pe, 'DIRECTORY_ENTRY_EXCEPTION', []):
            b, en = e.struct.BeginAddress, e.struct.EndAddress
            if self.text[0] <= b < self.text[1] and en > b:
                funcs[b] = max(funcs.get(b, 0), en)
        self.starts = sorted(funcs)
        self.ends = [funcs[s] for s in self.starts]
        self.md = Cs(CS_ARCH_X86, CS_MODE_64)
        self.md.detail = True
        self.cache = {}

    def func_at(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        if i >= 0 and rva < self.ends[i]:
            return self.starts[i]
        return None

    def section_of(self, rva):
        for n, va, sz in self.sections:
            if va <= rva < va + max(sz, 1):
                return n
        return None

    def disasm(self, start):
        if start in self.cache:
            return self.cache[start]
        end = self.ends[self.starts.index(start)]
        code = self.mem[start:end]
        out = []   # (rva, size, norm, rip_target, branch_target)
        for ins in self.md.disasm(code, start):
            rip = None
            br = None
            ops = []
            for op in ins.operands:
                if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                    rip = ins.address + ins.size + op.mem.disp
                    ops.append('m[rip]%d' % op.size)
                elif op.type == X86_OP_MEM:
                    ops.append('m[%d,%d,%d,%d,%d]%d' % (op.mem.base, op.mem.index,
                               op.mem.scale, op.mem.disp, op.mem.segment, op.size))
                elif op.type == X86_OP_IMM and ins.group(1) or \
                     op.type == X86_OP_IMM and ins.group(7) or \
                     op.type == X86_OP_IMM and ins.group(2):
                    # jump / call / relative branch groups
                    br = op.imm
                    ops.append('T')
                elif op.type == X86_OP_IMM:
                    ops.append('i%d' % op.imm)
                else:
                    ops.append('r%d' % op.reg)
            norm = ins.mnemonic + ' ' + ','.join(ops)
            out.append((ins.address, ins.size, norm, rip, br))
        self.cache[start] = out
        return out

    def key(self, start):
        return hashlib.sha1('\n'.join(x[2] for x in self.disasm(start))
                            .encode()).hexdigest()


class Matcher:
    def __init__(self, ref, tgt):
        self.ref, self.tgt = ref, tgt
        self.f = {}         # ref func start -> tgt func start
        self.fexact = set() # pairs whose normalised streams are identical
        self.votes = collections.defaultdict(collections.Counter)  # ref addr -> Counter(tgt addr)
        self.run()

    def pair(self, a, b, exact):
        if a in self.f or b in self.rf:
            return False
        self.f[a] = b
        self.rf[b] = a
        if exact:
            self.fexact.add(a)
        self.queue.append(a)
        return True

    def run(self):
        ref, tgt = self.ref, self.tgt
        self.rf = {}
        self.queue = []
        rk = collections.defaultdict(list)
        tk = collections.defaultdict(list)
        for s in ref.starts: rk[ref.key(s)].append(s)
        for s in tgt.starts: tk[tgt.key(s)].append(s)
        for k, rs in rk.items():
            ts = tk.get(k)
            if ts and len(rs) == 1 and len(ts) == 1:
                self.pair(rs[0], ts[0], True)
        self.propagate()
        self.fuzzy()
        self.propagate()

    def propagate(self):
        ref, tgt = self.ref, self.tgt
        while self.queue:
            a = self.queue.pop()
            b = self.f[a]
            ia, ib = ref.disasm(a), tgt.disasm(b)
            if a in self.fexact:
                aligned = list(zip(ia, ib))
            else:
                aligned = self.align(ia, ib)
            for x, y in aligned:
                for rx, ry in ((x[3], y[3]), (x[4], y[4])):
                    if rx is None or ry is None:
                        continue
                    self.votes[rx][ry] += 1
                    fa, fb = ref.func_at(rx), tgt.func_at(ry)
                    if fa == rx and fb == ry and fa not in self.f and fb not in self.rf:
                        exact = ref.key(fa) == tgt.key(fb)
                        if exact or self.similar(fa, fb) >= 0.80:
                            self.pair(fa, fb, exact)

    @staticmethod
    def align(ia, ib):
        sm = difflib.SequenceMatcher(None, [x[2] for x in ia], [y[2] for y in ib],
                                     autojunk=False)
        out = []
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == 'equal':
                out.extend(zip(ia[i1:i2], ib[j1:j2]))
        return out

    def similar(self, a, b):
        ia = [x[2] for x in self.ref.disasm(a)]
        ib = [y[2] for y in self.tgt.disasm(b)]
        if not ia or not ib:
            return 0.0
        if max(len(ia), len(ib)) > 3 * min(len(ia), len(ib)):
            return 0.0
        return difflib.SequenceMatcher(None, ia, ib, autojunk=False).ratio()

    def fuzzy(self):
        # Unpaired functions bracketed by paired anchors: try the unpaired
        # functions in the corresponding target window, best ratio wins.
        ref, tgt = self.ref, self.tgt
        anchors = sorted(self.f.items())
        ra = [a for a, _ in anchors]
        for s in ref.starts:
            if s in self.f:
                continue
            i = bisect.bisect_left(ra, s)
            lo = anchors[i - 1][1] if i > 0 else tgt.text[0]
            hi = anchors[i][1] if i < len(anchors) else tgt.text[1]
            if hi < lo:
                continue
            j0 = bisect.bisect_left(tgt.starts, lo)
            j1 = bisect.bisect_right(tgt.starts, hi)
            cands = [t for t in tgt.starts[j0:j1] if t not in self.rf]
            if not cands or len(cands) > 40:
                continue
            best = max(cands, key=lambda t: self.similar(s, t))
            r = self.similar(s, best)
            if r >= 0.85:
                self.pair(s, best, False)

    # ------------------------------------------------------------------ queries
    def code(self, rva):
        """Map a code address (function start or any instruction inside one)."""
        a = self.ref.func_at(rva)
        if a is None:
            return None, 'not inside any .pdata function'
        if a not in self.f or (a == rva and a not in self.fexact and rva in self.votes):
            # Unpaired (or only loosely paired) function: its callers in matched
            # code still say where it went, one vote per aligned call site.
            if rva in self.votes:
                r, why = self.data(rva)
                return r, 'by call/branch sites: ' + why
            return None, 'function 0x%X unmatched' % a
        b = self.f[a]
        ia, ib = self.ref.disasm(a), self.tgt.disasm(b)
        how = 'exact' if a in self.fexact else 'fuzzy %.3f' % self.similar(a, b)
        pairs = list(zip(ia, ib)) if a in self.fexact else self.align(ia, ib)
        for x, y in pairs:
            if x[0] == rva:
                return y[0], 'func 0x%X->0x%X %s' % (a, b, how)
        # rva lands on an instruction the alignment could not pair
        return None, 'func 0x%X->0x%X %s but instruction at +0x%X not aligned' % (
            a, b, how, rva - a)

    def data(self, rva):
        c = self.votes.get(rva)
        if not c:
            return None, 'no references from matched code'
        (best, n), *rest = c.most_common()
        dissent = sum(v for _, v in rest)
        return best, '%d vote(s)%s' % (n, ', %d dissent' % dissent if dissent else '')


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__ if __doc__ else 'usage: port_addresses.py ref tgt rva...')
        sys.exit(2)
    ref, tgt = Image(args[0]), Image(args[1])
    m = Matcher(ref, tgt)
    print('# ref %s stamp 0x%08X  funcs %d' % (ref.path, ref.stamp, len(ref.starts)))
    print('# tgt %s stamp 0x%08X  funcs %d' % (tgt.path, tgt.stamp, len(tgt.starts)))
    print('# paired %d (%d exact)' % (len(m.f), len(m.fexact)))
    for a in args[2:]:
        if a == '--stats':
            continue
        rva = int(a, 16)
        sec = ref.section_of(rva)
        if sec == '.text':
            r, why = m.code(rva)
        else:
            r, why = m.data(rva)
        print('0x%08X -> %s  [%s] %s' % (rva, '0x%08X' % r if r is not None else '??????????',
                                        sec, why))

if __name__ == '__main__':
    main()
