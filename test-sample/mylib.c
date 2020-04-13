#include "mylib.h"
void print_s(char *str) //a0
{
    asm("li a7, 0;"
        "scall");
};

void print_c(char c)
{
    asm("li a7, 1;"
        "scall");
};

void print_d(int d)
{
    asm("li a7, 2;"
        "scall");
};
