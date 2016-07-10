#include "sim8085.h"

pthread_cond_t io_signal = PTHREAD_COND_INITIALIZER;
pthread_cond_t io_ack = PTHREAD_COND_INITIALIZER;
pthread_mutex_t io_signal_mutex;
pthread_mutex_t io_ack_mutex;

const char * reg_map = "ABCDEHL", * flags_map = "S Z * AC * P * CY";

int GETHLADDRESS(Sim8085 * sim)  {
    return GETADDRESS(sim->REGISTER[GETREGCHAR('H')], sim->REGISTER[GETREGCHAR('L')]);
}
int GETBCADDRESS(Sim8085 * sim)  {
    return GETADDRESS(sim->REGISTER[GETREGCHAR('B')], sim->REGISTER[GETREGCHAR('C')]);
}
int GETDEADDRESS(Sim8085 * sim)  {
    return GETADDRESS(sim->REGISTER[GETREGCHAR('D')], sim->REGISTER[GETREGCHAR('E')]);
}

void MOV(Sim8085 * sim, int opcode) {
    int S = getsource(opcode), D = getdest(opcode);
    ///EITHER S IS MEMORY OR D IS  MEMORY BUT NOT BOTH
    if (ISMEM(S)) {///SOURCE = MEMORY, MOV R, M
        sim->REGISTER[regidx(D)] = sim->RAM[GETHLADDRESS(sim)];
    } else if (ISMEM(D)) { ///DEST = MEM, MOV M, R
        sim->RAM[GETHLADDRESS(sim)] = sim->REGISTER[regidx(S)];
    } else { ///SOURCE = REGISTER, DESTINATION = REGISTER
        sim->REGISTER[regidx(D)] = sim->REGISTER[regidx(S)];
    }
}

///SOURCE IS ALWAYS FROM MEMORY, BUT DESTINATION CAN BE EITHER MEMORY OR REGISTER
void MVI(Sim8085 * sim, int opcode) {
    int data = sim->RAM[sim->PC++];
    int D = getdest(opcode);
    if (ISMEM(D)) { ///MVI M, DATA
        sim->RAM[GETHLADDRESS(sim)] = data;
    } else { ///MVI REG, DATA
        sim->REGISTER[regidx(D)] = data;
    }
}

void LXI(Sim8085 * sim, int opcode) {
    int LB = sim->RAM[sim->PC++];
    int MB = sim->RAM[sim->PC++];
    int v = GETRP(opcode);
    if (v == 0) { ///BC
        sim->REGISTER[GETREGCHAR('B')] = MB;
        sim->REGISTER[GETREGCHAR('C')] = LB;
    } else if (v == 1) { ///DE
        sim->REGISTER[GETREGCHAR('D')] = MB;
        sim->REGISTER[GETREGCHAR('E')] = LB;
    } else if (v == 2) { ///HL
        sim->REGISTER[GETREGCHAR('H')] = MB;
        sim->REGISTER[GETREGCHAR('L')] = LB;
    } else if (v == 3) { ///sim->SP
        sim->SP = GETADDRESS(MB, LB);
    }
}


void SWAP_REG(Sim8085 * sim, char a, char b)  {
    int t = sim->REGISTER[GETREGCHAR(a)];
    sim->REGISTER[GETREGCHAR(a)] = sim->REGISTER[GETREGCHAR(b)];
    sim->REGISTER[GETREGCHAR(b)] = t;
}
int getzero(Sim8085 * sim) {
    return (sim->REGISTER[7] >> 6) & 1;
}
int getcarry(Sim8085 * sim) {
    return (sim->REGISTER[7] >> 0) & 1;
}
int getsign(Sim8085 * sim) {
    return (sim->REGISTER[7] >> 7) & 1;
}
int getparity(Sim8085 * sim) {
    return (sim->REGISTER[7] >> 2) & 1;
}
int getAF(Sim8085 * sim) {
    return (sim->REGISTER[7] >> 4) & 1;
}

void setzero(Sim8085 * sim) {
    sim->REGISTER[7] |= (1 << 6);
}
void resetzero(Sim8085 * sim) {
    sim->REGISTER[7] &= ~(1 << 6);
}
void setcarry(Sim8085 * sim) {
    sim->REGISTER[7] |= (1 << 0);
}
void resetcarry(Sim8085 * sim) {
    sim->REGISTER[7] &= ~(1 << 0);
}
void setsign(Sim8085 * sim) {
    sim->REGISTER[7] |= (1 << 7);
}
void resetsign(Sim8085 * sim) {
    sim->REGISTER[7] &= ~(1 << 7);
}
void setparity(Sim8085 * sim) {
    sim->REGISTER[7] |= (1 << 2);
}
void resetparity(Sim8085 * sim) {
    sim->REGISTER[7] &= ~(1 << 2);
}
void setAF(Sim8085 * sim) {
    sim->REGISTER[7] |= (1 << 4);
}
void resetAF(Sim8085 * sim) {
    sim->REGISTER[7] &= ~(1 << 4);
}

