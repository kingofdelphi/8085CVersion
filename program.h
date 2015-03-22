#ifndef PROGRAM_H
#define PROGRAM_H
#include <string.h>

#define MAX_PROGRAM_SIZE 1000 //in bytes
#define MAX_LABELS 50
#define MAX_LINES 1000
#define MAX_LINE_LENGTH 255
typedef struct {
    int bytes[MAX_PROGRAM_SIZE][3]; //first column gives the opcode, 2nd gives the program line, 3rd column gives the address as now
                                    //program may not be in contiguous space
    int count;
} ByteList;
typedef struct {
    char err_list[1000][255];
    int count;
} ErrorList;
typedef struct {
    char name[255];
    int address;
} Label;
typedef struct {
    Label label[MAX_LABELS];
    int count;
} LabelList;

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int count;
} ProgramFile;

void addToList(ByteList * bytelist, int opcode, int line, int address);

int findInLabelList(LabelList * labellist, const char * s, int n);
void addLabelToLabelList(LabelList * labellist, const char * s, int n, int addr);
#endif
