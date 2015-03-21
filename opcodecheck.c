#include "opcodecheck.h"

int is_jnz(int v) {
    return v == 0xc2;
}

int is_jz(int v) {
    return v == 0xca;
}

int is_jc(int v) {
    return v == 0xda;
}

int is_jnc(int v) {
    return v == 0xd2;
}

int is_jpe(int v) {
    return v == 0xea;
}

int is_jpo(int v) {
    return v == 0xe2;
}

int is_jp(int v) {
    return v == 0xf2;
}

int is_jm(int v) {
    return v == 0xfa;
}

///call
int is_call(int v) {
    return v == 0xcd;
}
int is_cnz(int v) {
    return v == 0xc4;
}


int is_cz(int v) {
    return v == 0xcc;
}

int is_cc(int v) {
    return v == 0xdc;
}

int is_cnc(int v) {
    return v == 0xd4;
}

int is_cpe(int v) {
    return v == 0xec;
}

int is_cpo(int v) {
    return v == 0xe4;
}

int is_cp(int v) {
    return v == 0xf4;
}

int is_cm(int v) {
    return v == 0xfc;
}
int is_ret(int v) {
    return v == 0xc9;
}

int is_rz(int v) {
    return v == 0xc8;
}

int is_rnz(int v) {
    return v == 0xc0;
}
int is_rc(int v) {
    return v == 0xd8;
}
int is_rnc(int v) {
    return v == 0xd0;
}
int is_rpe(int v) {
    return v == 0xe8;
}
int is_rpo(int v) {
    return v == 0xe0;
}
int is_rp(int v) {
    return v == 0xf0;
}
int is_rm(int v) {
    return v == 0xf8;
}
int is_pushb(int v) {
    return v == 0xc5;
}
int is_pushd(int v) {
    return v == 0xd5;
}
int is_pushh(int v) {
    return v == 0xe5;
}
int is_pushpsw(int v) {
    return v == 0xf5;
}
int is_popb(int v) {
    return v == 0xc1;
}
int is_popd(int v) {
    return v == 0xd1;
}
int is_poph(int v) {
    return v == 0xe1;
}
int is_poppsw(int v) {
    return v == 0xf1;
}
int is_sphl(int v) {
    return v == 0xf9;
}
int is_xthl(int v) {
    return v == 0xe3;
}

int is_pchl(int v) {
    return v == 0xe9;
}
int is_in(int v) {
    return v == 0xdb;
}
int is_out(int v) {
    return v == 0xd3;
}
int is_hlt(int v) {
    return v == 0x76;
}



int is_lda(int v) {
    return v == 0x3A;
}
int is_sta(int v) {
    return v == 0x32;
}
int is_lhld(int v) {
    return v == 0x2A;
}
int is_shld(int v) {
    return v == 0x22;
}
int is_ldaxb(int v) {
    return v == 0x0a;
}
int is_ldaxd(int v) {
    return v == 0x1a;
}
int is_staxb(int v) {
    return v == 0x02;
}
int is_staxd(int v) {
    return v == 0x12;
}
int is_xchg(int v) {
    return v == 0XEB;
}
int is_add(int v) {
    return (v >> 4) == 0x08;
}
int is_adi(int v) {
    return ISMEM(getsource(v)) && ((v >> 4) == 0xC);
}
int is_sbi(int v) {
    return ISMEM(getsource(v)) && ((v >> 4) == 0xD);
}

int is_sub(int v) {
    return (v >> 4) == 0x09;
}

int is_inr(int v) {
    return ((v >> 6) == 0) && ((v & 0x07) == 0x04);
}

int is_dcr(int v) {
    return ((v >> 6) == 0) && ((v & 0x07) == 0x05);
}

int is_inx(int v) {
    return ((v >> 6) == 0) && ((v & 0x0F) == 0x03);
}
int is_dcx(int v) {
    return ((v >> 6) == 0) && ((v & 0x0F) == 0x0B);
}
int is_daa(int v) {
    return v == 0x27;
}
int is_ana(int v) {
    return (((v >> 4) & 0xF) == 0x0A) && (((v >> 3) & 1) == 0);
}

int is_ani(int v) {
    return v == 0xe6;
}
int is_ora(int v) {
    return (((v >> 4) & 0xF) == 0x0B) && (((v >> 3) & 1) == 0);
}
int is_ori(int v) {
    return v == 0xf6;
}
int is_xra(int v) {
    return (((v >> 4) & 0xF) == 0x0A) && (((v >> 3) & 1) == 1);
}
int is_xri(int v) {
    return v == 0xee;
}
int is_cma(int v) {
    return v == 0x2f;
}
int is_cmc(int v) {
    return v == 0x3f;
}
int is_stc(int v) {
    return v == 0x37;
}

int is_jmp(int v) {
    return v == 0xc3;
}

int is_cmp(int v) {
    return (((v >> 4) & 0xF) == 0x0B) && (((v >> 3) & 1) == 1);
}

int is_rotleft(int v) {
    return (((v >> 5) & 0x7) == 0) && (v & 0xF) == 0x7;
}

int is_rotright(int v) {
    return (((v >> 5) & 0x7) == 0) && (v & 0xF) == 0xF;
}

int is_cpi(int v) {
    return v == 0xFE;
}
int is_mov(int v) {
    if (ISMEM(getsource(v)) && ISMEM(getdest(v))) return 0; ///BOTH SOURCE AND DEST CAN'T BE MEMORY
    return ((v >> 6) & 0x03) == 0x01;
}
int is_mvi(int v) {
    return ((v >> 6) == 0) && ISMEM((getsource(v)));
}
int is_lxi(int v) {
    return ((v >> 6) == 0) && ((v & 0xF) == 0x01);
}
int is_dad(int v) {
    return ((v >> 6) == 0) && ((v & 0xF) == 0x09);
}
