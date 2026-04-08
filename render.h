/* render.h - 画面バッファと描画 */
#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>
#include <SDL_ttf.h>

#define COLS 80
#define ROWS 24

/* 画面バッファの初期化 */
void screen_init(void);

/* 1文字を画面バッファに書き込む */
void screen_putchar(char c);

/* 画面バッファを SDL レンダラーに描画する */
void render_screen(SDL_Renderer *renderer, TTF_Font *font,
                   int char_w, int char_h);

/* カーソル位置を取得する */
void screen_get_cursor(int *x, int *y);

#endif /* RENDER_H */
