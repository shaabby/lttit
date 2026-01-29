#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#define SHELL_MAX_LINE 128
#define SHELL_MAX_ARGS 8

void shell_init(void);

void shell_on_message(const char *msg, int len);

int  shell_parse(char *line, char **argv, int max);
void shell_exec(int argc, char **argv);

#endif

