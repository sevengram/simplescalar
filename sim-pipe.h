#include "machine.h"

/* define values related to operands, all possible combinations are included */
typedef struct {
  int in1;			/* input 1 register number */
  int in2;			/* input 2 register number */
  int in3;			/* input 3 register number */
  int out1;			/* output 1 register number */
  int out2;			/* output 2 register number */
  /* TODO */
} oprand_t;


/*define buffer between fetch and decode stage*/
struct ifid_buf {
  md_inst_t inst;	    /* instruction that has been fetched */
  md_addr_t PC;	        /* pc value of current instruction */
  md_addr_t NPC;		/* the next instruction to fetch */
};


/*define buffer between decode and execute stage*/
struct idex_buf {
  md_addr_t PC;
  md_inst_t inst;		/* instruction in ID stage */ 
  int opcode;			/* operation number */
  oprand_t oprand;		/* operand */
  int instFlags;
  int latched;
  /* TODO */
};

/*define buffer between execute and memory stage*/
struct exmem_buf{
  md_inst_t inst;		/* instruction in EX stage */
  int needJump;
  int aluOutput;
  /* TODO */
};

/*define buffer between memory and writeback stage*/
struct memwb_buf{
  md_inst_t inst;		/* instruction in MEM stage */
  /* TODO */
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


#define MD_FETCH_INSTI(INST, MEM, PC)					\
  { INST.a = MEM_READ_WORD(mem, (PC));					\
    INST.b = MEM_READ_WORD(mem, (PC) + sizeof(word_t)); }

#define SET_OPCODE(OP, INST) ((OP) = ((INST).a & 0xff)) 

#define RSI(INST)		(INST.b >> 24& 0xff)		/* reg source #1 */
#define RTI(INST)		((INST.b >> 16) & 0xff)		/* reg source #2 */
#define RDI(INST)		((INST.b >> 8) & 0xff)		/* reg dest */

#define IMMI(INST)	((int)((/* signed */short)(INST.b & 0xffff)))	/*get immediate value*/
#define TARGI(INST)	(INST.b & 0x3ffffff)		/*jump target*/
