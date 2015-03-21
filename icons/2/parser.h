#ifndef PARSER_H
#define PARSER_H
    #include "all.h"
    #include "utility.h"
    #include "program.h"
void program_parse
(ProgramFile * program, ByteList * byte_list, LabelList * labels, ErrorList * err_list, int start_addr);
void gettoken(const char * s, int * i, int * tok_s, int * tok_e);
#endif
