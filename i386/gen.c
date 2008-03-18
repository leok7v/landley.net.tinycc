/*
 *  X86 code generator for TCC
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 *  Licensed under GPLv2, see file LICENSE in this tarball.
 */

/* number of available registers */
#define NB_REGS             4

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT     0x0001 /* generic integer register */
#define RC_FLOAT   0x0002 /* generic float register */
#define RC_EAX     0x0004
#define RC_ST0     0x0008 
#define RC_ECX     0x0010
#define RC_EDX     0x0020
#define RC_IRET    RC_EAX /* function return: integer register */
#define RC_LRET    RC_EDX /* function return: second integer register */
#define RC_FRET    RC_ST0 /* function return: float register */

/* pretty names for the registers */
enum {
    TREG_EAX = 0,
    TREG_ECX,
    TREG_EDX,
    TREG_ST0,
};

int reg_classes[NB_REGS] = {
    /* eax */ RC_INT | RC_EAX,
    /* ecx */ RC_INT | RC_ECX,
    /* edx */ RC_INT | RC_EDX,
    /* st0 */ RC_FLOAT | RC_ST0,
};

/* return registers for function */
#define REG_IRET TREG_EAX /* single word int return register */
#define REG_LRET TREG_EDX /* second word return register (for long long) */
#define REG_FRET TREG_ST0 /* float return register */

/* defined if function parameters must be evaluated in reverse order */
#define INVERT_FUNC_PARAMS

/* defined if structures are passed as pointers. Otherwise structures
   are directly pushed on stack. */
//#define FUNC_STRUCT_PARAM_AS_PTR

/* pointer size, in bytes */
#define PTR_SIZE 4

/* long double size and alignment, in bytes */
#define LDOUBLE_SIZE  12
#define LDOUBLE_ALIGN 4
/* maximum alignment (for aligned attribute support) */
#define MAX_ALIGN     8

/******************************************************/
/* ELF defines */

#define EM_TCC_TARGET EM_386

/* relocation type for 32 bit data relocation */
#define R_DATA_32   R_386_32
#define R_JMP_SLOT  R_386_JMP_SLOT
#define R_COPY      R_386_COPY

#define ELF_START_ADDR 0x08048000
#define ELF_PAGE_SIZE  0x1000

/******************************************************/

static unsigned long func_sub_sp_offset;
static unsigned long func_bound_offset;
static int func_ret_sub;

/* XXX: make it faster ? */
void gen_byte(int c)
{
    int ind1;

    if (!cur_text_section) return;
    ind1 = ind + 1;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind] = c;
    ind = ind1;
}

// Output a variable number of bytes, little endian, ignoring high zero bytes.
void gen_multibyte(unsigned int c)
{
    while (c) {
        gen_byte(c);
        c = c >> 8;
    }
}

// Output 4 bytes little endian
void gen_le32(int c)
{
    gen_byte(c);
    gen_byte(c >> 8);
    gen_byte(c >> 16);
    gen_byte(c >> 24);
}

/* output a symbol and patch all calls to it */
void gsym_addr(int t, int a)
{
    int n, *ptr;
    if (!cur_text_section) return;
    while (t) {
        ptr = (int *)(cur_text_section->data + t);
        n = *ptr; /* next value */
        *ptr = a - t - 4;
        t = n;
    }
}

void gsym(int t)
{
    gsym_addr(t, ind);
}

/* psym is used to put an instruction with a data field which is a
   reference to a symbol. It is in fact the same as oad ! */
#define psym oad

/* instruction + 4 bytes data. Return the address of the data */
static int oad(int c, int s)
{
    int ind1;

    if (!cur_text_section) return 0;
    gen_multibyte(c);
    ind1 = ind + 4;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    *(int *)(cur_text_section->data + ind) = s;
    s = ind;
    ind = ind1;
    return s;
}

/* output constant with relocation if 'r & VT_SYM' is true */
static void gen_addr32(int r, Sym *sym, int c)
{
    if (r & VT_SYM)
        greloc(cur_text_section, sym, ind, R_386_32);
    gen_le32(c);
}

/* generate a modrm reference. 'op_reg' contains the addtional 3
   opcode bits */
