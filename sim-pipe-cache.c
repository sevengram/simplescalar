#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* An implementation of 5-stage classic pipeline simulation */

/* don't count instructions flag, enabled by default, disable for inst count */
#undef NO_INSN_COUNT

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "sim.h"
#include "sim-pipe.h"
#include "sim-pipe-cache.h"

#define USE_CACHE

/* simulated registers */
static struct regs_t regs;

/* simulated memory */
static struct mem_t *mem = NULL;

/* register simulator-specific options */
void sim_reg_options(struct opt_odb_t *odb)
{
    opt_reg_header(odb,
                   "sim-pipe: This simulator implements based on sim-fast.\n");
}

/* check simulator-specific option values */
void sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
    if (dlite_active)
        fatal("sim-pipe does not support DLite debugging");
}

/* register simulator-specific statistics */
void sim_reg_stats(struct stat_sdb_t *sdb)
{
#ifndef NO_INSN_COUNT
    stat_reg_counter(sdb, "sim_num_insn",
                     "total number of instructions executed",
                     &sim_num_insn, sim_num_insn, NULL);
#endif /* !NO_INSN_COUNT */
    stat_reg_int(sdb, "sim_elapsed_time",
                 "total simulation time in seconds",
                 &sim_elapsed_time, 0, NULL);
#ifndef NO_INSN_COUNT
    stat_reg_formula(sdb, "sim_inst_rate",
                     "simulation speed (in insts/sec)",
                     "sim_num_insn / sim_elapsed_time", NULL);
#endif /* !NO_INSN_COUNT */
    ld_reg_stats(sdb);
    mem_reg_stats(mem, sdb);
}

struct ifid_buf fd;
struct idex_buf de;
struct exmem_buf em;
struct memwb_buf mw;

struct cache_set c_set[16];

#define DNA          (0)

/* general register dependence decoders */
#define DGPR(N)      (N)
#define DGPR_D(N)    ((N) &~1)

/* floating point register dependence decoders */
#define DFPR_L(N)   (((N)+32)&~1)
#define DFPR_F(N)   (((N)+32)&~1)
#define DFPR_D(N)   (((N)+32)&~1)

/* miscellaneous register dependence decoders */
#define DHI     (0+32+32)
#define DLO     (1+32+32)
#define DFCC    (2+32+32)
#define DTMP    (3+32+32)

/* initialize the simulator */
void sim_init(void)
{
    /* allocate and initialize register file */
    regs_init(&regs);

    /* allocate and initialize memory space */
    mem = mem_create("mem");
    mem_init(mem);
}

/* load program into simulated state */
void sim_load_prog(char *fname,    /* program to load */
                   int argc, char **argv,  /* program arguments */
                   char **envp)    /* program environment */
{
    /* load program text and data, set up environment, memory, and regs */
    ld_load_prog(fname, argc, argv, envp, &regs, mem, TRUE);
}

