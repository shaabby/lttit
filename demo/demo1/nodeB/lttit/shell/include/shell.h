#ifndef SHELL_H
#define SHELL_H

#define SHELL_MAX_LINE 32
#define SHELL_MAX_ARGS 8

void shell_main(void);
int shell_readline(char *buf, int max);
int shell_parse(char *line, char **argv, int max);
void shell_exec(int argc, char **argv);

#endif