static void gen_modrm(int op_reg, int r, Sym *sym, int c)
{
    op_reg <<= 3;
    if ((r & VT_VALMASK) == VT_CONST) {
        /* constant memory reference */
        gen_multibyte(0x05 | op_reg);  // XXX possibly gen_byte?
        gen_addr32(r, sym, c);
    } else if ((r & VT_VALMASK) == VT_LOCAL) {
        /* currently, we use only ebp as base */
        if (c == (char)c) {
            /* short reference */
            gen_multibyte(0x45 | op_reg);  // XXX possibly gen_byte?
            gen_byte(c);
        } else {
            oad(0x85 | op_reg, c);
        }
    } else {
        gen_byte(0x00 | op_reg | (r & VT_VALMASK));
    }
}


/* load 'r' from value 'sv' */
void load(int r, SValue *sv)
{
    int v, t, ft, fc, fr;
    SValue v1;

    fr = sv->r;
    ft = sv->type.t;
    fc = sv->c.ul;

    v = fr & VT_VALMASK;
    if (fr & VT_LVAL) {
        if (v == VT_LLOCAL) {
            v1.type.t = VT_INT;
            v1.r = VT_LOCAL | VT_LVAL;
            v1.c.ul = fc;
            load(r, &v1);
            fr = r;
        }
        if ((ft & VT_BTYPE) == VT_FLOAT) {
            gen_byte(0xd9); /* flds */
            r = 0;
        } else if ((ft & VT_BTYPE) == VT_DOUBLE) {
            gen_byte(0xdd); /* fldl */
            r = 0;
        } else if ((ft & VT_BTYPE) == VT_LDOUBLE) {
            gen_byte(0xdb); /* fldt */
            r = 5;
        } else if ((ft & VT_TYPE) == VT_BYTE) {
            gen_multibyte(0xbe0f);   /* movsbl */
        } else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED)) {
            gen_multibyte(0xb60f);   /* movzbl */
        } else if ((ft & VT_TYPE) == VT_SHORT) {
            gen_multibyte(0xbf0f);   /* movswl */
        } else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) {
            gen_multibyte(0xb70f);   /* movzwl */
        } else {
            gen_byte(0x8b);     /* movl */
        }
        gen_modrm(r, fr, sv->sym, fc);
    } else {
        if (v == VT_CONST) {
            gen_multibyte(0xb8 + r); /* mov $xx, r */  // XXX possibly gen_byte?
            gen_addr32(fr, sv->sym, fc);
        } else if (v == VT_LOCAL) {
            gen_byte(0x8d); /* lea xxx(%ebp), r */
            gen_modrm(r, VT_LOCAL, sv->sym, fc);
        } else if (v == VT_CMP) {
            oad(0xb8 + r, 0); /* mov $0, r */
            gen_byte(0x0f); /* setxx %br */
            gen_multibyte(fc);  // XXX possibly gen_byte?
            gen_multibyte(0xc0 + r); // XXX possibly gen_byte?
        } else if (v == VT_JMP || v == VT_JMPI) {
            t = v & 1;
            oad(0xb8 + r, t); /* mov $1, r */
            gen_multibyte(0x05eb); /* jmp after */
            gsym(fc);
            oad(0xb8 + r, t ^ 1); /* mov $0, r */
        } else if (v != r) {
            gen_byte(0x89);
            gen_multibyte(0xc0 + r + v * 8); /* mov v, r */  // XXX possibly gen_byte?
        }
    }
}

/* store register 'r' in lvalue 'v' */
void store(int r, SValue *v)
{
    int fr, bt, ft, fc;

    ft = v->type.t;
    fc = v->c.ul;
    fr = v->r & VT_VALMASK;
    bt = ft & VT_BTYPE;
    /* XXX: incorrect if float reg to reg */
    if (bt == VT_FLOAT) {
        gen_byte(0xd9); /* fsts */
        r = 2;
    } else if (bt == VT_DOUBLE) {
        gen_byte(0xdd); /* fstpl */
        r = 2;
    } else if (bt == VT_LDOUBLE) {
        gen_multibyte(0xc0d9); /* fld %st(0) */  // XXX combine?
        gen_byte(0xdb); /* fstpt */
        r = 7;
    } else {
        if (bt == VT_SHORT) gen_byte(0x66);
        else if (bt == VT_BYTE || bt == VT_BOOL) gen_byte(0x88);
        else gen_byte(0x89);
    }
    if (fr == VT_CONST ||
        fr == VT_LOCAL ||
        (v->r & VT_LVAL)) {
        gen_modrm(r, v->r, v->sym, fc);
    } else if (fr != r) {
        gen_multibyte(0xc0 + fr + r * 8); /* mov r, fr */ // XXX maybe gen_byte?
    }
}