void complement_carry(Sim8085 * sim) {
    sim->REGISTER[7] ^= 1 << 0;
}
///DOESNOT AFFECT AF FLAG, AFFECTS CARRY ACCORDING TO ecarry
void updateflags(Sim8085 * sim, int v, int ecarry) {
    if ((v & 0xFF) == 0) setzero(sim);
    else resetzero(sim);
    if (ecarry) {
        if (v > 0xFF) setcarry(sim);
        else resetcarry(sim);
    }
    if ((v >> 7) & 1) setsign(sim);///CHECK IF MSB IS 1
    else resetsign(sim);
    if (popcount(v & 0xFF) & 1) resetparity(sim);
    else setparity(sim);
}
void DAD(Sim8085 * sim, int v) {
    int reg_pair = GETRP(v);
    int op1 = GETHLADDRESS(sim);
    int op2 = 0;
    if (reg_pair == 0) op2 = GETBCADDRESS(sim);
    else if (reg_pair == 1) op2 = GETDEADDRESS(sim);
    else if (reg_pair == 2) op2 = op1; ///hl pair with hl pair addition = shift left
    else if (reg_pair == 3) op2 = sim->SP;
    op1 &= 0xFFFF;
    op2 &= 0xFFFF;
    int res = op1 + op2;
    if (res > 0xFFFF) setcarry(sim);
    else resetcarry(sim);
    sim->REGISTER[GETREGCHAR('H')] = (res >> 8) & 0Xff;
    sim->REGISTER[GETREGCHAR('L')] = (res >> 0) & 0Xff;
}
///with or without carry
void ADD(Sim8085 * sim, int v) {
    int with_carry = (v >> 3) & 1;
    int op1 = sim->REGISTER[GETREGCHAR('A')] & 0xFF;
    int op2;
    int id = getsource(v);
    if (ISMEM(id)) {
        op2 = sim->RAM[GETHLADDRESS(sim)] & 0xFF;
    } else {
        op2 = sim->REGISTER[regidx(id)] & 0xFF;
    }
    int res = op1 + op2;
    int prevcarry = getcarry(sim);
    if (with_carry) res += prevcarry;
    sim->REGISTER[GETREGCHAR('A')] = res & 0xFF;
    updateflags(sim, res, 1);
    int nib = (op1 & 0xF) + (op2 & 0xF);
    if (with_carry) nib += prevcarry;
    if (nib > 0xF) setAF(sim);
    else resetAF(sim);
}

///with or without carry
void ADI(Sim8085 * sim, int v) {
    int with_carry = (v >> 3) & 1;
    int op1 = sim->REGISTER[GETREGCHAR('A')] & 0xFF;
    int op2 = sim->RAM[sim->PC++] & 0xFF;;
    int res = op1 + op2;
    int prevcarry = getcarry(sim);
    if (with_carry) res += prevcarry;
    sim->REGISTER[GETREGCHAR('A')] = res & 0xFF;
    updateflags(sim, res, 1);
    int nib = (op1 & 0xF) + (op2 & 0xF);
    if (with_carry) nib += prevcarry;
    if (nib > 0xF) setAF(sim);
    else resetAF(sim);
}
/// does not affect carry flag
void INR(Sim8085 * sim, int v) {
    int dest = getdest(v);
    int * target = ISMEM(dest) ? &sim->RAM[GETHLADDRESS(sim)] : &sim->REGISTER[regidx(dest)];
    int op = *target;
    ++*target;
    *target &= 0xFF;
    updateflags(sim, *target, 0);
    if ((op & 0xF) + 1 > 0xF) setAF(sim);
    else resetAF(sim);
}

void SETREGPAIR(Sim8085 * sim, int reg_pair, int v) {
    int MB = (v >> 8) & 0xFF, LB = v & 0xFF;
    if (reg_pair == 0) {
        sim->REGISTER[GETREGCHAR('B')] = MB;
        sim->REGISTER[GETREGCHAR('C')] = LB;
    } else if (reg_pair == 1) {
        sim->REGISTER[GETREGCHAR('D')] = MB;
        sim->REGISTER[GETREGCHAR('E')] = LB;
    } else if (reg_pair == 2) {
        sim->REGISTER[GETREGCHAR('H')] = MB;
        sim->REGISTER[GETREGCHAR('L')] = LB;
    } else if (reg_pair == 3) sim->SP = v & 0xFFFF;
}

/// does not affect any flags
void INX(Sim8085 * sim, int v) {
    int reg_pair = GETRP(v);
    int op = 0;
    if (reg_pair == 0) op = GETBCADDRESS(sim);
    else if (reg_pair == 1) op = GETDEADDRESS(sim);
    else if (reg_pair == 2) op = GETHLADDRESS(sim);
    else if (reg_pair == 3) op = sim->SP;
    op = (op + 1) & 0xFFFF;
    SETREGPAIR(sim, reg_pair, op);
}

