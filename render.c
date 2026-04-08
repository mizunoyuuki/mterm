/* render.c - 画面バッファと描画 */

#include "render.h"
#include <string.h>

static char screen[ROWS][COLS + 1];
static int cursor_x = 0;
static int cursor_y = 0;

void screen_init(void)
{
    for (int i = 0; i < ROWS; i++) {
        memset(screen[i], ' ', COLS);
        screen[i][COLS] = '\0';
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void scroll(void)
{
    memmove(screen[0], screen[1], (ROWS - 1) * (COLS + 1));
    memset(screen[ROWS - 1], ' ', COLS);
    screen[ROWS - 1][COLS] = '\0';
    cursor_y = ROWS - 1;
}

void screen_putchar(char c)
{
    if (c == '\n') {
        cursor_y++;
        if (cursor_y >= ROWS) scroll();
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        /* 次の8の倍数まで進む */
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= COLS) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= ROWS) scroll();
        }
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
    } else if (c >= 0x20 && c <= 0x7E) {
        screen[cursor_y][cursor_x] = c;
        cursor_x++;
        if (cursor_x >= COLS) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= ROWS) scroll();
        }
    }
    /* それ以外の制御文字は無視 */
}

void render_screen(SDL_Renderer *renderer, TTF_Font *font,
                   int char_w, int char_h)
{
    SDL_Color green = {0, 255, 0, 255};

    for (int row = 0; row < ROWS; row++) {
        /* 全スペースの行はスキップ */
        int has_content = 0;
        for (int col = 0; col < COLS; col++) {
            if (screen[row][col] != ' ') {
                has_content = 1;
                break;
            }
        }
        if (!has_content) continue;

        SDL_Surface *surface = TTF_RenderText_Blended(font, screen[row], green);
        if (!surface) continue;

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect dst = {0, row * char_h, surface->w, surface->h};
        SDL_FreeSurface(surface);

        if (texture) {
            SDL_RenderCopy(renderer, texture, NULL, &dst);
            SDL_DestroyTexture(texture);
        }
    }

    /* カーソル描画（緑のブロック） */
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect cursor_rect = {
        cursor_x * char_w, cursor_y * char_h,
        char_w, char_h
    };
    SDL_RenderFillRect(renderer, &cursor_rect);
}

void screen_get_cursor(int *x, int *y)
{
    *x = cursor_x;
    *y = cursor_y;
}