static void gadd_sp(int val)
{
    if (val == (char)val) {
        gen_multibyte(0xc483);
        gen_byte(val);
    } else {
        oad(0xc481, val); /* add $xxx, %esp */
    }
}

/* 'is_jmp' is '1' if it is a jump */
static void gcall_or_jmp(int is_jmp)
{
    int r;
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
        /* constant case */
        if (vtop->r & VT_SYM) {
            /* relocation case */
            greloc(cur_text_section, vtop->sym, 
                   ind + 1, R_386_PC32);
        } else {
            /* put an empty PC32 relocation */
            put_elf_reloc(symtab_section, cur_text_section, 
                          ind + 1, R_386_PC32, 0);
        }
        oad(0xe8 + is_jmp, vtop->c.ul - 4); /* call/jmp im */
    } else {
        /* otherwise, indirect call */
        r = gv(RC_INT);
        gen_byte(0xff); /* call/jmp *r */
        gen_multibyte(0xd0 + r + (is_jmp << 4));  // XXX maybe gen_byte?
    }
}

static uint8_t fastcall_regs[3] = { TREG_EAX, TREG_EDX, TREG_ECX };
static uint8_t fastcallw_regs[2] = { TREG_ECX, TREG_EDX };

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */
void gfunc_call(int nb_args)
{
    int size, align, r, args_size, i, func_call;
    Sym *func_sym;
    
    args_size = 0;
    for(i = 0;i < nb_args; i++) {
        if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
            size = type_size(&vtop->type, &align);
            /* align to stack align size */
            size = (size + 3) & ~3;
            /* allocate the necessary size on stack */
            oad(0xec81, size); /* sub $xxx, %esp */
            /* generate structure store */
            r = get_reg(RC_INT);
            gen_byte(0x89); /* mov %esp, r */
            gen_multibyte(0xe0 + r);  // XXX maybe gen_byte?  Combine?
            vset(&vtop->type, r | VT_LVAL, 0);
            vswap();
            vstore();
            args_size += size;
        } else if (is_float(vtop->type.t)) {
            gv(RC_FLOAT); /* only one float register */
            if ((vtop->type.t & VT_BTYPE) == VT_FLOAT) size = 4;
            else if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE) size = 8;
            else size = 12;
            oad(0xec81, size); /* sub $xxx, %esp */
            if (size == 12) gen_multibyte(0x7cdb);
            else gen_multibyte(0x5cd9 + size - 4); /* fstp[s|l] 0(%esp) */
            gen_byte(0x24); // XXX maybe combine?
            gen_byte(0x00);
            args_size += size;
        } else {
            /* simple type (currently always same size) */
            /* XXX: implicit cast ? */
            r = gv(RC_INT);
            if ((vtop->type.t & VT_BTYPE) == VT_LLONG) {
                size = 8;
                gen_multibyte(0x50 + vtop->r2); /* push r */ // XXX maybe gen_byte?
            } else size = 4;
            gen_multibyte(0x50 + r); /* push r */ // XXX maybe gen_byte?
            args_size += size;
        }
        vtop--;
    }
    save_regs(0); /* save used temporary registers */
    func_sym = vtop->type.ref;
    func_call = func_sym->r;
    /* fast call case */
    if ((func_call >= FUNC_FASTCALL1 && func_call <= FUNC_FASTCALL3) ||
        func_call == FUNC_FASTCALLW) {
        int fastcall_nb_regs;
        uint8_t *fastcall_regs_ptr;
        if (func_call == FUNC_FASTCALLW) {
            fastcall_regs_ptr = fastcallw_regs;
            fastcall_nb_regs = 2;
        } else {
            fastcall_regs_ptr = fastcall_regs;
            fastcall_nb_regs = func_call - FUNC_FASTCALL1 + 1;
        }
        for(i = 0;i < fastcall_nb_regs; i++) {
            if (args_size <= 0)
                break;
            gen_multibyte(0x58 + fastcall_regs_ptr[i]); /* pop r */ // XXX maybe gen_byte?
            /* XXX: incorrect for struct/floats */
            args_size -= 4;
        }
    }
    gcall_or_jmp(0);
    if (args_size && func_sym->r != FUNC_STDCALL)
        gadd_sp(args_size);
    vtop--;
}