/// does not affect any flags
void DCX(Sim8085 * sim, int v) {
    int reg_pair = GETRP(v);
    int op = 0;
    if (reg_pair == 0) op = GETBCADDRESS(sim);
    else if (reg_pair == 1) op = GETDEADDRESS(sim);
    else if (reg_pair == 2) op = GETHLADDRESS(sim);
    else if (reg_pair == 3) op = sim->SP;
    op = (op - 1) & 0xFFFF;
    SETREGPAIR(sim, reg_pair, op);
}

//if a >= b, overflow occurs else no overflow
int sub(int a, int b, int bits) {
    bits = (1 << bits) - 1;
    a &= bits;
    b &= bits;
    b = bits - b;
    b++;
    return a + b;
}

//subtracts b from a with or without carry, bit at "bits" position 
//bit is set if borrow occurs else reset
int ss(Sim8085 * sim, int a, int b, int bits, int with_carry) {
    int res = sub(a, b, bits);
    int borrow = ((res >> bits) & 1) ^ 1;
    int mx = (1 << bits) - 1;
    res &= mx;
    if (with_carry) {
        int prevcarry = getcarry(sim);
        res = sub(res, prevcarry, bits);
        borrow |= ((res >> bits) & 1) ^ 1;
        res &= mx;
    }
    res |= (borrow << bits);
    return res;
}

int mysub(Sim8085 * sim, int a, int b, int with_carry, int ecarry) {
    int r = ss(sim, a, b, 8, with_carry);
    int r2 = ss(sim, a & 0xF, b & 0xF, 4, with_carry);
    if ((r2 > 4) & 1) setAF(sim); else resetAF(sim); 
    updateflags(sim, r, ecarry);
    return r & 0xFF;
}

/// does not affect carry flag
void DCR(Sim8085 * sim, int v) {
    int dest = getdest(v);
    int * target = ISMEM(dest) ? &sim->RAM[GETHLADDRESS(sim)] : &sim->REGISTER[regidx(dest)];
    *target = mysub(sim, *target, 1, 0, 0);
}


void SUB(Sim8085 * sim, int v) {
    int with_carry = (v >> 3) & 1;
    int op1 = sim->REGISTER[GETREGCHAR('A')] & 0xFF;
    int op2;
    int id = getsource(v);
    if (ISMEM(id)) {
        op2 = sim->RAM[GETHLADDRESS(sim)] & 0xFF;
    } else {
        op2 = sim->REGISTER[regidx(id)] & 0xFF;
    }
    int r = mysub(sim, op1, op2, with_carry, 1);
    sim->REGISTER[GETREGCHAR('A')] = r;
}

///with or without borrow
void SUI(Sim8085 * sim, int v) {
    int with_carry = (v >> 3) & 1;
    int op1 = sim->REGISTER[GETREGCHAR('A')] & 0xFF;
    int op2 = sim->RAM[sim->PC++] & 0xFF;;
    int r = mysub(sim, op1, op2, with_carry, 1);
    sim->REGISTER[GETREGCHAR('A')] = r;
}

void DAA(Sim8085 * sim) {
    int * accu = &sim->REGISTER[GETREGCHAR('A')];
    *accu &= 0xFF;
    if (((*accu & 0xF) > 0x9) || getAF(sim)) {
        *accu += 0x06;
        setAF(sim); ///CARRY TO HIGHER NIBBLE
    } else resetAF(sim);
    if ((((*accu >> 4) & 0xF) > 0x9) || getcarry(sim)) {
        *accu += 0x60;
        setcarry(sim);
    } else resetcarry(sim);
    updateflags(sim, (*accu) & 0xFF, 0);
}

