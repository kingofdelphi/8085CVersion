/* todo
    label naming conventions
 */

#include "parser.h"
#include "program.h"
#include "string.h"
const char *  HMAP = "0123456789ABCDEF";

const char * ONE_BYTE[] = {
    "ADC", "ADD", "ANA", "CMA", "CMC",
    "CMP", "DAA", "DAD", "DCR", "DCX",
    "HLT", "INR", "INX", "LDAX",
    "MOV",  "NOP", "ORA", "PCHL",
    "POP", "PUSH", "RAL", "RAR", "RC",
    "RET", "RLC", "RM", "RNC", "RNZ", "RP",
    "RPE", "RPO", "RRC", "RZ", "SBB", "SPHL",
    "STAX", "STC", "SUB", "XCHG", "XRA", "XTHL",
    "EI", "DI", "SIM"//for interrupt handling
};
const char * TWO_BYTE[] = {
    "ACI", "ADI", "ANI", "CPI", "IN", "MVI",
    "ORI", "OUT", "SBI", "SUI", "XRI"

};
const char * THREE_BYTE[] = {
    "CALL", "CC", "CM", "CNC", "CNZ", "CP",
    "CPE", "CPO", "CZ", "JC", "JM", "JMP",
    "JNC", "JNZ", "JP", "JPE", "JPO", "JZ",
    "LDA", "LHLD", "LXI", "SHLD", "STA"
};
const char * BRANCH_ARR[] = {
    "CALL", "CC", "CM", "CNC", "CNZ", "CP",
    "CPE", "CPO", "CZ", "JC", "JM", "JMP",
    "JNC", "JNZ", "JP", "JPE", "JPO", "JZ",
};
int is_hex_char(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F');
}
//string must end with an H
int inhex(const char * s, int n) {
    if (s == 0 || n == 0 || s[n - 1] != 'H') return 0;
    for (int i = 0; i < n - 1; ++i) if (!is_hex_char(s[i])) return 0;
    return 1;
}
int is_valid_no(const char * s, int n) {
    for (int i = 0; i < n; ++i) if (!isdigit(s[i])) return 0;
    return 1;
}
int toint(const char * s, int len, char temp_error[]) {
    if (!is_valid_no(s, len)) {
        strcat(temp_error, "Non-digit on a number field");
        return 0;
    }
    int n = 0;
    int i = 0;
    while (i < len && s[i] == '0') i++;
    if (len - i >= 8) {
        strcat(temp_error, "Overflow");
        return 0;
    }
    for (; i < len; ++i) {
        n = n * 10 + s[i] - 48;
    }
    return n;
}
char hexchar(int v) {
    assert(v >= 0 && v <= 0XF);
    return HMAP[v];
}
int NSTRCMP(const char * a, const char * b, int bn) {
    char tmp[255] = {0};
    strncat(tmp, b, bn);
    return strcmp(a, tmp);
}
int IS_ONE_BYTE_INST(const char * s, int n) {
    for (int i = 0; i < sizeof(ONE_BYTE) / sizeof(*ONE_BYTE); ++i) {
        if (NSTRCMP(ONE_BYTE[i], s, n) == 0) return 1;
    }
    return 0;
}
int IS_TWO_BYTE_INST(const char * s, int n) {
    for (int i = 0; i < sizeof(TWO_BYTE) / sizeof(*TWO_BYTE); ++i) {
        if (NSTRCMP(TWO_BYTE[i], s, n) == 0) return 1;
    }
    return 0;
}

int IS_THREE_BYTE_INST(const char * s, int n) {
    for (int i = 0; i < sizeof(THREE_BYTE) / sizeof(*THREE_BYTE); ++i) {
        if (NSTRCMP(THREE_BYTE[i], s, n) == 0) return 1;
    }
    return 0;
}
//branch to a label
int IS_BRANCH_INST(const char * s, int n) {
    for (int i = 0; i < sizeof(BRANCH_ARR) / sizeof(*BRANCH_ARR); ++i) {
        if (NSTRCMP(BRANCH_ARR[i], s, n) == 0) return 1;
    }
    return 0;
}

