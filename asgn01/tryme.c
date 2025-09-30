#include<string.h>
#include<stdlib.h>
#include<stdio.h>
int main(int argc, char *argv[]) {
    char *s;
    s = strdup("Tryme"); /* should call malloc() implicitly */
    puts(s);
    free(s);
    return 0;
}