/* print simulator-specific configuration information */
void sim_aux_config(FILE *stream)
{
    /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
void sim_aux_stats(FILE *stream)
{
    /* nada */
}

/* un-initialize simulator-specific state */
void sim_uninit(void)
{
    /* nada */
}


/*
 * configure the execution engine
 */

/* next program counter */
#define SET_NPC(EXPR)   (regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC     (regs.regs_PC)

/* general purpose registers */
#define GPR(N)      (regs.regs_R[N])
#define SET_GPR(N,EXPR)   (regs.regs_R[N] = (EXPR))
#define DECLARE_FAULT(EXP)  { break; }

#ifdef TARGET_PISA
/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_L(N)    (regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR) (regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N)    (regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR) (regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N)    (regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N,EXPR) (regs.regs_F.d[(N) >> 1] = (EXPR))

/* miscellaneous register accessors */
#define SET_HI(EXPR)    (regs.regs_C.hi = (EXPR))
#define HI      (regs.regs_C.hi)
#define SET_LO(EXPR)    (regs.regs_C.lo = (EXPR))
#define LO      (regs.regs_C.lo)
#define FCC     (regs.regs_C.fcc)
#define SET_FCC(EXPR)   (regs.regs_C.fcc = (EXPR))
#endif /* TARGET_PISA */

/* precise architected memory state accessor macros */
#define READ_BYTE(SRC, FAULT)           \
    ((FAULT) = md_fault_none, MEM_READ_BYTE(mem, (SRC)))
#define READ_HALF(SRC, FAULT)           \
    ((FAULT) = md_fault_none, MEM_READ_HALF(mem, (SRC)))
#define READ_WORD(SRC, FAULT)           \
    ((FAULT) = md_fault_none, MEM_READ_WORD(mem, (SRC)))

#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)            \
    ((FAULT) = md_fault_none, MEM_READ_QWORD(mem, (SRC)))
#endif /* HOST_HAS_QWORD */

#define WRITE_BYTE(SRC, DST, FAULT)         \
    ((FAULT) = md_fault_none, MEM_WRITE_BYTE(mem, (DST), (SRC)))
#define WRITE_HALF(SRC, DST, FAULT)         \
    ((FAULT) = md_fault_none, MEM_WRITE_HALF(mem, (DST), (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)         \
    ((FAULT) = md_fault_none, MEM_WRITE_WORD(mem, (DST), (SRC)))

#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)          \
    ((FAULT) = md_fault_none, MEM_WRITE_QWORD(mem, (DST), (SRC)))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST) sys_syscall(&regs, mem_access, mem, INST, TRUE)

#ifndef NO_INSN_COUNT
#define INC_INSN_CTR()  sim_num_insn++
#else /* !NO_INSN_COUNT */
#define INC_INSN_CTR()  /* nada */
#endif /* NO_INSN_COUNT */

/* start simulation, program loaded, processor precise state initialized */
void sim_main(void)
{
    fprintf(stderr, "sim: ** starting *pipe-cache* functional simulation **\n");

    /* must have natural byte/word ordering */
    if (sim_swap_bytes || sim_swap_words)
        fatal("sim: *pipe* functional simulation cannot swap bytes or words");

    fd.valP = regs.regs_PC;

    while (TRUE) {
        cycle_count++;

        /* maintain $r0 semantics */
        regs.regs_R[MD_REG_ZERO] = 0;

        /* keep an instruction count */
#ifndef NO_INSN_COUNT
        sim_num_insn++;
#endif /* !NO_INSN_COUNT */

        do_wb();

        do_mem();

        do_ex();

        do_id();

        do_if();
//
//        if (cycle_count > 400)
//            break;
    }
}

void do_if()
{
    fd.PC = fd.valP;
    MD_FETCH_INSTI(fd.inst, mem, fd.PC);
    MD_SET_OPCODE(fd.opcode, fd.inst);

    fd.valP = fd.PC + sizeof(md_inst_t);
}

void do_id()
{
    enum md_fault_type _fault;

    if (em.cond == 1) {
        de.PC = fd.PC;
        de.inst = fd.inst;
        de.opcode = OP_NA;
        de.flags = 0;
        de.res = FUClass_NA;
        de.port.srcA = DNA;
        de.port.srcB = DNA;
        de.port.srcC = DNA;
        de.port.dstE = DNA;
        de.port.dstM = DNA;
        de.valA = 0;
        de.valB = 0;
    } else {
        de.PC = fd.PC;
        de.inst = fd.inst;
        de.opcode = fd.opcode;
    }

    if (de.opcode == OP_NA) {
        return;
    }

    md_inst_t inst = de.inst;

    /* execute the instruction */
    switch (de.opcode) {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)		\
	case OP:				  \
        de.flags = FLAGS;     \
	    de.res = RES;         \
        de.port.dstE = DNA;   \
        de.port.dstM = DNA;   \
	    if (de.res == IntALU) \
            de.port.dstE = O1;     \
	    else if (de.res == RdPort) \
	        de.port.dstM = O1;     \
        de.port.srcA = I1;     \
        de.port.srcB = I2;     \
        de.port.srcC = I3;     \
        if (de.port.srcA != DNA && de.port.srcA == em.port.dstE) \
            de.valA = em.valE;  \
        else if (de.port.srcA != DNA && de.port.srcA == em.port.dstM) \
            de.valA = READ_WORD(em.valE, _fault);   \
        else if (de.port.srcA != DNA && de.port.srcA == mw.port.dstE) \
            de.valA = mw.valE;  \
        else if (de.port.srcA != DNA && de.port.srcA == mw.port.dstM) \
            de.valA = mw.valM;  \
        else    \
            de.valA = GPR(I1);  \
        if (de.port.srcB != DNA && de.port.srcB == em.port.dstE) \
            de.valB = em.valE;  \
        else if (de.port.srcB != DNA && de.port.srcB == em.port.dstM)  \
            de.valB = READ_WORD(em.valE, _fault);   \
        else if (de.port.srcB != DNA && de.port.srcB == mw.port.dstE)  \
            de.valB = mw.valE;  \
        else if (de.port.srcB != DNA && de.port.srcB == mw.port.dstM)  \
            de.valB = mw.valM;  \
        else    \
            de.valB = GPR(I2);  \
        break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)	\
	case OP:							\
	    panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#include "machine.def"
    default:
        panic("attempted to execute a bogus opcode");
    }

    if (de.opcode == JUMP) {
        SET_TPC((fd.PC & 036000000000) | (TARG << 2));
        SET_NPC((fd.PC & 036000000000) | (TARG << 2));
        fd.valP = regs.regs_NPC;
    }
}

void do_ex()
{
    em.PC = de.PC;
    em.inst = de.inst;
    em.opcode = de.opcode;
    em.flags = de.flags;
    em.res = de.res;
    em.port = de.port;
    em.valA = de.valA;
    em.valE = 0;
    em.cond = 0;

    if (em.opcode == OP_NA) {
        return;
    }

    md_inst_t inst = em.inst;
    /* execute the instruction */

    switch (em.opcode) {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)		\
	case OP:				     \
        SYMCAT(OP,_IMPL_PIPE);  \
        break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)	\
	case OP:							\
	    panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#include "machine.def"
    default:
        panic("attempted to execute a bogus opcode");
    }
}

