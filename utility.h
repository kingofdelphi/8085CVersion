#ifndef UTILITY_H
#define UTILITY_H
#include "all.h"
int getsource(int v);
int getdest(int v);
int ISMEM(int v);
int GETREGCHAR(char ch);
int regidx(int idx);
int GETADDRESS(int MB, int LB);
int GETRP(int v);
///8-bit twos complement
int b8twos(int n);
int b4twos(int n);
int popcount(int n);
int GETHEXVAL(char ch);
int hextodecimal(const char s[], int len);
void uppercase(char * s);
#endif