int isdelims(char ch) {
    return ch == ',' || ch == '$' || ch == ':';
}
void gettoken(const char * s, int * i, int * tok_s, int * tok_e) {
    *tok_s = *tok_e = 0;
    int n = strlen(s);
    while (*i < n && isspace(s[*i])) ++*i;
    if (*i < n && isdelims(s[*i])) {
        *tok_s = *i;
        *tok_e = *i + 1;
        ++*i;
        if (NSTRCMP("$", s + *tok_s, 1) == 0) *i = n; //do not parse rest of comment line
        return ;
    }
    *tok_s = *tok_e = *i;
    while (*i < n && !isspace(s[*i]) && !isdelims(s[*i])) {
        ++*tok_e;
        ++*i;
    }
    while (*i < n && isspace(s[*i])) ++*i;
    if (*i < n && s[*i] == ':') {
        *tok_e = *i + 1;
        ++*i;
    }
}

const char * findinstr(const char * s, char ch, int n) {
    for (int i = 0; i < n; ++i) if (s[i] == ch) return s + i;
    return 0;
}

void resolve_line(const char * line, LabelList * labels, ErrorList * err_list, int * addr, char to_resolve[][255], int * to_res_n) {
    int marker = 0;
    int tok_s, tok_e;
    gettoken(line, &marker, &tok_s, &tok_e);
    if (tok_s == tok_e || NSTRCMP("$", line + tok_s, tok_e - tok_s) == 0) return;
    const char * ptr = findinstr(line + tok_s, ':', tok_e - tok_s);
    if (ptr) { //A LABEL
        tok_e--;
        while (tok_e > tok_s && isspace(line[tok_e - 1])) --tok_e;
        if (tok_e == tok_s) {
            sprintf(err_list->err_list[err_list->count], "Empty Label name");
            err_list->count++;
            return ;
        }
        if (findInLabelList(labels, line + tok_s, tok_e - tok_s) != -1) {
            sprintf(err_list->err_list[err_list->count], "Multiple labels: ");
            strncat(err_list->err_list[err_list->count], line + tok_s, tok_e - tok_s);
            err_list->count++;
            return ;
        }
        addLabelToLabelList(labels, line + tok_s, tok_e - tok_s, *addr);
        gettoken(line, &marker, &tok_s, &tok_e);
        if (tok_s == tok_e || NSTRCMP("$", line + tok_s, tok_e - tok_s) == 0) return; //only a label on a line
    }
    if (IS_ONE_BYTE_INST(line + tok_s, tok_e - tok_s)) ++*addr;
    else if (IS_TWO_BYTE_INST(line + tok_s, tok_e - tok_s)) *addr += 2;
    else if (IS_THREE_BYTE_INST(line + tok_s, tok_e - tok_s)) {
        if (IS_BRANCH_INST(line + tok_s, tok_e - tok_s)) {
            gettoken(line, &marker, &tok_s, &tok_e);
            if (tok_s == tok_e) {
                sprintf(err_list->err_list[err_list->count], "Invalid operand: label missing");
                err_list->count++;
                return;
            }
            if (findInLabelList(labels, line + tok_s, tok_e - tok_s) == -1) {

                to_resolve[*to_res_n][0] = 0;
                strncat(to_resolve[*to_res_n], line + tok_s, tok_e - tok_s);
                ++*to_res_n;
            }
        }
        *addr += 3;
    } else {
        sprintf(err_list->err_list[err_list->count], "Undefined instruction: ");
        strncat(err_list->err_list[err_list->count], line + tok_s, tok_e - tok_s);
        err_list->count++;
    }
}
//identifies any undefined instructions
void resolveLabels(ProgramFile * lines, LabelList * labels, ErrorList * err_list, int start_addr) {
    char to_resolve[100][255]; //labels that need to be resolved at the next pass
    int to_res_n = 0;
    int addr = start_addr;

    for (int i = 0; i < lines->count; ++i) {
        char lnmsg[255];
        sprintf(lnmsg, " at line %d: ", i);
        int pc = err_list->count;
        resolve_line(lines->lines[i], labels, err_list, &addr, to_resolve, &to_res_n);
        for (int j = pc; j < err_list->count; ++j) {
            strcat(err_list->err_list[j], lnmsg);
        }
    }
    for (int i = 0; i < to_res_n; ++i) {
        if (findInLabelList(labels, to_resolve[i], strlen(to_resolve[i])) == -1) {
            sprintf(err_list->err_list[err_list->count], "Undefined label: %s", to_resolve[i]);
            err_list->count++;
        }
    }
}