void XCHG(Sim8085 * sim) {
    SWAP_REG(sim, 'D', 'H');
    SWAP_REG(sim, 'E', 'L');
}
void LHLD(Sim8085 * sim) {
    int LB = sim->RAM[sim->PC++];
    int MB = sim->RAM[sim->PC++];
    int EA = GETADDRESS(MB, LB);
    sim->REGISTER[GETREGCHAR('L')] = sim->RAM[EA];
    EA++;
    sim->REGISTER[GETREGCHAR('H')] = sim->RAM[EA];
}
void SHLD(Sim8085 * sim) {
    int LB = sim->RAM[sim->PC++];
    int MB = sim->RAM[sim->PC++];
    int EA = GETADDRESS(MB, LB);
    sim->RAM[EA] = sim->REGISTER[GETREGCHAR('L')];
    EA++;
    sim->RAM[EA] = sim->REGISTER[GETREGCHAR('H')];
}
void LDA(Sim8085 * sim) {
    int LB = sim->RAM[sim->PC++];
    int MB = sim->RAM[sim->PC++];
    sim->REGISTER[GETREGCHAR('A')] = sim->RAM[GETADDRESS(MB, LB)];
}
void STA(Sim8085 * sim) {
    int LB = sim->RAM[sim->PC++];
    int MB = sim->RAM[sim->PC++];
    sim->RAM[GETADDRESS(MB, LB)] = sim->REGISTER[GETREGCHAR('A')];
}
void LDAXB(Sim8085 * sim) {
    sim->REGISTER[GETREGCHAR('A')] = sim->RAM[GETBCADDRESS(sim)];
}
void LDAXD(Sim8085 * sim) {
    sim->REGISTER[GETREGCHAR('A')] = sim->RAM[GETDEADDRESS(sim)];
}
void STAXB(Sim8085 * sim) {
    sim->RAM[GETBCADDRESS(sim)] = sim->REGISTER[GETREGCHAR('A')];
}
void STAXD(Sim8085 * sim) {
    sim->RAM[GETDEADDRESS(sim)] = sim->REGISTER[GETREGCHAR('A')];
}
void ANA(Sim8085 * sim, int v) {
    int id = getsource(v);
    int with;
    if (ISMEM(id)) {
        with = sim->RAM[GETHLADDRESS(sim)] & 0xFF;
    } else {
        with = sim->REGISTER[regidx(id)] & 0xFF;
    }
    sim->REGISTER[GETREGCHAR('A')] &= 0xFF;
    sim->REGISTER[GETREGCHAR('A')] &= with;
    resetAF(sim);
    resetcarry(sim);
    updateflags(sim, sim->REGISTER[GETREGCHAR('A')], 0);
}
void ANI(Sim8085 * sim) {
    int with = sim->RAM[sim->PC++] & 0xFF;
    sim->REGISTER[GETREGCHAR('A')] &= 0xFF;
    sim->REGISTER[GETREGCHAR('A')] &= with;
    resetAF(sim);
    resetcarry(sim);
    updateflags(sim, sim->REGISTER[GETREGCHAR('A')], 0);
}
void ORA(Sim8085 * sim, int v) {
    int id = getsource(v);
    int with;
    if (ISMEM(id)) {
        with = sim->RAM[GETHLADDRESS(sim)] & 0xFF;
    } else {
        with = sim->REGISTER[regidx(id)] & 0xFF;
    }
    sim->REGISTER[GETREGCHAR('A')] &= 0xFF;
    sim->REGISTER[GETREGCHAR('A')] |= with;
    resetAF(sim);
    resetcarry(sim);
    updateflags(sim, sim->REGISTER[GETREGCHAR('A')], 0);
}
void ORI(Sim8085 * sim) {
    int with = sim->RAM[sim->PC++] & 0xFF;
    sim->REGISTER[GETREGCHAR('A')] &= 0xFF;
    sim->REGISTER[GETREGCHAR('A')] |= with;
    resetAF(sim);
    resetcarry(sim);
    updateflags(sim, sim->REGISTER[GETREGCHAR('A')], 0);
}
void XRA(Sim8085 * sim, int v) {
    int id = getsource(v);
    int with;
    if (ISMEM(id)) {
        with = sim->RAM[GETHLADDRESS(sim)] & 0xFF;
    } else {
        with = sim->REGISTER[regidx(id)] & 0xFF;
    }
    sim->REGISTER[GETREGCHAR('A')] &= 0xFF;
    sim->REGISTER[GETREGCHAR('A')] ^= with;
    resetAF(sim);
    resetcarry(sim);
    updateflags(sim, sim->REGISTER[GETREGCHAR('A')], 0);
}
void XRI(Sim8085 * sim) {
    int with = sim->RAM[sim->PC++] & 0xFF;
    sim->REGISTER[GETREGCHAR('A')] &= 0xFF;
    sim->REGISTER[GETREGCHAR('A')] ^= with;
    resetAF(sim);
    resetcarry(sim);
    updateflags(sim, sim->REGISTER[GETREGCHAR('A')], 0);
}
void CMA(Sim8085 * sim) {
    sim->REGISTER[GETREGCHAR('A')] = (~sim->REGISTER[GETREGCHAR('A')]) & 0xFF;
}
void CMC(Sim8085 * sim) {
    sim->REGISTER[7] ^= 1;
}
void STC(Sim8085 * sim) {
    setcarry(sim);
}

///with or without carry
void RAL(Sim8085 * sim, int v) {
    int with_carry = (v >> 4) & 1;
    int * accu = &sim->REGISTER[GETREGCHAR('A')];
    int b = ((*accu) >> 7) & 1;
    *accu <<= 1;
    *accu &= 0xff;
    *accu &= ~1;
    *accu |= with_carry ? getcarry(sim) : b;
    if (b) setcarry(sim);
    else resetcarry(sim);
}

///with or without carry
void RAR(Sim8085 * sim, int v) {
    int with_carry = (v >> 4) & 1;
    int * accu = &sim->REGISTER[GETREGCHAR('A')];
    int b = (*accu >> 0) & 1;
    *accu &= 0xff;
    *accu >>= 1;
    *accu |= (with_carry ? getcarry(sim) : b) << 7;
    if (b) setcarry(sim);
    else resetcarry(sim);
}

void CMP(Sim8085 * sim, int v) {
    int id = getsource(v);
    int op2;
    if (ISMEM(id)) {
        op2 = sim->RAM[GETHLADDRESS(sim)] & 0xFF;
    } else {
        op2 = sim->REGISTER[regidx(id)] & 0xFF;
    }
    sim->REGISTER[GETREGCHAR('A')] &= 0xFF;
    int op1 = sim->REGISTER[GETREGCHAR('A')];
    mysub(sim, op1, op2, 0, 1);
}

