#ifndef SIM_PIPE_H
#define SIM_PIPE_H

#include "machine.h"

/* define values related to operands, all possible combinations are included */
struct port_t{
    int srcA;      /* input 1 register number */
    int srcB;      /* input 2 register number */
    int srcC;      /* input 3 register number */
    int dstE;      /* output 1 register number */
    int dstM;      /* output 2 register number */
} ;

/*define buffer between fetch and decode stage*/
struct ifid_buf {
    md_inst_t inst;   /* instruction that has been fetched */
    md_addr_t PC;     /* pc value of current instruction */
    md_addr_t valP;    /* the next instruction to fetch */
};

/*define buffer between decode and execute stage*/
struct idex_buf {
    md_inst_t inst;   /* instruction in ID stage */
    md_addr_t PC;     /* pc value of current instruction */
    enum md_opcode opcode; /* operation number */
    enum md_fu_class res;
    int flags;
    struct port_t port;  /* operand */
    int valA;
    int valB;
    int inStall;
};

/*define buffer between execute and memory stage*/
struct exmem_buf{
    md_inst_t inst;   /* instruction in EX stage */
    md_addr_t PC;     /* pc value of current instruction */
    enum md_opcode opcode; /* operation number */
    enum md_fu_class res;
    int flags;
    struct port_t port;  /* operand */
    int valA;
    int valE;
    int cond;   /* need jump */
};

/*define buffer between memory and writeback stage*/
struct memwb_buf{
    md_inst_t inst;   /* instruction in MEM stage */
    md_addr_t PC;     /* pc value of current instruction */
    enum md_opcode opcode; /* operation number */
    enum md_fu_class res;
    int flags;
    struct port_t port;  /* operand */
    int valM;
    int valE;
};

/*do fetch stage*/
void do_if();

/*do decode stage*/
void do_id();

/*do execute stage*/
void do_ex();

/*do memory stage*/
void do_mem();

/*do write_back to register*/
void do_wb();

#define MD_FETCH_INSTI(INST, MEM, PC)         \
    { INST.a = MEM_READ_WORD(mem, (PC));      \
      INST.b = MEM_READ_WORD(mem, (PC) + sizeof(word_t)); }

#define SET_OPCODE(OP, INST) ((OP) = ((INST).a & 0xff))

#define RSI(INST)   (INST.b >> 24& 0xff)      /* reg source #1 */
#define RTI(INST)   ((INST.b >> 16) & 0xff)   /* reg source #2 */
#define RDI(INST)   ((INST.b >> 8) & 0xff)    /* reg dest */

#define IMMI(INST)  ((int)((short)(INST.b & 0xffff))) /* get immediate value */
#define TARGI(INST) (INST.b & 0x3ffffff)    /*jump target*/

#endif
