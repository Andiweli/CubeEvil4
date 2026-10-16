#!/usr/bin/env python3
"""Gekko (PowerPC 750CL) inline asm of the CRI middleware -> C.

The CRI MPEG decoder of src/lib keeps its paired-single kernels as
CodeWarrior inline asm (mpv_umc.c, dct_fsri.c, cftyp422_ppc.c). This tool
translates such a block instruction by instruction into C: every GPR is a
32-bit variable, every FPR a pair of floats (ps0, ps1), labels and branches
become C labels and gotos, CR0, CTR and the carry bit are variables. The
quantised loads and stores (psq_l / psq_st) go through the graphics
quantisation registers the CRI code sets itself (mtspr GQRn), emulated as
re4_cri_gqr[] (re4_cri_port.h). Memory is read in the host's byte order: the
kernels translated here only move bytes, floats and the 16-bit IDCT blocks
that other translated kernels write.

The output is meant to be reviewed and checked in as a patch
(port/whole/patches/0050-cri-asm-in-c.patch, built by
port/tools/whole_cri_patches.py); the instruction set covered is what those
blocks use. An unknown instruction stops the translation.

Operands are physical registers (r0..r31, f0..f31) or C variables the
surrounding function declared (`register Uint8 *d`, `register
__vec2x32float__ x0`). C variables of integer or pointer type are copied into
32-bit registers at the start of the block and back at its end (only those
the block writes); paired-single variables are used in place.
"""
import re

GQR_SPR = {f"GQR{i}": i for i in range(8)}
GQR_SPR.update({str(912 + i): i for i in range(8)})
PHYS_GPR = re.compile(r"^r(\d+)$")
PHYS_FPR = re.compile(r"^f(\d+)$")
MEM = re.compile(r"^(-?[\w@+ .]+)\((\w+)\)$")


