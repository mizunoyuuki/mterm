/* myterm - Step 5: 入出力を接続する */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>   /* waitpid */

#include "pty.h"
#include "render.h"

#define FONT_PATH "/System/Library/Fonts/Menlo.ttc"
#define FONT_SIZE 16

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* --- SDL2 初期化 --- */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    TTF_Font *font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (!font) {
        fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    int char_w, char_h;
    TTF_SizeText(font, "M", &char_w, &char_h);

    SDL_Window *window = SDL_CreateWindow(
        "myterm",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        COLS * char_w, ROWS * char_h,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    screen_init();

    /* --- PTY + シェル起動 --- */
    int master_fd;
    pid_t shell_pid;
    if (pty_spawn(&master_fd, &shell_pid, COLS, ROWS) < 0) {
        fprintf(stderr, "pty_spawn failed\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /* --- メインループ --- */
    SDL_StartTextInput();
    int running = 1;
    int shell_alive = 1;
    int dirty = 1;
    SDL_Event event;
    char pty_buf[4096];

    while (running) {
        /* --- イベント処理 --- */
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_TEXTINPUT:
                if (shell_alive) {
                    /* 通常の文字入力 → PTYに送信 */
                    pty_write(master_fd, event.text.text,
                              strlen(event.text.text));
                }
                break;

            case SDL_KEYDOWN:
                if (!shell_alive) {
                    /* シェル終了後は任意のキーでプログラム終了 */
                    running = 0;
                    break;
                }

                if (event.key.keysym.mod & KMOD_CTRL) {
                    /* Ctrl + キー → 制御コードを送信 */
                    SDL_Keycode key = event.key.keysym.sym;
                    if (key >= SDLK_a && key <= SDLK_z) {
                        char ctrl = (char)(key - SDLK_a + 1);
                        pty_write(master_fd, &ctrl, 1);
                    }
                } else {
                    switch (event.key.keysym.sym) {
                    case SDLK_RETURN:
                        pty_write(master_fd, "\r", 1);
                        break;
                    case SDLK_BACKSPACE:
                        pty_write(master_fd, "\x7f", 1);
                        break;
                    case SDLK_TAB:
                        pty_write(master_fd, "\t", 1);
                        break;
                    }
                }
                break;
            }
        }

        /* --- PTY から読み取り --- */
        if (shell_alive) {
            int n = pty_read(master_fd, pty_buf, sizeof(pty_buf));
            if (n > 0) {
                for (int i = 0; i < n; i++) {
                    screen_putchar(pty_buf[i]);
                }
                dirty = 1;
            } else if (n < 0) {
                /* シェルが終了した */
                shell_alive = 0;
                int status;
                waitpid(shell_pid, &status, 0);

                /* メッセージを表示 */
                const char *msg = "\r\n[Process exited. Press any key to close.]";
                for (int i = 0; msg[i]; i++) {
                    screen_putchar(msg[i]);
                }
                dirty = 1;
            }
        }

        /* --- 描画 --- */
        if (dirty) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            render_screen(renderer, font, char_w, char_h);
            SDL_RenderPresent(renderer);
            dirty = 0;
        } else {
            SDL_Delay(16);
        }
    }

    /* --- 終了処理 --- */
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