#define FUNC_PROLOG_SIZE 9

/* generate function prolog of type 't' */
void gfunc_prolog(CType *func_type)
{
    int addr, align, size, func_call, fastcall_nb_regs;
    int param_index, param_addr;
    uint8_t *fastcall_regs_ptr;
    Sym *sym;
    CType *type;

    sym = func_type->ref;
    func_call = sym->r;
    addr = 8;
    loc = 0;
    if (func_call >= FUNC_FASTCALL1 && func_call <= FUNC_FASTCALL3) {
        fastcall_nb_regs = func_call - FUNC_FASTCALL1 + 1;
        fastcall_regs_ptr = fastcall_regs;
    } else if (func_call == FUNC_FASTCALLW) {
        fastcall_nb_regs = 2;
        fastcall_regs_ptr = fastcallw_regs;
    } else {
        fastcall_nb_regs = 0;
        fastcall_regs_ptr = NULL;
    }
    param_index = 0;

    ind += FUNC_PROLOG_SIZE;
    func_sub_sp_offset = ind;
    /* if the function returns a structure, then add an
       implicit pointer parameter */
    func_vt = sym->type;
    if ((func_vt.t & VT_BTYPE) == VT_STRUCT) {
        /* XXX: fastcall case ? */
        func_vc = addr;
        addr += 4;
        param_index++;
    }
    /* define parameters */
    while ((sym = sym->next) != NULL) {
        type = &sym->type;
        size = type_size(type, &align);
        size = (size + 3) & ~3;
#ifdef FUNC_STRUCT_PARAM_AS_PTR
        /* structs are passed as pointer */
        if ((type->t & VT_BTYPE) == VT_STRUCT) {
            size = 4;
        }
#endif
        if (param_index < fastcall_nb_regs) {
            /* save FASTCALL register */
            loc -= 4;
            gen_byte(0x89);     /* movl */
            gen_modrm(fastcall_regs_ptr[param_index], VT_LOCAL, NULL, loc);
            param_addr = loc;
        } else {
            param_addr = addr;
            addr += size;
        }
        sym_push(sym->token & ~SYM_FIELD, type,
                 VT_LOCAL | VT_LVAL, param_addr);
        param_index++;
    }
    func_ret_sub = 0;
    /* pascal type call ? */
    if (func_call == FUNC_STDCALL)
        func_ret_sub = addr - 8;

    /* leave some room for bound checking code */
    if (do_bounds_check) {
        oad(0xb8, 0); /* lbound section pointer */
        oad(0xb8, 0); /* call to function */
        func_bound_offset = lbounds_section->data_offset;
    }
}