void do_mem()
{
    enum md_fault_type _fault;
    mw.opcode = em.opcode;
    mw.PC = em.PC;
    mw.inst = em.inst;
    mw.flags = em.flags;
    mw.res = em.res;
    mw.port = em.port;
    mw.valE = em.valE;
    mw.valM = 0;

    switch (mw.res) {
    case WrPort:
        write_word((word_t)em.valA, em.valE);
        break;
    case RdPort:
        mw.valM = read_word(em.valE);
        break;
    default:
        break;
    }
}

void do_wb()
{
    if (mw.port.dstE != DNA)
        SET_GPR(mw.port.dstE, mw.valE);
    if (mw.port.dstM != DNA)
        SET_GPR(mw.port.dstM, mw.valM);

    if (mw.opcode == SYSCALL) {
        SYSCALL(mw.inst);
    }
}

void write_back(struct cache_line* line, int index)
{
    int i;
    enum md_fault_type _fault;
    md_addr_t base_addr = GET_BASE(line->tag, index);
    for (i = 0; i < 4; i++) {
        WRITE_WORD(line->data[i], base_addr + 4 * i, _fault);
        if (_fault != md_fault_none) {
            fprintf(stderr, "WRITE_WORD error: %d\n", _fault);
            DECLARE_FAULT(_fault);
        }
    }
    line->dirty = 0;
}

void cache_load(md_addr_t base_addr)
{
    int i;
    int index = GET_SET(base_addr);
    enum md_fault_type _fault;
    READ_WORD(base_addr + 4 * i, _fault);
    int pos = (c_set[index].cur_pos++) % 4;
    if (c_set[index].c_line[pos].valid) {
        cache_replace_count++;
        if (c_set[index].c_line[pos].dirty) {
            write_back(&c_set[index].c_line[pos], index);
            cache_wb_count++;
        }
    }
    c_set[index].c_line[pos].tag = GET_TAG(base_addr);
    c_set[index].c_line[pos].dirty = 0;
    c_set[index].c_line[pos].valid = 1;
    for (i = 0; i < 4; i++) {
        c_set[index].c_line[pos].data[i] = READ_WORD(base_addr + 4 * i, _fault);
        if (_fault != md_fault_none) {
            fprintf(stderr, "READ_WORD error: %d\n", _fault);
            DECLARE_FAULT(_fault);
        }
    }
}

bool_t read_cache(word_t *result, md_addr_t addr)
{
    int i;
    int index = GET_SET(addr);
    int tag = GET_TAG(addr);
    for (i = 0; i < 4; i++) {
        if (c_set[index].c_line[i].valid && c_set[index].c_line[i].tag == tag) {
            *result = c_set[index].c_line[i].data[GET_OFFSET(addr)];
            return TRUE;
        }
    }
    return FALSE;
}

bool_t write_cache(word_t data, md_addr_t addr)
{
    int i;
    int index = GET_SET(addr);
    int tag = GET_TAG(addr);
    for (i = 0; i < 4; i++) {
        if (c_set[index].c_line[i].valid && c_set[index].c_line[i].tag == tag) {
            c_set[index].c_line[i].data[GET_OFFSET(addr)] = data;
            c_set[index].c_line[i].dirty = 1;
            return TRUE;
        }
    }
    return FALSE;
}

word_t read_word(md_addr_t addr)
{
 //   printf("read - addr:0x%x, set:%d, tag:%d, offset:%d\n", addr, GET_SET(addr), GET_TAG(addr),GET_OFFSET(addr));
    word_t result;
    enum md_fault_type _fault;
#ifdef USE_CACHE
    if (read_cache(&result, addr)) {
        cache_hit_count++;
        return result;
    } else {
        cycle_count += 9;
        cache_miss_count++;
        mem_access_count++;
        result = READ_WORD(addr, _fault);
        if (_fault != md_fault_none)
            fprintf(stderr, "READ_WORD error: %d\n", _fault);
        cache_load(GET_BASEADDR(addr));
        return result;
    }
#else
    cycle_count += 9;
    mem_access_count++;
    result = READ_WORD(addr, _fault);
    if (_fault != md_fault_none)
        fprintf(stderr, "READ_WORD error: %d\n", _fault);
    return result;
#endif
}

void write_word(word_t data, md_addr_t addr)
{
 //   printf("write - addr:0x%x, set:%d, tag:%d, offset:%d\n", addr, GET_SET(addr), GET_TAG(addr),GET_OFFSET(addr));
    enum md_fault_type _fault;
#ifdef USE_CACHE
    if (!write_cache(data, addr)) {
        cycle_count += 9;
        cache_miss_count++;
        mem_access_count++;
        WRITE_WORD(data, addr, _fault);
        if (_fault != md_fault_none)
            fprintf(stderr, "WRITE_WORD error: %d\n", _fault);
        cache_load(GET_BASEADDR(addr));
    } else {
        cache_hit_count++;
    }
#else
    cycle_count += 9;
    mem_access_count++;
    WRITE_WORD(data, addr, _fault);
    if (_fault != md_fault_none)
        fprintf(stderr, "WRITE_WORD error: %d\n", _fault);
#endif
}