void CPI(Sim8085 * sim) {
    int op2 = sim->RAM[sim->PC++] & 0xFF;
    sim->REGISTER[GETREGCHAR('A')] &= 0xFF;
    int op1 = sim->REGISTER[GETREGCHAR('A')];
    mysub(sim, op1, op2, 0, 1);
}

void BRANCH(Sim8085 * sim, int call) {
    int LB = sim->RAM[sim->PC++] & 0xFF;
    int MB = sim->RAM[sim->PC++] & 0xFF;
    if (call) {
        assert(sim->SP > 1);
        sim->RAM[--sim->SP] = (sim->PC >> 8) & 0xFF;
        sim->RAM[--sim->SP] = (sim->PC >> 0) & 0xFF;
    }
    sim->PC = GETADDRESS(MB, LB);
}

void BRANCH_ZERO(Sim8085 * sim, int v, int call) {
    int LB = sim->RAM[sim->PC++] & 0xFF;
    int MB = sim->RAM[sim->PC++] & 0xFF;
    if (getzero(sim) == v) {
        if (call) {
            assert(sim->SP > 1);
            sim->RAM[--sim->SP] = (sim->PC >> 8) & 0xFF;
            sim->RAM[--sim->SP] = (sim->PC >> 0) & 0xFF;
        }
        sim->PC = GETADDRESS(MB, LB);
    }
}

void BRANCH_PARITY(Sim8085 * sim, int v, int call) {
    int LB = sim->RAM[sim->PC++] & 0xFF;
    int MB = sim->RAM[sim->PC++] & 0xFF;
    if (getparity(sim) == v) {
        if (call) {
            assert(sim->SP > 1);
            sim->RAM[--sim->SP] = (sim->PC >> 8) & 0xFF;
            sim->RAM[--sim->SP] = (sim->PC >> 0) & 0xFF;
        }
        sim->PC = GETADDRESS(MB, LB);
    }
}

void BRANCH_CARRY(Sim8085 * sim, int v, int call) {
    int LB = sim->RAM[sim->PC++] & 0xFF;
    int MB = sim->RAM[sim->PC++] & 0xFF;
    if (getcarry(sim) == v) {
        if (call) {
            assert(sim->SP > 1);
            sim->RAM[--sim->SP] = (sim->PC >> 8) & 0xFF;
            sim->RAM[--sim->SP] = (sim->PC >> 0) & 0xFF;
        }
        sim->PC = GETADDRESS(MB, LB);
    }
}

void BRANCH_SIGN(Sim8085 * sim, int v, int call) {
    int LB = sim->RAM[sim->PC++] & 0xFF;
    int MB = sim->RAM[sim->PC++] & 0xFF;
    if (getsign(sim) == v) {
        if (call) {
            assert(sim->SP > 1);
            sim->RAM[--sim->SP] = (sim->PC >> 8) & 0xFF;
            sim->RAM[--sim->SP] = (sim->PC >> 0) & 0xFF;
        }
        sim->PC = GETADDRESS(MB, LB);
    }
}
void BRANCH_TO_SERVICE_ROUTINE
(Sim8085 * sim, int address) {
    sim->RAM[--sim->SP] = (sim->PC >> 8) & 0xFF;
    sim->RAM[--sim->SP] = (sim->PC >> 0) & 0xFF;
    sim->PC = address;
}
void checkInterrupts(Sim8085 * sim) {
    //enable interrupts priority_wise
    if (sim->trap) {//non-maskable
        BRANCH_TO_SERVICE_ROUTINE(sim, 0x24);
        sim->trap = 0; //reset signal, naive method(needs a REDO)
    } else if (sim->interrupts_enabled) {//maskable
        //todo: enable nested interrupts
        if (sim->rst7_5) {
            if (sim->rst7_5_enable) BRANCH_TO_SERVICE_ROUTINE(sim, 0x3c);
            sim->rst7_5 = 0;
        } else if (sim->rst6_5) {
            if (sim->rst6_5_enable) BRANCH_TO_SERVICE_ROUTINE(sim, 0x34);
            sim->rst6_5 = 0;
        } else if (sim->rst5_5) {
            sim->rst5_5 = 0;
            if (sim->rst5_5_enable) BRANCH_TO_SERVICE_ROUTINE(sim, 0x2c);
        } else if (sim->intr) {
            sim->intr = 0;
            BRANCH_TO_SERVICE_ROUTINE(sim, sim->intr_opcode_latch);
        }
    }
}
void RET(Sim8085 * sim) {
    assert(sim->SP <= 0xFFFF - 2);
    int LB = sim->RAM[sim->SP++] & 0xFF;
    int MB = sim->RAM[sim->SP++] & 0xFF;
    sim->PC = GETADDRESS(MB, LB);
}
void RET_SIGN(Sim8085 * sim, int v) {
    if (getsign(sim) == v) {
        assert(sim->SP <= 0xFFFF - 2);
        int LB = sim->RAM[sim->SP++] & 0xFF;
        int MB = sim->RAM[sim->SP++] & 0xFF;
        sim->PC = GETADDRESS(MB, LB);
    }
}
void RET_CARRY(Sim8085 * sim, int v) {
    if (getcarry(sim) == v) {
        assert(sim->SP <= 0xFFFF - 2);
        int LB = sim->RAM[sim->SP++] & 0xFF;
        int MB = sim->RAM[sim->SP++] & 0xFF;
        sim->PC = GETADDRESS(MB, LB);
    }
}