/* generate function epilog */
void gfunc_epilog(void)
{
    int v, saved_ind;

#ifdef CONFIG_TCC_BCHECK
    if (do_bounds_check && func_bound_offset != lbounds_section->data_offset) {
        int saved_ind;
        int *bounds_ptr;
        Sym *sym, *sym_data;
        /* add end of table info */
        bounds_ptr = section_ptr_add(lbounds_section, sizeof(int));
        *bounds_ptr = 0;
        /* generate bound local allocation */
        saved_ind = ind;
        ind = func_sub_sp_offset;
        sym_data = get_sym_ref(&char_pointer_type, lbounds_section, 
                               func_bound_offset, lbounds_section->data_offset);
        greloc(cur_text_section, sym_data,
               ind + 1, R_386_32);
        oad(0xb8, 0); /* mov %eax, xxx */
        sym = external_global_sym(TOK___bound_local_new, &func_old_type, 0);
        greloc(cur_text_section, sym, 
               ind + 1, R_386_PC32);
        oad(0xe8, -4);
        ind = saved_ind;
        /* generate bound check local freeing */
        gen_multibyte(0x5250); /* save returned value, if any */
        greloc(cur_text_section, sym_data,
               ind + 1, R_386_32);
        oad(0xb8, 0); /* mov %eax, xxx */
        sym = external_global_sym(TOK___bound_local_delete, &func_old_type, 0);
        greloc(cur_text_section, sym, 
               ind + 1, R_386_PC32);
        oad(0xe8, -4);
        gen_multibyte(0x585a); /* restore returned value, if any */
    }
#endif
    gen_byte(0xc9); /* leave */
    if (func_ret_sub == 0) {
        gen_byte(0xc3); /* ret */
    } else {
        gen_byte(0xc2); /* ret n */
        gen_byte(func_ret_sub);
        gen_byte(func_ret_sub >> 8);
    }
    /* align local size to word & save local variables */
    
    v = (-loc + 3) & -4; 
    saved_ind = ind;
    ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
#ifdef TINYCC_TARGET_PE
    if (v >= 4096) {
        Sym *sym = external_global_sym(TOK___chkstk, &func_old_type, 0);
        oad(0xe8, -4); /* call __chkstk, (does the stackframe too) */
        greloc(cur_text_section, sym, ind-4, R_386_PC32);
    } else
#endif
    {
        gen_multibyte(0xe58955);  /* push %ebp, mov %esp, %ebp */
        gen_multibyte(0xec81);  /* sub esp, stacksize */
    }
    gen_le32(v);
    ind = saved_ind;
}

/* generate a jump to a label */
int gjmp(int t)
{
    return psym(0xe9, t);
}

/* generate a jump to a fixed address */
void gjmp_addr(int a)
{
    int r;
    r = a - ind - 2;
    if (r == (char)r) {
        gen_byte(0xeb);
        gen_byte(r);
    } else {
        oad(0xe9, a - ind - 5);
    }
}

/* generate a test. set 'inv' to invert test. Stack entry is popped */
int gtst(int inv, int t)
{
    int v, *p;

    v = vtop->r & VT_VALMASK;
    if (v == VT_CMP) {
        /* fast case : can jump directly since flags are set */
        gen_byte(0x0f);
        t = psym((vtop->c.i - 16) ^ inv, t);
    } else if (v == VT_JMP || v == VT_JMPI) {
        /* && or || optimization */
        if ((v & 1) == inv) {
            /* insert vtop->c jump list in t */
            p = &vtop->c.i;
            while (*p != 0)
                p = (int *)(cur_text_section->data + *p);
            *p = t;
            t = vtop->c.i;
        } else {
            t = gjmp(t);
            gsym(vtop->c.i);
        }
    } else {
        if (is_float(vtop->type.t) || is_llong(vtop->type.t)) {
            /* compare != 0 to get a 32-bit int for testing */
            vpushi(0);
            gen_op(TOK_NE);
        }
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            /* constant jmp optimization */
            if ((vtop->c.i != 0) != inv) 
                t = gjmp(t);
        } else {
            v = gv(RC_INT);
            gen_byte(0x85);
            gen_multibyte(0xc0 + v * 9); // XXX maybe gen_byte?
            gen_byte(0x0f);
            t = psym(0x85 ^ inv, t);
        }
    }
    vtop--;
    return t;
}