int get_id(const char * s, int n) {
    char t[255] = {0};
    strncat(t, s, n);
    if (strcmp(t, "A") == 0) return 7;
    if (strcmp(t, "B") == 0) return 0;
    if (strcmp(t, "C") == 0) return 1;
    if (strcmp(t, "D") == 0) return 2;
    if (strcmp(t, "E") == 0) return 3;
    if (strcmp(t, "H") == 0) return 4;
    if (strcmp(t, "L") == 0) return 5;
    if (strcmp(t, "M") == 0) return 6;
    return -1; // invalid operand s
}
//this function parses a single line of instruction
void inst_parse(const char * line, LabelList * labels, int line_no, ByteList * byte_list, char temp_error[]) {
    int marker = 0, tok_s, tok_e;
    gettoken(line, &marker, &tok_s, &tok_e);
    if (NSTRCMP("$", line + tok_s, tok_e - tok_s) == 0) return ; //empty list
    if (findinstr(line + tok_s, ':', tok_e - tok_s)) { //A LABEL
        tok_e--;
        while (tok_e > tok_s && isspace(line[tok_e - 1])) --tok_e;
        if (tok_e == tok_s) {
            sprintf(temp_error, "Emtpy label name");
            return ;
        }
        gettoken(line, &marker, &tok_s, &tok_e);
        if (tok_s == tok_e || NSTRCMP("$", line + tok_s, tok_e - tok_s) == 0) return ;
    }
    if (IS_ONE_BYTE_INST(line + tok_s, tok_e - tok_s)) {
        if (NSTRCMP("ADC", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = (17 << 3) + r;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("ADD", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = (0x8 << 4) + r;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("ANA", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = (0xA << 4) + r;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("CMA", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0X2F, line_no);
        } else if (NSTRCMP("CMC", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x3f, line_no);
        } else if (NSTRCMP("CMP", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = (23 << 3) + r;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("DAA", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x27, line_no);
        } else if (NSTRCMP("DAD", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int opcode;
            if (NSTRCMP("B", line + tok_s, tok_e - tok_s) == 0) opcode = 0;
            else if (NSTRCMP("D", line + tok_s, tok_e - tok_s) == 0) opcode = 1;
            else if (NSTRCMP("H", line + tok_s, tok_e - tok_s) == 0) opcode = 2;
            else if (NSTRCMP("SP", line + tok_s, tok_e - tok_s) == 0) opcode = 3;
            else {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            opcode = (opcode << 4) + 0x9;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("DCR", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = 0x05 + (r << 3);
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("DCX", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int opcode;
            if (NSTRCMP("B", line + tok_s, tok_e - tok_s) == 0) opcode = 0;
            else if (NSTRCMP("D", line + tok_s, tok_e - tok_s) == 0) opcode = 1;
            else if (NSTRCMP("H", line + tok_s, tok_e - tok_s) == 0) opcode = 2;
            else if (NSTRCMP("SP", line + tok_s, tok_e - tok_s) == 0) opcode = 3;
            else {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            opcode = (opcode << 4) + 0xB;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("HLT", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x76, line_no);
        } else if (NSTRCMP("INR", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = (r << 3) + 0x04;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("INX", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int opcode;
            if (NSTRCMP("B", line + tok_s, tok_e - tok_s) == 0) opcode = 0;
            else if (NSTRCMP("D", line + tok_s, tok_e - tok_s) == 0) opcode = 1;
            else if (NSTRCMP("H", line + tok_s, tok_e - tok_s) == 0) opcode = 2;
            else if (NSTRCMP("SP", line + tok_s, tok_e - tok_s) == 0) opcode = 3;
            else {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            opcode = (opcode << 4) + 0x3;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("LDAX", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int opcode;
            if (NSTRCMP("B", line + tok_s, tok_e - tok_s) == 0) opcode = 0;
            else if (NSTRCMP("D", line + tok_s , tok_e - tok_s) == 0) opcode = 1;
            else {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            opcode = (opcode << 4) + 0xA;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("MOV", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int tok2s, tok2e;
            gettoken(line, &marker, &tok2s, &tok2e);
            if (NSTRCMP(",", line + tok2s, tok2e - tok2s) != 0) {
                strcat(temp_error, ", missing");
                return ;
            }
            gettoken(line, &marker, &tok2s, &tok2e);
            if (NSTRCMP("M", line + tok_s, tok_e - tok_s) == 0 && NSTRCMP("M", line + tok2s, tok2e - tok2s) == 0) {
                sprintf(temp_error, "Invalid operation: Memory to memory");
                return ;
            }
            int tr = get_id(line + tok2s, tok2e - tok2s);
            if (tr == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok2s, tok2e - tok2s);
                return ;
            }
            int opcode = (0x1 << 6) + (r << 3) + tr;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("NOP", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x00, line_no);
        } else if (NSTRCMP("ORA", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            int opcode = (0xB << 4) + r;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("PCHL", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xe9, line_no);
        } else if (NSTRCMP("POP", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int opcode;
            if (NSTRCMP("B", line + tok_s, tok_e - tok_s) == 0) opcode = 0;
            else if (NSTRCMP("D", line + tok_s, tok_e - tok_s) == 0) opcode = 1;
            else if (NSTRCMP("H", line + tok_s, tok_e - tok_s) == 0) opcode = 2;
            else if (NSTRCMP("PSW", line + tok_s, tok_e - tok_s) == 0) opcode = 3;
            else {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            opcode = (0x3 << 6) + (opcode << 4) + 0x1;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("PUSH", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int opcode;
            if (NSTRCMP("B", line + tok_s, tok_e - tok_s) == 0) opcode = 0;
            else if (NSTRCMP("D", line + tok_s, tok_e - tok_s) == 0) opcode = 1;
            else if (NSTRCMP("H", line + tok_s, tok_e - tok_s) == 0) opcode = 2;
            else if (NSTRCMP("PSW", line + tok_s, tok_e - tok_s) == 0) opcode = 3;
            else {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            opcode = (0x3 << 6) + (opcode << 4) + 0x5;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("RAL", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x17, line_no);
        } else if (NSTRCMP("RAR", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x1f, line_no);
        } else if (NSTRCMP("RC", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xd8, line_no);
        } else if (NSTRCMP("RET", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xc9, line_no);
        } else if (NSTRCMP("RLC", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x7, line_no);
        } else if (NSTRCMP("RM", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xf8, line_no);
        } else if (NSTRCMP("RNC", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xd0, line_no);
        } else if (NSTRCMP("RNZ", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xc0, line_no);
        } else if (NSTRCMP("RP", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xf0, line_no);
        } else if (NSTRCMP("RPE", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xe8, line_no);
        } else if (NSTRCMP("RPO", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xe0, line_no);
        } else if (NSTRCMP("RRC", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x0f, line_no);
        } else if (NSTRCMP("RZ", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xc8, line_no);
        } else if (NSTRCMP("SBB", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = (19 << 3) + r;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("SPHL", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xf9, line_no);
        } else if (NSTRCMP("STAX", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int opcode = 0;
            if (NSTRCMP("B", line + tok_s, tok_e - tok_s) == 0) opcode = 0;
            else if (NSTRCMP("D", line + tok_s,tok_e - tok_s) == 0) opcode = 1;
            else {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            opcode = (opcode << 4) + 0x2;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("STC", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0x37, line_no);
        } else if (NSTRCMP("SUB", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = (0x9 << 4) + r;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("XCHG", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xeb, line_no);
        } else if (NSTRCMP("XRA", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            int opcode = (21 << 3) + r;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("XTHL", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xe3, line_no);
        } else if (NSTRCMP("EI", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xfb, line_no);
        } else if (NSTRCMP("DI", line + tok_s, tok_e - tok_s) == 0) {
            addToList(byte_list, 0xf3, line_no);
        }

    } else if (IS_TWO_BYTE_INST(line + tok_s, tok_e - tok_s)) {
        if (NSTRCMP("ACI", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0XCE, line_no);
        else if (NSTRCMP("ADI", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0XC6, line_no);
        else if (NSTRCMP("ANI", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0XE6, line_no);
        else if (NSTRCMP("CPI", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0Xfe, line_no);
        else if (NSTRCMP("IN", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0Xdb, line_no);
        else if (NSTRCMP("MVI", line + tok_s, tok_e - tok_s) == 0) {
            gettoken(line, &marker, &tok_s, &tok_e);
            int r = get_id(line + tok_s, tok_e - tok_s);
            if (r == -1) {
                sprintf(temp_error, "Invalid operand ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            gettoken(line, &marker, &tok_s, &tok_e);
            if (NSTRCMP(",", line + tok_s, tok_e - tok_s) != 0) {
                strcat(temp_error, ", missing");
                return ;
            }
            int opcode = (r << 3) + 0x06;
            addToList(byte_list, opcode, line_no);
        } else if (NSTRCMP("ORI", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0xf6, line_no);
        else if (NSTRCMP("OUT", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0xd3, line_no);
        else if (NSTRCMP("SBI", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0xde, line_no);
        else if (NSTRCMP("SUI", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0xd6, line_no);
        else if (NSTRCMP("XRI", line + tok_s, tok_e - tok_s) == 0)
            addToList(byte_list, 0xee, line_no);
        gettoken(line, &marker, &tok_s, &tok_e);
        if (tok_s == tok_e) {
            strcat(temp_error, "Second operand missing");
            return ;
        }
        int v;
        if (inhex(line + tok_s, tok_e - tok_s)) {
            v = hextodecimal(line + tok_s, tok_e - tok_s - 1);
        } else {
            v = toint(line + tok_s, tok_e - tok_s, temp_error);
            if (temp_error[0]) return ;
        }
        if (v > 0xFF) {
            strcat(temp_error, "Overflow");
            return ;
        }
        addToList(byte_list, v, line_no);
    } else if (IS_THREE_BYTE_INST(line + tok_s, tok_e - tok_s)) {
        if (IS_BRANCH_INST(line + tok_s, tok_e - tok_s)) {
            if (NSTRCMP("CALL", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xcd, line_no);
            else if (NSTRCMP("CC", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xdc, line_no);
            else if (NSTRCMP("CM", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xfc, line_no);
            else if (NSTRCMP("CNC", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xd4, line_no);
            else if (NSTRCMP("CNZ", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xc4, line_no);
            else if (NSTRCMP("CP", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xf4, line_no);
            else if (NSTRCMP("CPE", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xec, line_no);
            else if (NSTRCMP("CPO", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xe4, line_no);
            else if (NSTRCMP("CZ", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xcc, line_no);
            else if (NSTRCMP("JC", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xda, line_no);
            else if (NSTRCMP("JM", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xfa, line_no);
            else if (NSTRCMP("JMP", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xc3, line_no);
            else if (NSTRCMP("JNC", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xd2, line_no);
            else if (NSTRCMP("JNZ", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xc2, line_no);
            else if (NSTRCMP("JP", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xf2, line_no);
            else if (NSTRCMP("JPE", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xea, line_no);
            else if (NSTRCMP("JPO", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xe2, line_no);
            else if (NSTRCMP("JZ", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0xca, line_no);
            gettoken(line, &marker, &tok_s, &tok_e);
            int v;
            if ((v = findInLabelList(labels, line + tok_s, tok_e - tok_s)) == -1) {
                sprintf(temp_error, "Undefined label ");
                strncat(temp_error, line + tok_s, tok_e - tok_s);
                return ;
            }
            v = labels->label[v].address;
            addToList(byte_list, v & 0xFF, line_no);
            addToList(byte_list, (v >> 8) & 0Xff, line_no);
        } else {
            if (NSTRCMP("LDA", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0x3a, line_no);
            else if (NSTRCMP("LHLD", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0x2a, line_no);
            else if (NSTRCMP("LXI", line + tok_s, tok_e - tok_s) == 0) {
                gettoken(line, &marker, &tok_s, &tok_e);
                int opcode;
                if (NSTRCMP("B", line + tok_s, tok_e - tok_s) == 0) opcode = 0;
                else if (NSTRCMP("D", line + tok_s,tok_e - tok_s) == 0) opcode = 1;
                else if (NSTRCMP("H", line + tok_s, tok_e - tok_s) == 0) opcode = 2;
                else if (NSTRCMP("SP", line + tok_s, tok_e - tok_s) == 0) opcode = 3;
                else {
                    sprintf(temp_error, "Invalid operand ");
                    strncat(temp_error, line + tok_s, tok_e - tok_s);
                    return ;
                }
                gettoken(line, &marker, &tok_s, &tok_e);
                if (NSTRCMP(",", line + tok_s, tok_e - tok_s) != 0) {
                    strcat(temp_error, ", missing");
                    return ;
                }
                opcode = (opcode << 4) + 0x1;
                addToList(byte_list, opcode, line_no);
            } else if (NSTRCMP("SHLD", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0x22, line_no);
            else if (NSTRCMP("STA", line + tok_s, tok_e - tok_s) == 0)
                addToList(byte_list, 0x32, line_no);
            gettoken(line, &marker, &tok_s, &tok_e);
            if (tok_s == tok_e) {
                strcat(temp_error, "Operand missing");
                return ;
            }
            int v;
            if (inhex(line + tok_s, tok_e - tok_s)) {
                v = hextodecimal(line + tok_s, tok_e - tok_s - 1);
            } else {
                v = toint(line + tok_s, tok_e - tok_s, temp_error);
                if (*temp_error) return ;
            }
            if (v > 0xFFFF) {
                strcat(temp_error, "Overflow");
                return ;
            }
            addToList(byte_list, v & 0xFF, line_no);
            addToList(byte_list, (v >> 8) & 0xff, line_no);
        }
    }
    gettoken(line, &marker, &tok_s, &tok_e);
    if (tok_s != tok_e && NSTRCMP("$", line + tok_s, tok_e - tok_s) != 0) {
        strcat(temp_error, "Extra insertions");
        return ;
    }
}
void parse(ProgramFile * lines, LabelList * labels, ErrorList * err_list, ByteList * byte_list) {
    char temp_error[255];
    for (int i = 0; i < lines->count; ++i) {
        if (strlen(lines->lines[i]) == 0) continue;
        temp_error[0] = 0;
        inst_parse(lines->lines[i], labels, i, byte_list, temp_error);
        if (temp_error[0]) {
            sprintf(err_list->err_list[err_list->count], "Error at line %d: %s", i, temp_error);
            err_list->count++;
        }
    }
}

void program_parse
(ProgramFile * program, ByteList * byte_list, LabelList * labels, ErrorList * err_list, int start_addr) {
    //commented out to make the program case sensitive[all instructions should be in uppercase]
//    for (int i = 0; i < program->count; ++i) {
//        uppercase(program->lines[i]);
//    }
    resolveLabels(program, labels, err_list, start_addr);
    byte_list->count = 0;
    parse(program, labels, err_list, byte_list);
}