void PUSHB(Sim8085 * sim) {
    assert(sim->SP > 1);
    sim->RAM[--sim->SP] = sim->REGISTER[GETREGCHAR('B')];
    sim->RAM[--sim->SP] = sim->REGISTER[GETREGCHAR('C')];
}
void PUSHD(Sim8085 * sim) {
    assert(sim->SP > 1);
    sim->RAM[--sim->SP] = sim->REGISTER[GETREGCHAR('D')];
    sim->RAM[--sim->SP] = sim->REGISTER[GETREGCHAR('E')];
}
void PUSHH(Sim8085 * sim) {
    assert(sim->SP > 1);
    sim->RAM[--sim->SP] = sim->REGISTER[GETREGCHAR('H')];
    sim->RAM[--sim->SP] = sim->REGISTER[GETREGCHAR('L')];
}
void PUSHPSW(Sim8085 * sim) {
    assert(sim->SP > 1);
    sim->RAM[--sim->SP] = sim->REGISTER[GETREGCHAR('A')];
    sim->RAM[--sim->SP] = sim->REGISTER[7];
}
void POPPSW(Sim8085 * sim) {
    assert(sim->SP <= 0xFFFF - 2);
    sim->REGISTER[7] = sim->RAM[sim->SP++];
    sim->REGISTER[GETREGCHAR('A')] = sim->RAM[sim->SP++];
}
void POPB(Sim8085 * sim) {
    assert(sim->SP <= 0xFFFF - 2);
    sim->REGISTER[GETREGCHAR('C')] = sim->RAM[sim->SP++];
    sim->REGISTER[GETREGCHAR('B')] = sim->RAM[sim->SP++];
}
void POPD(Sim8085 * sim) {
    assert(sim->SP <= 0xFFFF - 2);
    sim->REGISTER[GETREGCHAR('E')] = sim->RAM[sim->SP++];
    sim->REGISTER[GETREGCHAR('D')] = sim->RAM[sim->SP++];
}

void POPH(Sim8085 * sim) {
    assert(sim->SP <= 0xFFFF - 2);
    sim->REGISTER[GETREGCHAR('L')] = sim->RAM[sim->SP++];
    sim->REGISTER[GETREGCHAR('H')] = sim->RAM[sim->SP++];
}
void SPHL(Sim8085 * sim) {
    sim->SP = GETHLADDRESS(sim);
}
void PCHL(Sim8085 * sim) {
    sim->PC = GETHLADDRESS(sim);
}

void XTHL(Sim8085 * sim) {
    assert(sim->SP <= 0XFFFF - 2); ///stack shouldn't be empty
    int * LB = &sim->RAM[sim->SP], * MB = &sim->RAM[sim->SP + 1];
    int * H = &sim->REGISTER[GETREGCHAR('H')];
    int * L = &sim->REGISTER[GETREGCHAR('L')];
    *H ^= *MB ^= *H ^= *MB;
    *L ^= *LB ^= *L ^= *LB;
}


void RET_PARITY(Sim8085 * sim, int v) {
    if (getparity(sim) == v) {
        assert(sim->SP <= 0xFFFF - 2);
        int LB = sim->RAM[sim->SP++] & 0xFF;
        int MB = sim->RAM[sim->SP++] & 0xFF;
        sim->PC = GETADDRESS(MB, LB);
    }
}

void RET_ZERO(Sim8085 * sim, int v) {
    if (getzero(sim) == v) {
        assert(sim->SP <= 0xFFFF - 2);
        int LB = sim->RAM[sim->SP++] & 0xFF;
        int MB = sim->RAM[sim->SP++] & 0xFF;
        sim->PC = GETADDRESS(MB, LB);
    }
}