/* generate an integer binary operation */
void gen_opi(int op)
{
    int r, fr, opc, c;

    switch(op) {
    case '+':
    case TOK_ADDC1: /* add with carry generation */
        opc = 0;
    gen_op8:
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            /* constant case */
            vswap();
            r = gv(RC_INT);
            vswap();
            c = vtop->c.i;
            if (c == (char)c) {
                /* XXX: generate inc and dec for smaller code ? */
                gen_byte(0x83);
                gen_multibyte(0xc0 | (opc << 3) | r); // XXX maybe gen_byte?
                gen_byte(c);
            } else {
                gen_byte(0x81);
                oad(0xc0 | (opc << 3) | r, c);
            }
        } else {
            gv2(RC_INT, RC_INT);
            r = vtop[-1].r;
            fr = vtop[0].r;
            gen_multibyte((opc << 3) | 0x01); // XXX maybe gen_byte?
            gen_multibyte(0xc0 + r + fr * 8);  // XXX maybe gen_byte?
        }
        vtop--;
        if (op >= TOK_ULT && op <= TOK_GT) {
            vtop->r = VT_CMP;
            vtop->c.i = op;
        }
        break;
    case '-':
    case TOK_SUBC1: /* sub with carry generation */
        opc = 5;
        goto gen_op8;
    case TOK_ADDC2: /* add with carry use */
        opc = 2;
        goto gen_op8;
    case TOK_SUBC2: /* sub with carry use */
        opc = 3;
        goto gen_op8;
    case '&':
        opc = 4;
        goto gen_op8;
    case '^':
        opc = 6;
        goto gen_op8;
    case '|':
        opc = 1;
        goto gen_op8;
    case '*':
        gv2(RC_INT, RC_INT);
        r = vtop[-1].r;
        fr = vtop[0].r;
        vtop--;
        gen_multibyte(0xaf0f); /* imul fr, r */
        gen_multibyte(0xc0 + fr + r * 8); // XXX maybe gen_byte?
        break;
    case TOK_SHL:
        opc = 4;
        goto gen_shift;
    case TOK_SHR:
        opc = 5;
        goto gen_shift;
    case TOK_SAR:
        opc = 7;
    gen_shift:
        opc = 0xc0 | (opc << 3);
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            /* constant case */
            vswap();
            r = gv(RC_INT);
            vswap();
            c = vtop->c.i & 0x1f;
            gen_byte(0xc1); /* shl/shr/sar $xxx, r */
            gen_multibyte(opc | r); // XXX maybe gen_byte?
            gen_byte(c);
        } else {
            /* we generate the shift in ecx */
            gv2(RC_INT, RC_ECX);
            r = vtop[-1].r;
            gen_byte(0xd3); /* shl/shr/sar %cl, r */
            gen_multibyte(opc | r); // XXX maybe gen_byte?
        }
        vtop--;
        break;
    case '/':
    case TOK_UDIV:
    case TOK_PDIV:
    case '%':
    case TOK_UMOD:
    case TOK_UMULL:
        /* first operand must be in eax */
        /* XXX: need better constraint for second operand */
        gv2(RC_EAX, RC_ECX);
        r = vtop[-1].r;
        fr = vtop[0].r;
        vtop--;
        save_reg(TREG_EDX);
        if (op == TOK_UMULL) {
            gen_byte(0xf7); /* mul fr */
            gen_multibyte(0xe0 + fr); // XXX maybe gen_byte, or combine?
            vtop->r2 = TREG_EDX;
            r = TREG_EAX;
        } else {
            if (op == TOK_UDIV || op == TOK_UMOD) {
                gen_multibyte(0xf7d231); /* xor %edx, %edx, div fr, %eax */
                gen_multibyte(0xf0 + fr); // XXX maybe gen_byte/combine?
            } else {
                gen_multibyte(0xf799); /* cltd, idiv fr, %eax */
                gen_multibyte(0xf8 + fr); // XXX maybe gen_byte/combine?
            }
            if (op == '%' || op == TOK_UMOD)
                r = TREG_EDX;
            else
                r = TREG_EAX;
        }
        vtop->r = r;
        break;
    default:
        opc = 7;
        goto gen_op8;
    }
}

/* generate a floating point operation 'v = t1 op t2' instruction. The
   two operands are guaranted to have the same floating point type */
