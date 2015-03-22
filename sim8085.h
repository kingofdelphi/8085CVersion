#ifndef SIM_8085_H
#define SIM_8085_H
#include "all.h"
#include "program.h"
typedef struct {
    ///FLAGS REGISTER IS IN REGISTER[7]
    int RAM[1 << 16], REGISTER[8], SP, PC, START_ADDRESS, IOPORTS[1 << 8];
    int line_no[1 << 16];
    ProgramFile lines;
    ErrorList err_list;
    LabelList labels;
    ByteList byte_list;
    int HALT, interrupts_enabled;
    //boolean values provides information whether
    //each interrupt pins are available
    int rst7_5_enable, rst6_5_enable, rst5_5_enable;
    int trap, rst7_5, rst6_5, rst5_5; //interrupt pins
} Sim8085; ///struct end

int GETHLADDRESS(Sim8085 * ) ;
int GETBCADDRESS(Sim8085 * ) ;
int GETDEADDRESS(Sim8085 * ) ;
void MOV(Sim8085 *, int opcode);
///SOURCE IS ALWAYS FROM MEMORY, BUT DESTINATION CAN BE EITHER MEMORY OR REGISTER
void MVI(Sim8085 * , int opcode);
void LXI(Sim8085 * , int opcode);
void SWAP_REG(Sim8085 *, char a, char b);
int getzero(Sim8085 * ) ;
int getcarry(Sim8085 * ) ;
int getsign(Sim8085 * );
int getparity(Sim8085 * );
int getAF(Sim8085 * ) ;
void setzero(Sim8085 * );
void resetzero(Sim8085 * );
void setcarry(Sim8085 * );
void resetcarry(Sim8085 * );
void setsign(Sim8085 * );
void resetsign(Sim8085 * );
void setparity(Sim8085 * );
void resetparity(Sim8085 * );
void setAF(Sim8085 * );
void resetAF(Sim8085 * );

void complement_carry(Sim8085 * );
///DOESNOT AFFECT AF FLAG, AFFECTS CARRY ACCORDING TO ecarry
void updateflags(Sim8085 *, int v, int ecarry);
void DAD(Sim8085 * sim, int v);
///with or without carry
void ADD(Sim8085 * sim, int v);
///with or without carry
void ADI(Sim8085 * sim, int v);
/// does not affect carry flag
void INR(Sim8085 * sim, int v);
void SETREGPAIR(Sim8085 * sim, int reg_pair, int v);
/// does not affect any flags
void INX(Sim8085 * sim, int v);
/// does not affect any flags
void DCX(Sim8085 * sim, int v);
/// does not affect carry flag
void DCR(Sim8085 * sim, int v);
///with or without carry
void SUB(Sim8085 * sim, int v);

///with or without borrow
void SUI(Sim8085 * sim, int v);
void DAA(Sim8085 * );
void XCHG(Sim8085 * );
void LHLD(Sim8085 * );
void SHLD(Sim8085 * );
void LDA(Sim8085 * );
void STA(Sim8085 * );
void LDAXB(Sim8085 * );
void LDAXD(Sim8085 * );
void STAXB(Sim8085 * );
void STAXD(Sim8085 * );
void ANA(Sim8085 * sim, int v);
void ANI(Sim8085 * );
void ORA(Sim8085 * sim, int v);
void ORI(Sim8085 * );
void XRA(Sim8085 * sim, int v);
void XRI(Sim8085 * );
void CMA(Sim8085 * );
void CMC(Sim8085 * );
void STC(Sim8085 * );
///with or without carry
void RAL(Sim8085 * sim, int v);
///with or without carry
void RAR(Sim8085 * sim, int v);
void CMP(Sim8085 * sim, int v);
void CPI(Sim8085 * );
void BRANCH(Sim8085 * sim, int call);
void BRANCH_ZERO(Sim8085 *, int v, int call);
void BRANCH_PARITY(Sim8085 *, int v, int call);
void BRANCH_CARRY(Sim8085 *, int v, int call);
void BRANCH_SIGN(Sim8085 *, int v, int call);
void RET(Sim8085 * );
void RET_SIGN(Sim8085 * sim, int v);
void RET_CARRY(Sim8085 * sim, int v);
void PUSHB(Sim8085 * );
void PUSHD(Sim8085 * );
void PUSHH(Sim8085 * );
void PUSHPSW(Sim8085 * );
void POPPSW(Sim8085 * );
void POPB(Sim8085 * );
void POPD(Sim8085 * );
void POPH(Sim8085 * );
void SPHL(Sim8085 * );
void PCHL(Sim8085 * );
void XTHL(Sim8085 * );
void RET_PARITY(Sim8085 * sim, int v);
void RET_ZERO(Sim8085 * sim, int v);
void IN(Sim8085 * );
void OUT(Sim8085 * );
void singlestep(Sim8085 * );
void loadprogram(Sim8085 *, int start_addr, ProgramFile * program);
void stat(Sim8085 * );
void BRANCH_TO_SERVICE_ROUTINE
(Sim8085 * sim, int address);
void checkInterrupts(Sim8085 * sim);

int getIL(Sim8085 * sim);
int readIO(Sim8085 * sim, int addr);
void writeIO(Sim8085 * sim, int addr, int val);
int readMemory(Sim8085 * sim, int addr);
void writeMemory(Sim8085 * sim, int addr, int val);

int hasHalted(Sim8085 * sim);
int programSize(Sim8085 * sim);
#endif