void delay_loop() {
    for (int i = 0; i < 10000000; ++i) ; //sync
}
int read_signal(Sim8085 * sim, int address) {
    sim->address_latch = address;
    sim->iow = 0;
    pthread_mutex_lock(&io_signal_mutex);
    pthread_cond_broadcast(&io_signal);
    pthread_mutex_unlock(&io_signal_mutex);
    delay_loop(); //place your plate for your food
    return sim->data_latch;
}
void write_signal(Sim8085 * sim, int address, int data) {
    sim->address_latch = address;
    sim->data_latch = data;
    sim->iow = 1;
    pthread_mutex_lock(&io_signal_mutex);
    pthread_cond_broadcast(&io_signal);
    pthread_mutex_unlock(&io_signal_mutex);
    delay_loop();
}
void IN(Sim8085 * sim) {
    int addr = sim->RAM[sim->PC++] & 0XFF; 
    int data = read_signal(sim, addr);
    sim->REGISTER[GETREGCHAR('A')] = data;
}
void OUT(Sim8085 * sim) { 
    int addr = sim->RAM[sim->PC++] & 0XFF; 
    int value = sim->REGISTER[GETREGCHAR('A')];
    write_signal(sim, addr, value);
}
void RIM(Sim8085 * sim) {
    int val = (sim->interrupts_enabled << 3) |
        (!sim->rst7_5_enable << 2) |
        (!sim->rst6_5_enable << 1) |
        (!sim->rst5_5_enable << 0);
    sim->REGISTER[GETREGCHAR('A')] = val;
}
void SIM(Sim8085 * sim) {
    int accu = sim->REGISTER[GETREGCHAR('A')];    
    if ((accu >> 4) & 1) sim->rst7_5 = 0; //reset rst 7.5 latch
    if ((accu >> 3) & 1) {
        sim->rst5_5_enable = !(accu & 1);
        sim->rst6_5_enable = !((accu >> 1) & 1);
        sim->rst7_5_enable = !((accu >> 2) & 1);
    }
}
void RST(Sim8085 * sim, int opcode) {
    int addr = opcode & (0x7 << 3); //directly calculate the rst jump address
    BRANCH_TO_SERVICE_ROUTINE(sim, addr);
}
void EI(Sim8085 * sim) {
    sim->interrupts_enabled = 1;
   
}
void DI(Sim8085 * sim) {
    sim->interrupts_enabled = 0;
    
}
void singlestep(Sim8085 * sim) {
    int v = sim->RAM[sim->PC++] & 0xFF;
    if (v == 0) return ; ///NOP INSTRUCTION
    if (is_hlt(v)) sim->HALT = 1; ///HLT
    ///data transfer
    else if (is_mov(v)) MOV(sim, v);
    else if (is_mvi(v)) MVI(sim, v);
    else if (is_lxi(v)) LXI(sim, v);
    else if (is_lda(v)) LDA(sim);
    else if (is_sta(v)) STA(sim);
    else if (is_lhld(v)) LHLD(sim);
    else if (is_shld(v)) SHLD(sim);
    else if (is_ldaxb(v)) LDAXB(sim);
    else if (is_ldaxd(v)) LDAXD(sim);
    else if (is_staxb(v)) STAXB(sim);
    else if (is_staxd(v)) STAXD(sim);
    else if (is_xchg(v)) XCHG(sim);
    ///arithmetic
    else if (is_add(v)) ADD(sim, v);
    else if (is_adi(v)) ADI(sim, v);
    else if (is_sub(v)) SUB(sim, v);
    else if (is_sbi(v)) SUI(sim, v);
    else if (is_dad(v)) DAD(sim, v);
    else if (is_inr(v)) INR(sim, v);
    else if (is_dcr(v)) DCR(sim, v);
    else if (is_inx(v)) INX(sim, v);
    else if (is_dcx(v)) DCX(sim, v);
    else if (is_daa(v)) DAA(sim);
    ///logical
    else if (is_ana(v)) ANA(sim, v);
    else if (is_ani(v)) ANI(sim);
    else if (is_ora(v)) ORA(sim, v);
    else if (is_ori(v)) ORI(sim);
    else if (is_xra(v)) XRA(sim, v);
    else if (is_xri(v)) XRI(sim);
    else if (is_cma(v)) CMA(sim);
    else if (is_cmc(v)) CMC(sim);
    else if (is_stc(v)) STC(sim);
    else if (is_cmp(v)) CMP(sim, v);
    else if (is_cpi(v)) CPI(sim);
    else if (is_rotleft(v)) RAL(sim, v);
    else if (is_rotright(v)) RAR(sim, v);
    ///branching
    else if (is_jmp(v)) BRANCH(sim, 0);
    else if (is_jnz(v)) BRANCH_ZERO(sim, 0, 0);
    else if (is_jz(v)) BRANCH_ZERO(sim, 1, 0);
    else if (is_jnc(v)) BRANCH_CARRY(sim, 0, 0);
    else if (is_jc(v)) BRANCH_CARRY(sim, 1, 0);
    else if (is_jpe(v)) BRANCH_PARITY(sim, 1, 0);
    else if (is_jpo(v)) BRANCH_PARITY(sim, 0, 0);
    else if (is_jp(v)) BRANCH_SIGN(sim, 0, 0);
    else if (is_jm(v)) BRANCH_SIGN(sim, 1, 0);
    else if (is_call(v)) BRANCH(sim, 1);
    else if (is_cnz(v)) BRANCH_ZERO(sim, 0, 1);
    else if (is_cz(v)) BRANCH_ZERO(sim, 1, 1);
    else if (is_cnc(v)) BRANCH_CARRY(sim, 0, 1);
    else if (is_cc(v)) BRANCH_CARRY(sim, 1, 1);
    else if (is_cpe(v)) BRANCH_PARITY(sim, 1, 1);
    else if (is_cpo(v)) BRANCH_PARITY(sim, 0, 1);
    else if (is_cp(v)) BRANCH_SIGN(sim, 0, 1);
    else if (is_cm(v)) BRANCH_SIGN(sim, 1, 1);
    else if (is_ret(v)) RET(sim);
    else if (is_rnz(v)) RET_ZERO(sim, 0);
    else if (is_rz(v)) RET_ZERO(sim, 1);
    else if (is_rnc(v)) RET_CARRY(sim, 0);
    else if (is_rc(v)) RET_CARRY(sim, 1);
    else if (is_rpe(v)) RET_PARITY(sim, 1);
    else if (is_rpo(v)) RET_PARITY(sim, 0);
    else if (is_rp(v)) RET_SIGN(sim, 0);
    else if (is_rm(v)) RET_SIGN(sim, 1);
    ///stack + misc
    else if (is_pushb(v)) PUSHB(sim);
    else if (is_pushd(v)) PUSHD(sim);
    else if (is_pushh(v)) PUSHH(sim);
    else if (is_pushpsw(v)) PUSHPSW(sim);
    else if (is_popb(v)) POPB(sim);
    else if (is_popd(v)) POPD(sim);
    else if (is_poph(v)) POPH(sim);
    else if (is_poppsw(v)) POPPSW(sim);
    else if (is_sphl(v)) SPHL(sim);
    else if (is_xthl(v)) XTHL(sim);
    else if (is_pchl(v)) PCHL(sim);
    else if (is_in(v)) IN(sim);
    else if (is_out(v)) OUT(sim);
    //interrupts
    else if (is_ei(v)) EI(sim);
    else if (is_di(v)) DI(sim);
    else if (is_sim(v)) SIM(sim);
    else if (is_rim(v)) RIM(sim);
    else if (is_rst(v)) RST(sim, v);

    //interrupt check logic
    checkInterrupts(sim);
}
void loadprogram(Sim8085 * sim, int start_addr, ProgramFile * program_file) {
    sim->lines.count = 0;
    sim->err_list.count = 0;
    sim->labels.count = 0;
    sim->byte_list.count = 0;
    sim->interrupts_enabled = 0;
    sim->rst5_5_enable = sim->rst6_5_enable = sim->rst7_5_enable = 0;
    sim->rst5_5 = sim->rst6_5 = sim->rst7_5 = sim->trap = sim->intr = 0;
    sim->iow = sim->address_latch = sim->data_latch = 0;
    sim->intr_opcode_latch = 0;
    sim->HALT = 0;
    sim->PC = sim->START_ADDRESS = start_addr;
    sim->SP = 0xFFFF;
    
    for (int i = 0; i < (1 << 16); ++i) sim->line_no[i] = -1; //initially each address in RAM is not associated to any line of the program_file
    program_parse(program_file, &sim->byte_list, &sim->labels, &sim->err_list, sim->START_ADDRESS);
    if (sim->err_list.count == 0) {
        for (int i = 0, ADDR = sim->PC; i < sim->byte_list.count; ++i) {
            int paddr = sim->byte_list.bytes[i][2];
            sim->RAM[paddr] = sim->byte_list.bytes[i][0];//load opcode value
            sim->line_no[paddr] = sim->byte_list.bytes[i][1]; //load line number of the program from which this code was extracted
            char value[3], ad[5];
            //printf("%d ", paddr);
            //value[2] = 0;
            //value[1] = hexchar(sim->RAM[paddr] & 0xF); 
            //value[0] = hexchar((sim->RAM[paddr] >> 4) & 0xF); 
            //ad[4] = 0;
            //ad[3] = hexchar(paddr & 0xF); 
            //ad[2] = hexchar((paddr >> 4) & 0xF); 
            //ad[1] = hexchar((paddr >> 8) & 0xF);
            //ad[0] = hexchar((paddr >> 12) & 0xF);
            //printf("Line: %d Addr: %s val: %s\n", sim->line_no[paddr], ad, value);
        }
    }

}
void stat(Sim8085 * sim) {
    printf("sim->PC: %d, sim->SP: %d\n", sim->PC, sim->SP);
    for (int i = 0; i < 7; ++i) {
        printf("%c: %d ", reg_map[i], sim->REGISTER[i] & 0xFF);
    }
    putchar('\n');
    printf("S: %d Z: %d AC: %d P: %d C: %d\n", getsign(sim), getzero(sim), getAF(sim), getparity(sim), getcarry(sim));
}

//rest

int hasHalted(Sim8085 * sim) {
    return sim->HALT;
}
int getIL(Sim8085 * sim) {
    assert(sim->PC <= 0xFFFF);
    if (sim->line_no[sim->PC] < 0) {
        printf("addr: %d, Fallthrough : Executing instruction was not written by user\n", sim->PC);
    }
    return sim->line_no[sim->PC];
}
int readMemory(Sim8085 * sim, int addr) {
    assert(addr >= 0 && addr < (1 << 16));
    return sim->RAM[addr] & 0xFF;
}
void writeMemory(Sim8085 * sim, int addr, int val) {
    assert(addr >= 0 && addr < (1 << 16));
    assert(val >= 0 && val <= 0xFF);
    sim->RAM[addr] = val;
}