/* XXX: need to use ST1 too */
void gen_opf(int op)
{
    int a, ft, fc, swapped, r;

    /* convert constants to memory references */
    if ((vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
        vswap();
        gv(RC_FLOAT);
        vswap();
    }
    if ((vtop[0].r & (VT_VALMASK | VT_LVAL)) == VT_CONST)
        gv(RC_FLOAT);

    /* must put at least one value in the floating point register */
    if ((vtop[-1].r & VT_LVAL) &&
        (vtop[0].r & VT_LVAL)) {
        vswap();
        gv(RC_FLOAT);
        vswap();
    }
    swapped = 0;
    /* swap the stack if needed so that t1 is the register and t2 is
       the memory reference */
    if (vtop[-1].r & VT_LVAL) {
        vswap();
        swapped = 1;
    }
    if (op >= TOK_ULT && op <= TOK_GT) {
        /* load on stack second operand */
        load(TREG_ST0, vtop);
        save_reg(TREG_EAX); /* eax is used by FP comparison code */
        if (op == TOK_GE || op == TOK_GT)
            swapped = !swapped;
        else if (op == TOK_EQ || op == TOK_NE)
            swapped = 0;
        if (swapped)
            gen_multibyte(0xc9d9); /* fxch %st(1) */
        gen_multibyte(0xe9da); /* fucompp */
        gen_multibyte(0xe0df); /* fnstsw %ax */ // XXX maybe combine?
        if (op == TOK_EQ) {
            gen_multibyte(0x45e480); /* and $0x45, %ah */
            gen_multibyte(0x40fC80); /* cmp $0x40, %ah */
        } else if (op == TOK_NE) {
            gen_multibyte(0x45e480); /* and $0x45, %ah */
            gen_multibyte(0x40f480); /* xor $0x40, %ah */
            op = TOK_NE; // XXX redundant?
        } else if (op == TOK_GE || op == TOK_LE) {
            gen_multibyte(0x05c4f6); /* test $0x05, %ah */
            op = TOK_EQ;
        } else {
            gen_multibyte(0x45c4f6); /* test $0x45, %ah */
            op = TOK_EQ;
        }
        vtop--;
        vtop->r = VT_CMP;
        vtop->c.i = op;
    } else {
        /* no memory reference possible for long double operations */
        if ((vtop->type.t & VT_BTYPE) == VT_LDOUBLE) {
            load(TREG_ST0, vtop);
            swapped = !swapped;
        }
        
        switch(op) {
        default:
        case '+':
            a = 0;
            break;
        case '-':
            a = 4;
            if (swapped)
                a++;
            break;
        case '*':
            a = 1;
            break;
        case '/':
            a = 6;
            if (swapped)
                a++;
            break;
        }
        ft = vtop->type.t;
        fc = vtop->c.ul;
        if ((ft & VT_BTYPE) == VT_LDOUBLE) {
            gen_byte(0xde); /* fxxxp %st, %st(1) */
            gen_multibyte(0xc1 + (a << 3)); // XXX maybe gen_byte/combine?
        } else {
            /* if saved lvalue, then we must reload it */
            r = vtop->r;
            if ((r & VT_VALMASK) == VT_LLOCAL) {
                SValue v1;
                r = get_reg(RC_INT);
                v1.type.t = VT_INT;
                v1.r = VT_LOCAL | VT_LVAL;
                v1.c.ul = fc;
                load(r, &v1);
                fc = 0;
            }

            if ((ft & VT_BTYPE) == VT_DOUBLE) gen_byte(0xdc);
            else gen_byte(0xd8);
            gen_modrm(a, r, vtop->sym, fc);
        }
        vtop--;
    }
}

/* convert integers to fp 't' type. Must handle 'int', 'unsigned int'
   and 'long long' cases. */
void gen_cvt_itof(int t)
{
    save_reg(TREG_ST0);
    gv(RC_INT);
    if ((vtop->type.t & VT_BTYPE) == VT_LLONG) {
        /* signed long long to float/double/long double (unsigned case
           is handled generically) */
        gen_multibyte(0x50 + vtop->r2); /* push r2 */  // XXX maybe gen_byte?
        gen_multibyte(0x50 + (vtop->r & VT_VALMASK)); /* push r */ /// XXX maybe gen_byte?
        gen_multibyte(0x242cdf); /* fildll (%esp) */
        gen_multibyte(0x08c483); /* add $8, %esp */
    } else if ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) == 
               (VT_INT | VT_UNSIGNED)) {
        /* unsigned int to float/double/long double */
        gen_byte(0x6a); /* push $0 */
        gen_byte(0x00);
        gen_multibyte(0x50 + (vtop->r & VT_VALMASK)); /* push r */ // XXX maybe gen_byte?
        gen_multibyte(0x242cdf); /* fildll (%esp) */
        gen_multibyte(0x08c483); /* add $8, %esp */
    } else {
        /* int to float/double/long double */
        gen_multibyte(0x50 + (vtop->r & VT_VALMASK)); /* push r */ // XXX maybe gen_byte?
        gen_multibyte(0x2404db); /* fildl (%esp) */
        gen_multibyte(0x04c483); /* add $4, %esp */
    }
    vtop->r = TREG_ST0;
}

