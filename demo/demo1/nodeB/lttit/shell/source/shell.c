#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "vim.h"
#include "fs.h"
#include "comm.h"
#include "schedule.h"
#include "heap.h"

static char linebuf[SHELL_MAX_LINE];
static char path[64];

static char cwd[64] = "";
static char *argv_buf[SHELL_MAX_ARGS];
static char abs[64];

static void normalize_path(char *path)
{
    char *src = path;
    char *dst = path;

    if (*src != '/') {
        return;
    }

    while (*src) {
        if (*src == '/' && *(src + 1) == '/') {
            src++;
            continue;
        }

        if (src[0] == '/' && src[1] == '.' && (src[2] == '/' || src[2] == '\0')) {
            src += 2;
            continue;
        }

        if (src[0] == '/' && src[1] == '.' && src[2] == '.' &&
            (src[3] == '/' || src[3] == '\0')) {

            if (dst != path) {
                dst--;
                while (dst > path && *dst != '/') dst--;
            }
            src += 3;
            continue;
        }

        *dst++ = *src++;
    }

    if (dst > path + 1 && *(dst - 1) == '/')
        dst--;

    *dst = '\0';
}

static void make_abs_path(char *out, const char *path)
{
    if (path[0] == '/') {
        path++;
    }

    if (cwd[0] == '\0') {
        snprintf(out, 128, "%s", path);
    } else {
        snprintf(out, 128, "%s/%s", cwd, path);
    }

    normalize_path(out);
}



void shell_cmd_ls(int argc, char **argv)
{
    memset(path, 0, sizeof(path));
    if (argc == 1) {
        strcpy(path, cwd);
    } else {
        make_abs_path(path, argv[1]);
    }

    struct dirent ents[16];
    int nread = 0;

    if (fs_readdir(path, ents, 16, &nread) != 0) {
        comm_write("ls: cannot open directory\r\n", 27);
        return;
    }

    for (int i = 0; i < nread; i++) {
        comm_write(ents[i].name, strlen(ents[i].name));
        comm_write("\r\n", 2);
    }
}

static int fs_is_dir(const char *path)
{
    struct dirent tmp[1];
    int nread = 0;
    return fs_readdir(path, tmp, 1, &nread) == 0;
}



int shell_readline(char *buf, int max)
{
    int pos = 0;
    for (;;) {
        char c = comm_getc();

        if (c == '\r' || c == '\n') {
            comm_putc('\r');
            comm_putc('\n');
            buf[pos] = 0;
            return pos;
        }

        if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                comm_write("\b \b", 3);
            }
            continue;
        }

        if (pos < max - 1) {
            buf[pos++] = c;
            comm_putc(c);
        }
    }
}

void shell_main(void)
{
    comm_write("> ", 2);

    int len = shell_readline(linebuf, SHELL_MAX_LINE);
    if (len <= 0) return;

    int argc = shell_parse(linebuf, argv_buf, SHELL_MAX_ARGS);
    if (argc == 0) return;

    shell_exec(argc, argv_buf);

}

int shell_parse(char *line, char **argv, int max)
{
    int argc = 0;
    while (*line && argc < max) {
        while (*line == ' ') line++;
        if (!*line) break;

        argv[argc++] = line;

        while (*line && *line != ' ') line++;
        if (*line) *line++ = 0;
    }
    return argc;
}

/*
 * I think, max is 12
 * */
#define MAX_LS_COUNT 12
int cmd_ls(int argc, char **argv)
{
    memset(path, 0, sizeof(path));
    if (argc > 1)
        make_abs_path(path, argv[1]);
    else
        strcpy(path, cwd);

    struct dirent *ents = heap_malloc(sizeof(struct dirent) * MAX_LS_COUNT);
    if (!ents) {
        comm_write("ls: no memory\r\n", 16);
        return -1;
    }

    int n = 0;
    if (fs_readdir(path, ents, MAX_LS_COUNT, &n) < 0) {
        comm_write("ls: cannot open ", 16);
        comm_write(path, strlen(path));
        comm_write("\r\n", 2);

        heap_free(ents);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        comm_write(ents[i].name, strlen(ents[i].name));
        comm_write("  ", 2);
    }

    comm_write("\r\n", 2);

    heap_free(ents);
    return 0;
}


int cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        comm_write("usage: cat FILE\r\n", 18);
        return -1;
    }

    memset(path, 0, sizeof(path));
    make_abs_path(path, argv[1]);

    struct inode *ino;
    if (fs_open(path, 0, &ino) < 0) {
        comm_write("cat: cannot open ", 18);
        comm_write(argv[1], strlen(argv[1]));
        comm_write("\r\n", 2);
        return -1;
    }

    uint32_t off = 0;
    int r;
    char *cat_buf = heap_malloc(FS_BLOCK_SIZE);
    while ((r = fs_read(ino, off, cat_buf, sizeof(cat_buf))) > 0) {
        for (int i = 0; i < r; i++) {
            if (cat_buf[i] == '\n') {
                comm_write("\r\n", 2);
            } else {
                comm_putc(cat_buf[i]);
            }
        }
        off += r;
    }
    heap_free(cat_buf);
    fs_close(ino);
    return 0;
}



int cmd_touch(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: touch FILE\n");
        return -1;
    }

    struct inode *ino;
    if (fs_open(argv[1], O_CREAT, &ino) < 0) {
        printf("touch: cannot create %s\n", argv[1]);
        return -1;
    }
    fs_close(ino);
    return 0;
}

int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: mkdir DIR\n");
        return -1;
    }

    struct inode *ino;
    if (fs_mkdir(argv[1], &ino) < 0) {
        printf("mkdir: cannot create %s\n", argv[1]);
        return -1;
    }
    return 0;
}

int cmd_vim(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: edit FILE\n");
        return -1;
    }
    memset(abs, 0, sizeof(abs));
    make_abs_path(abs, argv[1]);

    vim_main(abs);
    return 0;
}


int cmd_cd(int argc, char **argv)
{
    if (argc < 2) {
        comm_write("usage: cd <dir>\r\n", 17);
        return -1;
    }
    memset(path, 0, sizeof(path));
    make_abs_path(path, argv[1]);

    if (!fs_is_dir(path)) {
        comm_write("cd: no such directory\r\n", 25);
        return -1;
    }

    strcpy(cwd, path);
    return 0;
}

int cmd_sync(int argc, char **argv)
{
    (void)argv;
    if (argc < 2) {
        comm_write("usage: sync \r\n", 17);
        return -1;
    }
    fs_sync();
    return 0;
}

int cmd_mem(int argc, char **argv)
{
    struct heap_stats st = heap_get_stats();

    char buf[128];
    snprintf(buf, sizeof(buf),
             "heap_remain: %u\r\n"
             "heap_free_iter: %u\r\n"
             "heap_max_block: %u\r\n"
             "heap_free_blocks: %u\r\n",
             st.remain_size,
             st.free_size_iter,
             st.max_free_block,
             st.free_blocks);

    comm_write(buf, strlen(buf));
    return 0;
}



int cmd_ps(int argc, char **argv)
{
    char buf[160];
    struct task_info info;

    comm_write("PID   STATE      STACK_USED   PERIOD   DEADLINE\r\n", 52);

    for (uint32_t pid = 1; pid < TASK_COUNT; pid++) {

        if (rtos_get_task_info(pid, &info) != 0)
            continue;

        const char *state_str =
                (info.state == RUNNING)  ? "RUNNING"  :
                (info.state == Ready)    ? "READY"    :
                (info.state == OS_Delay)  ? "DELAYED"  :
                (info.state == Suspend)? "SUSPEND"  :
                (info.state == Dead)  ? "DELETED"  :
                "UNKNOWN";

        snprintf(buf, sizeof(buf),
                 "%-5u %-10s %-12u %-8u %-8u\r\n",
                 info.pid,
                 state_str,
                 info.stack_watermark,
                 info.period,
                 info.deadline);

        comm_write(buf, strlen(buf));
    }

    return 0;
}


struct cmd_entry {
    const char *name;
    int (*func)(int argc, char **argv);
};
static struct cmd_entry cmd_table[] = {
        {"ls",    cmd_ls},
        {"cat",   cmd_cat},
        {"touch", cmd_touch},
        {"mkdir", cmd_mkdir},
        {"vim",  cmd_vim},
        {"cd",    cmd_cd},
        {"sync",    cmd_sync},
        {"mem", cmd_mem},
        {"ps", cmd_ps},
        {NULL, NULL}
};

void shell_exec(int argc, char **argv)
{
    for (int i = 0; cmd_table[i].name; i++) {
        if (strcmp(argv[0], cmd_table[i].name) == 0) {
            cmd_table[i].func(argc, argv);
            return;
        }
    }
    printf("unknown command: %s\n", argv[0]);
}
