/* pty.h - PTY管理 */
#ifndef PTY_H
#define PTY_H

#include <sys/types.h>  /* pid_t */

/*
 * PTY を作成し、/bin/sh を子プロセスとして起動する。
 *   master_fd: マスター側のファイルディスクリプタ（出力）
 *   child_pid: 子プロセスの PID（出力）
 *   cols, rows: ターミナルサイズ
 * 戻り値: 0=成功, -1=失敗
 */
int pty_spawn(int *master_fd, pid_t *child_pid, int cols, int rows);

/*
 * PTY マスターから読み取る（ノンブロッキング）。
 * 戻り値: 読み取ったバイト数, 0=データなし, -1=エラーまたはEOF
 */
int pty_read(int master_fd, char *buf, int bufsize);

/*
 * PTY マスターに書き込む。
 * 戻り値: 書き込んだバイト数, -1=エラー
 */
int pty_write(int master_fd, const char *buf, int len);

#endif /* PTY_H */