/* convert fp to int 't' type */
/* XXX: handle long long case */
void gen_cvt_ftoi(int t)
{
    int r, r2, size;
    Sym *sym;
    CType ushort_type;

    ushort_type.t = VT_SHORT | VT_UNSIGNED;

    gv(RC_FLOAT);
    if (t != VT_INT) size = 8;
    else size = 4;
    
    gen_multibyte(0x2dd9); /* ldcw xxx */
    sym = external_global_sym(TOK___tcc_int_fpu_control, 
                              &ushort_type, VT_LVAL);
    greloc(cur_text_section, sym, 
           ind, R_386_32);
    gen_le32(0);
    
    oad(0xec81, size); /* sub $xxx, %esp */
    if (size == 4) gen_multibyte(0x1cdb); /* fistpl */
    else gen_multibyte(0x3cdf); /* fistpll */
    gen_byte(0x24); // XXX maybe combine?
    gen_multibyte(0x2dd9); /* ldcw xxx */
    sym = external_global_sym(TOK___tcc_fpu_control, 
                              &ushort_type, VT_LVAL);
    greloc(cur_text_section, sym, 
           ind, R_386_32);
    gen_le32(0);

    r = get_reg(RC_INT);
    gen_multibyte(0x58 + r); /* pop r */ // XXX maybe gen_byte?
    if (size == 8) {
        if (t == VT_LLONG) {
            vtop->r = r; /* mark reg as used */
            r2 = get_reg(RC_INT);
            gen_multibyte(0x58 + r2); /* pop r2 */ // XXX maybe gen_byte?
            vtop->r2 = r2;
        } else gen_multibyte(0x04c483); /* add $4, %esp */
    }
    vtop->r = r;
}

/* convert from one floating point type to another */
void gen_cvt_ftof(int t)
{
    /* all we have to do on i386 is to put the float in a register */
    gv(RC_FLOAT);
}

/* computed goto support */
void gen_goto(void)
{
    gcall_or_jmp(1);
    vtop--;
}

/* bound check support functions */
#ifdef CONFIG_TCC_BCHECK

/* generate a bounded pointer addition */
void gen_bounded_ptr_add(void)
{
    Sym *sym;

    /* prepare fast i386 function call (args in eax and edx) */
    gv2(RC_EAX, RC_EDX);
    /* save all temporary registers */
    vtop -= 2;
    save_regs(0);
    /* do a fast function call */
    sym = external_global_sym(TOK___bound_ptr_add, &func_old_type, 0);
    greloc(cur_text_section, sym, 
           ind + 1, R_386_PC32);
    oad(0xe8, -4);
    /* returned pointer is in eax */
    vtop++;
    vtop->r = TREG_EAX | VT_BOUNDED;
    /* address of bounding function call point */
    vtop->c.ul = (cur_text_section->reloc->data_offset - sizeof(Elf32_Rel)); 
}

/* patch pointer addition in vtop so that pointer dereferencing is
   also tested */
void gen_bounded_ptr_deref(void)
{
    int func;
    int size, align;
    Elf32_Rel *rel;
    Sym *sym;

    size = 0;
    /* XXX: put that code in generic part of tcc */
    if (!is_float(vtop->type.t)) {
        if (vtop->r & VT_LVAL_BYTE)
            size = 1;
        else if (vtop->r & VT_LVAL_SHORT)
            size = 2;
    }
    if (!size)
        size = type_size(&vtop->type, &align);
    switch(size) {
    case  1: func = TOK___bound_ptr_indir1; break;
    case  2: func = TOK___bound_ptr_indir2; break;
    case  4: func = TOK___bound_ptr_indir4; break;
    case  8: func = TOK___bound_ptr_indir8; break;
    case 12: func = TOK___bound_ptr_indir12; break;
    case 16: func = TOK___bound_ptr_indir16; break;
    default:
        error("unhandled size when derefencing bounded pointer");
        func = 0;
        break;
    }

    /* patch relocation */
    /* XXX: find a better solution ? */
    rel = (Elf32_Rel *)(cur_text_section->reloc->data + vtop->c.ul);
    sym = external_global_sym(func, &func_old_type, 0);
    if (!sym->c)
        put_extern_sym(sym, NULL, 0, 0);
    rel->r_info = ELF32_R_INFO(sym->c, ELF32_R_TYPE(rel->r_info));
}
#endif
