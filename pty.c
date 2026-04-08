/* pty.c - PTY管理 */

#include "pty.h"
#include <util.h>       /* forkpty (macOS) */
#include <unistd.h>     /* read, write, execl, _exit */
#include <fcntl.h>      /* fcntl, O_NONBLOCK */
#include <errno.h>      /* errno, EAGAIN */
#include <stdlib.h>     /* setenv */
#include <termios.h>    /* struct winsize */

int pty_spawn(int *master_fd, pid_t *child_pid, int cols, int rows)
{
    struct winsize ws;
    ws.ws_col = cols;
    ws.ws_row = rows;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    pid_t pid = forkpty(master_fd, NULL, NULL, &ws);
    if (pid < 0) {
        return -1;  /* forkpty 失敗 */
    }

    if (pid == 0) {
        /* --- 子プロセス --- */
        setenv("TERM", "dumb", 1);
        execl("/bin/sh", "sh", NULL);
        _exit(1);  /* execl が失敗した場合のみここに来る */
    }

    /* --- 親プロセス --- */
    /* マスターPTYをノンブロッキングに設定 */
    int flags = fcntl(*master_fd, F_GETFL);
    fcntl(*master_fd, F_SETFL, flags | O_NONBLOCK);

    *child_pid = pid;
    return 0;
}

int pty_read(int master_fd, char *buf, int bufsize)
{
    int n = read(master_fd, buf, bufsize);
    if (n > 0) {
        return n;
    }
    if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0;  /* データなし（正常） */
    }
    return -1;  /* EOF またはエラー */
}

int pty_write(int master_fd, const char *buf, int len)
{
    return write(master_fd, buf, len);
}
