#include "program.h"

void addToList(ByteList * bytelist, int opcode, int line) {
    bytelist->bytes[bytelist->count][0] = opcode;
    bytelist->bytes[bytelist->count][1] = line;
    bytelist->count++;
}

int findInLabelList(LabelList * labellist, const char * s, int n) {
    char tmp[255] = {0};
    strncat(tmp, s, n);
    for (int i = 0; i < labellist->count; ++i) {
        if (strcmp(labellist->label[i].name, tmp) == 0) return i;
    }
    return -1;
}

void addLabelToLabelList(LabelList * labellist, const char * s, int n, int addr) {
    labellist->label[labellist->count].address = addr;
    labellist->label[labellist->count].name[0] = 0;
    strncat(labellist->label[labellist->count].name, s, n);
    labellist->count++;
}