class Translator:
    def __init__(self, prefix, c_gprs=(), c_fprs=(), entry=None, symbols=None):
        """prefix: label prefix; c_gprs / c_fprs: names of C variables used as
        integer / paired-single operands; entry: physical register -> C
        expression it holds at the start (function arguments)."""
        self.prefix = prefix
        self.c_gprs = set(c_gprs)
        self.c_fprs = set(c_fprs)
        self.entry = dict(entry or {})
        self.symbols = symbols or {}
        self.used_gprs = set()
        self.used_fprs = set()
        self.written = set()
        self.lines = []

    # --- operands ---------------------------------------------------------
    def gpr(self, name, write=False):
        name = name.strip()
        m = PHYS_GPR.match(name)
        if m:
            self.used_gprs.add(int(m.group(1)))
            return f"r{m.group(1)}"
        if name in self.c_gprs:
            if write:
                self.written.add(name)
            return f"v_{name}"
        raise ValueError(f"unknown integer operand {name}")

    def base(self, name):
        """rA of an address: r0 reads as 0."""
        if name.strip() == "r0" or name.strip() == "0":
            return "0u"
        return self.gpr(name)

    def fpr(self, name):
        name = name.strip()
        m = PHYS_FPR.match(name)
        if m:
            self.used_fprs.add(int(m.group(1)))
            return f"f{m.group(1)}"
        if name in self.c_fprs:
            return name
        raise ValueError(f"unknown float operand {name}")

    def imm(self, text):
        text = text.strip()
        m = re.match(r"^(.+)@ha$", text)
        if m:  # lis rD, sym@ha: the whole address (the @l part then adds 0)
            return f"(Uint32)({self.symbol(m.group(1))})"
        m = re.match(r"^(.+)@l$", text)
        if m:
            return "0"
        return str(int(text, 0))

    def symbol(self, text):
        text = text.strip()
        m = re.match(r"^(\w+)\s*\+\s*(0x[0-9a-fA-F]+|\d+)$", text)
        if m:
            return f"(Uint8 *){self.symbols.get(m.group(1), m.group(1))} + {m.group(2)}"
        return f"(Uint8 *){self.symbols.get(text, text)}"

    def mem(self, text):
        m = MEM.match(text.strip())
        if not m:
            raise ValueError(f"bad memory operand {text}")
        disp = self.imm(m.group(1))
        return disp, self.base(m.group(2)), m.group(2)

    def label(self, name):
        return f"{self.prefix}_{name}"

    # --- instructions -----------------------------------------------------
    def emit(self, s):
        self.lines.append("\t" + s)

    def cmp0(self, value):
        self.emit(f"cr0 = (Sint32)({value}) < 0 ? -1 : (Sint32)({value}) > 0 ? 1 : 0;")

    def translate_line(self, line):
        line = line.strip()
        if not line:
            return
        if line.endswith(":"):
            self.lines.append(f"{self.label(line[:-1])}:;")
            return
        op, _, rest = line.partition(" ")
        args = [a.strip() for a in rest.split(",")] if rest.strip() else []
        record = op.endswith(".")
        base_op = op.rstrip(".")
        handler = getattr(self, "op_" + base_op.replace(".", "_"), None)
        if handler is None:
            raise ValueError(f"unsupported instruction: {line}")
        handler(args, record)

    # integer arithmetic
    def op_add(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = {self.gpr(a[1])} + {self.gpr(a[2])};")
        if rec:
            self.cmp0(d)

    def op_addi(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = {self.base(a[1])} + (Uint32)({self.imm(a[2])});")

    def op_subi(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = {self.base(a[1])} - (Uint32)({self.imm(a[2])});")

    def op_subic(self, a, rec):
        d = self.gpr(a[0], True)
        s = self.gpr(a[1])
        v = self.imm(a[2])
        self.emit(f"ca = {s} >= (Uint32)({v});")
        self.emit(f"{d} = {s} - (Uint32)({v});")
        if rec:
            self.cmp0(d)

    def op_subf(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = {self.gpr(a[2])} - {self.gpr(a[1])};")
        if rec:
            self.cmp0(d)

    def op_addze(self, a, rec):
        d = self.gpr(a[0], True)
        s = self.gpr(a[1])
        self.emit(f"{{ Uint32 t_ = {s} + ca; ca = t_ < {s}; {d} = t_; }}")
        if rec:
            self.cmp0(d)

    def op_mulli(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = (Uint32)((Sint32){self.gpr(a[1])} * (Sint32)({self.imm(a[2])}));")

    def op_li(self, a, rec):
        self.emit(f"{self.gpr(a[0], True)} = (Uint32)({self.imm(a[1])});")

    def op_lis(self, a, rec):
        v = a[1].strip()
        d = self.gpr(a[0], True)
        if v.endswith("@ha"):
            self.emit(f"{d} = {self.imm(v)};")
        else:
            self.emit(f"{d} = (Uint32)({self.imm(v)}) << 16;")

    def op_oris(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = {self.gpr(a[1])} | ((Uint32)({self.imm(a[2])}) << 16);")

    def op_mr(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = {self.gpr(a[1])};")

    def op_slwi(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = {self.gpr(a[1])} << {int(a[2], 0)};")

    def op_srwi(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = {self.gpr(a[1])} >> {int(a[2], 0)};")

    def op_srawi(self, a, rec):
        d = self.gpr(a[0], True)
        s = self.gpr(a[1])
        n = int(a[2], 0)
        self.emit(f"ca = (Sint32){s} < 0 && ({s} & {hex((1 << n) - 1)}u) != 0;")
        self.emit(f"{d} = (Uint32)((Sint32){s} >> {n});")
        if rec:
            self.cmp0(d)

    def op_clrlwi(self, a, rec):
        d = self.gpr(a[0], True)
        n = int(a[2], 0)
        self.emit(f"{d} = {self.gpr(a[1])} & {hex(0xFFFFFFFF >> n)}u;")
        if rec:
            self.cmp0(d)

    def op_clrlslwi(self, a, rec):
        d = self.gpr(a[0], True)
        b, n = int(a[2], 0), int(a[3], 0)
        self.emit(f"{d} = ({self.gpr(a[1])} & {hex(0xFFFFFFFF >> b)}u) << {n};")

    def op_extrwi(self, a, rec):
        d = self.gpr(a[0], True)
        n, b = int(a[2], 0), int(a[3], 0)
        self.emit(f"{d} = ({self.gpr(a[1])} >> {32 - (b + n)}) & {hex((1 << n) - 1)}u;")

    def op_rlwinm(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = __rlwinm({self.gpr(a[1])}, {int(a[2], 0)}, {int(a[3], 0)}, {int(a[4], 0)});")
        if rec:
            self.cmp0(d)

    def op_rlwimi(self, a, rec):
        d = self.gpr(a[0], True)
        self.emit(f"{d} = __rlwimi({d}, {self.gpr(a[1])}, {int(a[2], 0)}, {int(a[3], 0)}, {int(a[4], 0)});")

    def op_cmpwi(self, a, rec):
        self.emit(f"cr0 = (Sint32){self.gpr(a[0])} < (Sint32)({self.imm(a[1])}) ? -1 : "
                  f"(Sint32){self.gpr(a[0])} > (Sint32)({self.imm(a[1])}) ? 1 : 0;")

    def op_cmpw(self, a, rec):
        x, y = self.gpr(a[0]), self.gpr(a[1])
        self.emit(f"cr0 = (Sint32){x} < (Sint32){y} ? -1 : (Sint32){x} > (Sint32){y} ? 1 : 0;")

    # branches
    def op_b(self, a, rec):
        self.emit(f"goto {self.label(a[0])};")

    def op_bdnz(self, a, rec):
        self.emit(f"if (--ctr != 0) goto {self.label(a[0])};")

    def branch(self, a, cond):
        self.emit(f"if ({cond}) goto {self.label(a[0])};")

    def op_blt(self, a, rec):
        self.branch(a, "cr0 < 0")

    def op_bgt(self, a, rec):
        self.branch(a, "cr0 > 0")

    def op_ble(self, a, rec):
        self.branch(a, "cr0 <= 0")

    def op_bge(self, a, rec):
        self.branch(a, "cr0 >= 0")

    def op_bne(self, a, rec):
        self.branch(a, "cr0 != 0")

    def op_beq(self, a, rec):
        self.branch(a, "cr0 == 0")

    def op_mtctr(self, a, rec):
        self.emit(f"ctr = {self.gpr(a[0])};")

    # special registers
    def op_mtspr(self, a, rec):
        spr = a[0].strip()
        if spr not in GQR_SPR:
            raise ValueError(f"mtspr {spr}")
        self.emit(f"re4_cri_gqr[{GQR_SPR[spr]}] = {self.gpr(a[1])};")

    def op_mfspr(self, a, rec):
        spr = a[1].strip()
        if spr not in GQR_SPR:
            raise ValueError(f"mfspr {spr}")
        self.emit(f"{self.gpr(a[0], True)} = re4_cri_gqr[{GQR_SPR[spr]}];")

    # cache
    def op_dcbt(self, a, rec):
        self.emit("/* dcbt */")

    def op_dcbz(self, a, rec):
        self.emit(f"__dcbz({self.base(a[0])}, {self.gpr(a[1])});")

    # integer memory (host byte order)
    def op_lwz(self, a, rec):
        disp, base, _ = self.mem(a[1])
        self.emit(f"{self.gpr(a[0], True)} = *(Uint32 *)({base} + (Uint32)({disp}));")

    def op_lwzx(self, a, rec):
        self.emit(f"{self.gpr(a[0], True)} = *(Uint32 *)({self.base(a[1])} + {self.gpr(a[2])});")

    def op_lhz(self, a, rec):
        disp, base, _ = self.mem(a[1])
        self.emit(f"{self.gpr(a[0], True)} = *(Uint16 *)({base} + (Uint32)({disp}));")

    def op_stw(self, a, rec):
        disp, base, _ = self.mem(a[1])
        self.emit(f"*(Uint32 *)({base} + (Uint32)({disp})) = {self.gpr(a[0])};")

    # float memory
    def op_lfs(self, a, rec):
        disp, base, _ = self.mem(a[1])
        f = self.fpr(a[0])
        self.emit(f"{f}.ps0 = {f}.ps1 = *(Float32 *)({base} + (Uint32)({disp}));")

    def op_lfsx(self, a, rec):
        f = self.fpr(a[0])
        self.emit(f"{f}.ps0 = {f}.ps1 = *(Float32 *)({self.base(a[1])} + {self.gpr(a[2])});")

    def op_lfd(self, a, rec):
        disp, base, _ = self.mem(a[1])
        self.emit(f"__builtin_memcpy(&{self.fpr(a[0])}.raw, (void *)({base} + (Uint32)({disp})), 8);")

    def op_stfd(self, a, rec):
        disp, base, _ = self.mem(a[1])
        self.emit(f"__builtin_memcpy((void *)({base} + (Uint32)({disp})), &{self.fpr(a[0])}.raw, 8);")

    def op_stfdx(self, a, rec):
        self.emit(f"__builtin_memcpy((void *)({self.base(a[1])} + {self.gpr(a[2])}), &{self.fpr(a[0])}.raw, 8);")

    def op_stfdu(self, a, rec):
        disp, _, reg = self.mem(a[1])
        r = self.gpr(reg, True)
        self.emit(f"{r} += (Uint32)({disp});")
        self.emit(f"__builtin_memcpy((void *){r}, &{self.fpr(a[0])}.raw, 8);")

    # paired singles
    def psq(self, a, update, store):
        f = self.fpr(a[0])
        disp, base, reg = self.mem(a[1])
        w, i = int(a[2], 0), int(a[3], 0)
        if update:
            r = self.gpr(reg, True)
            self.emit(f"{r} += (Uint32)({disp});")
            ea = r
        else:
            ea = f"{base} + (Uint32)({disp})"
        fn = "re4_psq_st" if store else "re4_psq_l"
        self.emit(f"{fn}(&{f}, (void *)({ea}), {w}, re4_cri_gqr[{i}]);")

    def op_psq_l(self, a, rec):
        self.psq(a, False, False)

    def op_psq_lu(self, a, rec):
        self.psq(a, True, False)

    def op_psq_st(self, a, rec):
        self.psq(a, False, True)

    def op_psq_stu(self, a, rec):
        self.psq(a, True, True)

    def ps2(self, a, op):
        d, x, y = self.fpr(a[0]), self.fpr(a[1]), self.fpr(a[2])
        self.emit(f"{{ Float32 a0_ = {x}.ps0 {op} {y}.ps0, a1_ = {x}.ps1 {op} {y}.ps1; "
                  f"{d}.ps0 = a0_; {d}.ps1 = a1_; }}")

    def op_ps_add(self, a, rec):
        self.ps2(a, "+")

    def op_ps_sub(self, a, rec):
        self.ps2(a, "-")

    def op_ps_mul(self, a, rec):
        self.ps2(a, "*")

    def merge(self, a, i, j):
        d, x, y = self.fpr(a[0]), self.fpr(a[1]), self.fpr(a[2])
        self.emit(f"{{ Float32 a0_ = {x}.ps{i}, a1_ = {y}.ps{j}; {d}.ps0 = a0_; {d}.ps1 = a1_; }}")

    def op_ps_merge00(self, a, rec):
        self.merge(a, 0, 0)

    def op_ps_merge01(self, a, rec):
        self.merge(a, 0, 1)

    def op_ps_merge10(self, a, rec):
        self.merge(a, 1, 0)

    def op_ps_merge11(self, a, rec):
        self.merge(a, 1, 1)

    def op_ps_sel(self, a, rec):
        # ps_sel frD, frA, frC, frB: frA >= 0 ? frC : frB
        d, x, c, b = (self.fpr(v) for v in a)
        self.emit(f"{{ Float32 a0_ = {x}.ps0 >= 0.0f ? {c}.ps0 : {b}.ps0, "
                  f"a1_ = {x}.ps1 >= 0.0f ? {c}.ps1 : {b}.ps1; {d}.ps0 = a0_; {d}.ps1 = a1_; }}")

    # --- block ------------------------------------------------------------
    def translate(self, body):
        for raw in body.split("\n"):
            code = raw.split("//")[0].split("#")[0]
            for part in code.split(";"):
                if part.strip():
                    self.translate_line(part)
        head = [f"\t/* port: the asm block in C (port/tools/whole_ppc_to_c.py) */",
                "\t{"]
        decl = []
        if self.used_gprs:
            decl.append("\tUint32 " + ", ".join(f"r{n}" for n in sorted(self.used_gprs)) + ";")
        if self.used_fprs:
            decl.append("\tre4_ps " + ", ".join(f"f{n}" for n in sorted(self.used_fprs)) + ";")
        decl.append("\tUint32 ctr = 0, ca = 0;")
        decl.append("\tSint32 cr0 = 0;")
        for name in sorted(self.c_gprs):
            decl.append(f"\tUint32 v_{name} = (Uint32)({name});")
        for reg, expr in self.entry.items():
            n = int(PHYS_GPR.match(reg).group(1))
            if n in self.used_gprs:
                decl.append(f"\t{reg} = (Uint32)({expr});")
        tail = []
        for name in sorted(self.written):
            tail.append(f"\t{name} = (__typeof__({name}))v_{name};")
        tail.append("\t(void)ctr; (void)ca; (void)cr0;")
        body = ["\t" + line for line in decl + self.lines + tail]
        return "\n".join(head + body + ["\t}"])


def translate(body, prefix, **kw):
    return Translator(prefix, **kw).translate(body)
