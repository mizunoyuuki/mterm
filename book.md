# ゼロから作るターミナルエミュレータ

**〜 700行のCコードで理解する、キーボードからモニターまで 〜**

---

## 目次

- [第0章 この本について](#第0章-この本について)
- [第1章 ターミナルとは何か](#第1章-ターミナルとは何か)
- [第2章 キーボードからモニターまでの13層](#第2章-キーボードからモニターまでの13層)
- [第3章 使用する技術とツール](#第3章-使用する技術とツール)
- [第4章 Step 1 — 黒いウィンドウを出す](#第4章-step-1--黒いウィンドウを出す)
- [第5章 Step 2 — フォントを読み込んで文字を表示する](#第5章-step-2--フォントを読み込んで文字を表示する)
- [第6章 Step 3 — キーボード入力を受け取る](#第6章-step-3--キーボード入力を受け取る)
- [第7章 Step 4 — PTYを開いてシェルを起動する](#第7章-step-4--ptyを開いてシェルを起動する)
- [第8章 Step 5 — 入出力を接続する](#第8章-step-5--入出力を接続する)
- [第9章 Step 6 — カーソルと仕上げ](#第9章-step-6--カーソルと仕上げ)
- [付録A APIリファレンス早見表](#付録a-apiリファレンス早見表)
- [付録B トラブルシューティング](#付録b-トラブルシューティング)
- [付録C さらに先へ](#付録c-さらに先へ)

---

# 第0章 この本について

## 何を作るのか

本書では macOS 上で動くミニマルなGUIターミナルエミュレータ **myterm** を、C言語でゼロから実装する。完成すると以下のことができる:

- 黒い背景に緑の文字が表示されるウィンドウが開く
- キーボードから文字を入力し、`/bin/sh` に渡す
- シェルの出力がウィンドウに描画される
- `ls`、`cat`、`echo` などのコマンドが実行できる

cd などのディレクトリ管理やTAB補完、ANSI色などは実装しない。絶対パスでのコマンド実行に特化した、約700行のミニマル実装だ。

## 前提知識

- C言語の基本文法（ポインタ、配列、構造体、関数）
- ターミナルでの基本操作（`make`、`gcc`）
- macOS を使っていること（Apple Silicon / Intel 不問）

## この本の読み方

本書は**6つのステップ**に分かれており、各ステップを終えるたびにプログラムをコンパイル・実行して動作確認できる。必ず順番に進めること。

各章の構成:
1. **背景知識** — そのステップで使う技術の解説
2. **実装** — 完全なソースコード付き
3. **動作確認** — 確認方法と期待される結果
4. **コラム** — 知っておくと面白い周辺知識

---

# 第1章 ターミナルとは何か

## 1.1 テレタイプの時代

1960〜70年代、コンピュータとのやりとりは**テレタイプ端末 (TTY)** という物理的な機械を通じて行われた。キーボードで入力すると電気信号がコンピュータに送られ、コンピュータからの出力は印字用紙に印刷された。

```
[テレタイプ端末] ←シリアル回線→ [コンピュータ (UNIX)]
  キーボード                       /bin/sh
  プリンター                       ls, cat ...
```

やがてプリンターの代わりにCRTディスプレイが使われるようになり、**ビデオ端末** (VT100など) が登場した。しかし基本的な構造は同じで、シリアル回線を通じてコンピュータと通信していた。

## 1.2 疑似端末 (PTY) の発明

物理端末が消えてソフトウェアだけでコンピュータを操作する時代になると、OSカーネル内に**疑似端末 (Pseudo Terminal, PTY)** が実装された。PTYは物理端末をソフトウェアでエミュレートする仕組みだ。

```
[ターミナルエミュレータ]           [カーネル]              [シェル]
   (myterm)            ←PTYマスター | PTYスレーブ→    (/bin/sh)
   GUI描画                   line discipline            コマンド実行
   キー入力受付                エコー・行編集
```

PTYには**マスター側**と**スレーブ側**があり、間に**line discipline（回線規律）**というカーネル内の処理層が挟まっている:

- **マスター側**: ターミナルエミュレータ（今回作る myterm）が操作する
- **スレーブ側**: シェル（/bin/sh）の stdin/stdout/stderr に接続される
- **line discipline**: エコー、行編集、シグナル送信（Ctrl-C → SIGINT）を処理する

つまり私たちが作る myterm は、かつての VT100 端末の役割をソフトウェアで担うことになる。

## 1.3 現代のターミナルエミュレータ

macOS の Terminal.app、iTerm2、Linux の GNOME Terminal、Windows Terminal — これらはすべて同じ原理で動いている。違いは主にGUI部分（フォントレンダリング、色のサポート、タブ機能など）にある。

本書で作る myterm は、これらのターミナルエミュレータの最小構成版だ。

### コラム: 「ターミナル」「コンソール」「シェル」の違い

| 用語 | 意味 |
|------|------|
| **ターミナル (端末)** | ユーザーとコンピュータの間のインターフェース装置。物理的にはVT100、ソフトウェアではターミナルエミュレータ |
| **コンソール** | そのコンピュータに直接接続された端末。歴史的にはメインフレームの操作卓 |
| **シェル** | コマンドを解釈して実行するプログラム（sh, bash, zsh）。ターミナルの向こう側で動く |
| **TTY** | テレタイプの略。UNIX系OSでは端末デバイスを指す（`/dev/tty`） |
| **PTY** | 疑似端末。ソフトウェアで TTY をエミュレートする仕組み |

---

# 第2章 キーボードからモニターまでの13層

myterm でキーを押してから画面に文字が表示されるまでに、データは13の層を通過する。この章では全体像を把握しよう。

## 2.1 全体図

```
[キーボード]
    │  (1) HIDドライバ: USB/Bluetooth → キーコード (scancode)
    ▼
[OS (macOS)]
    │  (2) キーボードレイアウト: scancode → 文字コード (Unicode)
    │      修飾キー (Shift/Ctrl/Cmd) の処理もここ
    ▼
[SDL2 イベントシステム]
    │  (3) SDL_KEYDOWN: 生キーコード (制御キー用)
    │      SDL_TEXTINPUT: 変換済み文字 (文字入力用)
    ▼
[myterm のイベントハンドラ]
    │  (4) 文字 → PTYにwrite()
    │      制御キー → 制御コードに変換してPTYにwrite()
    │      例: Ctrl-C → 0x03, Enter → '\r', Backspace → 0x7f
    ▼
[PTY マスター側 (master_fd)]
    │  (5) write()でデータを送信
    │      カーネル内のline discipline(回線規律)が処理:
    │      - エコー (ECHO): 入力文字を出力側にコピー
    │      - 行編集 (ICANON): Backspaceで1文字削除
    │      - シグナル (ISIG): Ctrl-C → SIGINT, Ctrl-Z → SIGTSTP
    ▼
[PTY スレーブ側 → /bin/sh の stdin]
    │  (6) シェルがread()で入力を受け取る
    │      コマンドを解析・実行
    ▼
[コマンド実行 (fork/exec)]
    │  (7) 例: ls → ファイル一覧を stdout に出力
    ▼
[/bin/sh の stdout/stderr → PTY スレーブ側]
    │  (8) プログラムの出力がPTYスレーブにwrite()される
    │      line disciplineが出力加工:
    │      - '\n' → '\r\n' 変換 (ONLCR)
    ▼
[PTY マスター側 (master_fd)]
    │  (9) 親プロセス(myterm)がread()で出力を受け取る
    │      非ブロッキングI/O (O_NONBLOCK)
    ▼
[myterm の画面バッファ処理]
    │  (10) バイトストリームを解釈:
    │       - 印字可能文字 → screen[y][x]に格納
    │       - '\n' → カーソルを次の行へ
    │       - '\r' → カーソルを行頭へ
    │       - '\t' → 次のタブストップへ
    │       - ESCシーケンス → カーソル移動/画面クリア等
    ▼
[SDL2_ttf フォントレンダリング]
    │  (11) screen[][]の各行を文字列として
    │       TTF_RenderText_Blended() → SDL_Surface → SDL_Texture
    ▼
[SDL2 レンダラー]
    │  (12) SDL_RenderCopy()でテクスチャを配置
    │       SDL_RenderPresent()でバックバッファをフリップ
    ▼
[GPU → モニター]
       (13) フレームバッファの内容がディスプレイに表示される
```

## 2.2 各層の詳細

### 層 (1)〜(2): ハードウェアからOSへ

キーボードのキーを押すと、キーボードのファームウェアが **scancode** という番号を生成する。例えば「A」キーの scancode は 0x04（USB HID規格）。

OS のキーボードドライバがこの scancode を受け取り、キーボードレイアウト（US配列、JIS配列など）に基づいて文字コードに変換する。Shift が押されていれば `a` → `A`、Ctrl が押されていれば制御文字になる。

### 層 (3): SDL2のイベントシステム

SDL2 は OS からのキー情報を **2種類のイベント** に分けて提供する:

| イベント | 内容 | 用途 |
|---------|------|------|
| `SDL_KEYDOWN` | 生のキーコード + 修飾キー情報 | 制御キー（Enter, Backspace, Ctrl-C, 矢印キー） |
| `SDL_TEXTINPUT` | OS が変換した文字（UTF-8文字列） | 通常の文字入力 |

なぜ2種類あるのか？ 例えば日本語入力やデッドキー（アクセント付き文字の入力）では、複数のキー操作が1つの文字になる。`SDL_TEXTINPUT` はそういった変換済みの結果を提供する。`SDL_KEYDOWN` は raw な物理キー情報だ。

### 層 (4): myterm のイベントハンドラ

myterm は SDL2 のイベントを受け取り、PTY に送るバイト列に変換する:

| 入力 | PTYに送るバイト | 理由 |
|------|-----------------|------|
| 通常の文字 'a' | `'a'` (0x61) | そのまま |
| Enter | `'\r'` (0x0D) | ターミナルの慣例。PTYのline disciplineが `\r` → `\r\n` に変換 |
| Backspace | `0x7F` (DEL) | macOS のデフォルト VERASE 文字 |
| Ctrl-C | `0x03` (ETX) | ASCII制御文字。Ctrl + 文字 = 文字コード - 0x60 |
| Ctrl-D | `0x04` (EOT) | EOF を示す |

### 層 (5): line discipline（回線規律）

カーネル内の最も重要な層。以下の処理を自動的に行う:

**入力側の処理 (termios の `c_iflag`, `c_lflag`):**
- **ECHO**: 入力された文字を出力側にコピー（画面にエコー）
- **ICANON**: 正規モード。行単位で編集できる（Backspaceで削除、Enterで確定）
- **ISIG**: Ctrl-C で SIGINT、Ctrl-Z で SIGTSTP を送信

**出力側の処理 (termios の `c_oflag`):**
- **ONLCR**: `\n` を `\r\n` に変換（出力時）
- **OPOST**: 出力処理の有効/無効

この層があるおかげで、myterm は自前でエコーや行編集を実装しなくてよい。

### 層 (6)〜(8): シェルとコマンド実行

シェル（/bin/sh）は PTY スレーブ側の stdin から入力を読み、コマンドを実行し、結果を stdout/stderr に書く。stdout/stderr は PTY スレーブに接続されているので、出力は再び line discipline を通ってマスター側に届く。

### 層 (9): ノンブロッキング読み取り

myterm は SDL2 のメインループ（60fps）で毎フレーム PTY マスターから `read()` する。データがなければ即座に戻る（`EAGAIN`）。これにより GUI の応答性を損なわずに PTY と通信できる。

### 層 (10): バイトストリームの解釈

PTY から読み取ったデータはバイトストリームだ。印字可能文字はそのまま画面バッファに格納し、制御文字（`\n`, `\r`, `\t` など）はカーソル移動に変換する。

### 層 (11)〜(13): レンダリングパイプライン

```
screen[ROWS][COLS]  →  TTF_RenderText_Blended()  →  SDL_Texture  →  GPU  →  モニター
  (文字データ)           (ラスタライズ)            (テクスチャ)    (描画)   (表示)
```

SDL2_ttf がフォントのグリフ（字形）をピクセルに変換し、SDL2 のレンダラーが GPU を使って画面に描画する。

### コラム: VT100 とエスケープシーケンス

1978年にDEC社が発売したVT100端末は、**ANSIエスケープシーケンス**を初めてサポートした端末だ。ESC（0x1B）に続く文字列でカーソル移動、画面クリア、文字色の変更などを指示する。

```
\x1b[2J     ← 画面全体をクリア
\x1b[10;5H  ← カーソルを10行5列に移動
\x1b[31m    ← 文字色を赤に変更
```

現在でもほぼすべてのターミナルエミュレータがVT100互換のエスケープシーケンスをサポートしている。本書の myterm は最小実装のためこれらを扱わないが、Step 6 のオプションで基本的なものに触れる。

---

# 第3章 使用する技術とツール

## 3.1 技術スタック

| 技術 | 役割 | バージョン |
|------|------|-----------|
| **C言語** | 実装言語 | C11 |
| **SDL2** | ウィンドウ作成・キー入力・描画 | 2.x |
| **SDL2_ttf** | TrueTypeフォントレンダリング | 2.x |
| **POSIX API** | PTY・プロセス管理 | macOS標準 |
| **clang** | コンパイラ | Xcode付属 |
| **Make** | ビルドツール | macOS標準 |

## 3.2 環境構築

### Xcode Command Line Tools

```bash
xcode-select --install
```

これで `clang`、`make`、POSIX ヘッダが使えるようになる。

### Homebrew でライブラリをインストール

```bash
brew install sdl2 sdl2_ttf
```

### インストール確認

```bash
# SDL2 のコンパイルフラグ確認
sdl2-config --cflags --libs
# 出力例: -I/opt/homebrew/include/SDL2 -D_THREAD_SAFE
#         -L/opt/homebrew/lib -lSDL2

# SDL2_ttf の確認
pkg-config --cflags --libs SDL2_ttf
# 出力例: -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2_ttf

# フォントの確認
ls /System/Library/Fonts/Menlo.ttc
```

## 3.3 必要なC言語の知識

### ファイルディスクリプタ

UNIX系OSでは、ファイル・ソケット・パイプ・PTYなどすべてのI/Oを**ファイルディスクリプタ (fd)** という整数で扱う:

```c
int fd = open("file.txt", O_RDONLY);  // fd = 3 など
char buf[256];
int n = read(fd, buf, sizeof(buf));   // fd からnバイト読む
write(fd, "hello", 5);                // fd に5バイト書く
close(fd);                            // fd を閉じる
```

標準のファイルディスクリプタ:
- `0` = stdin（標準入力）
- `1` = stdout（標準出力）
- `2` = stderr（標準エラー出力）

### fork() と exec()

```c
pid_t pid = fork();
// fork() はプロセスを複製する。ここから先は2つのプロセスが同時に実行される。

if (pid == 0) {
    // 子プロセス
    execl("/bin/ls", "ls", "-l", NULL);
    // execl() は現在のプロセスを別のプログラムに置き換える。
    // 成功すると戻ってこない。
    _exit(1);  // execl失敗時のみここに来る
} else {
    // 親プロセス（pid = 子プロセスのPID）
    int status;
    waitpid(pid, &status, 0);  // 子の終了を待つ
}
```

**重要**: exec 失敗時は `_exit()` を使う。`exit()` は atexit ハンドラを実行してしまい、親プロセスのリソース（ファイルバッファなど）を壊す可能性がある。

### ノンブロッキングI/O

通常の `read()` はデータが来るまでブロック（待機）する。GUIアプリケーションでは、メインループが止まると画面が固まるので、ブロッキングは許されない。

```c
#include <fcntl.h>
#include <errno.h>

// fd をノンブロッキングに設定
int flags = fcntl(fd, F_GETFL);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);

// 読み取り
char buf[4096];
int n = read(fd, buf, sizeof(buf));
if (n > 0) {
    // n バイト読めた
} else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    // データがまだない（正常。次のフレームで再試行）
} else if (n == 0) {
    // EOF（相手が接続を閉じた）
} else {
    // エラー
}
```

## 3.4 SDL2 の基本概念

### 初期化からウィンドウ表示まで

```
SDL_Init()  →  SDL_CreateWindow()  →  SDL_CreateRenderer()
                                            │
                ┌───────────────────────────┘
                ▼
          メインループ:
            SDL_PollEvent()    ← イベント処理
            SDL_RenderClear()  ← 画面クリア
            (描画処理)
            SDL_RenderPresent() ← 画面更新
            SDL_Delay(16)      ← ~60fps制御
```

### ダブルバッファリング

SDL2 のレンダラーは**ダブルバッファリング**を行う:
1. **バックバッファ**に描画する（ユーザーには見えない）
2. `SDL_RenderPresent()` でバックバッファとフロントバッファを入れ替える
3. フロントバッファの内容がモニターに表示される

これにより、描画途中の画面がちらつかない。

### SDL2 のイベントモデル

```c
SDL_Event event;
while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
        // ウィンドウの×ボタンが押された
        break;
    case SDL_KEYDOWN:
        // キーが押された（物理キー）
        // event.key.keysym.sym  → キーコード (SDLK_RETURN など)
        // event.key.keysym.mod  → 修飾キー (KMOD_CTRL など)
        break;
    case SDL_TEXTINPUT:
        // テキスト入力（変換済み文字）
        // event.text.text → UTF-8 文字列
        break;
    }
}
```

## 3.5 SDL2_ttf の基本概念

SDL2_ttf は FreeType ライブラリのラッパーで、TrueType/OpenType フォントをレンダリングする。

```
TTF_OpenFont()
    │
    ▼  フォントオブジェクト (TTF_Font*)
    │
TTF_RenderText_Blended()
    │
    ▼  SDL_Surface* (CPUメモリ上のピクセルデータ)
    │
SDL_CreateTextureFromSurface()
    │
    ▼  SDL_Texture* (GPUメモリ上のテクスチャ)
    │
SDL_RenderCopy()  →  画面に描画
```

**Surface** はCPUメモリ上のピクセルデータ、**Texture** はGPUメモリ上のテクスチャ。Surface は作ったらすぐ Texture に変換して解放するのが定石。

### コラム: .ttc (TrueType Collection) ファイル

macOS の Menlo.ttc は1つのファイルに4つのフォントを格納している:

| インデックス | フォント |
|-------------|---------|
| 0 | Menlo Regular |
| 1 | Menlo Bold |
| 2 | Menlo Italic |
| 3 | Menlo Bold Italic |

`TTF_OpenFont()` はデフォルトでインデックス 0（Regular）を開く。別のフォントが欲しい場合は `TTF_OpenFontIndex(path, size, index)` を使う。

## 3.6 ファイル構成の計画

開発は段階的に進める:

**Step 1〜3（main.c 1ファイル）:**
```
myterm/
├── main.c       ← すべてのコードをここに
├── Makefile
└── plan.md
```

**Step 4以降（ファイル分割）:**
```
myterm/
├── main.c       イベントループ、SDL初期化
├── pty.c        PTY・fork/exec
├── pty.h        PTY関数宣言
├── render.c     画面バッファ・テキスト描画
├── render.h     描画関数宣言
├── Makefile
└── plan.md
```

なぜ最初から分割しないのか？ Step 1〜3 は SDL2 のみでPTYを使わない。インターフェースが固まっていない段階でヘッダファイルを管理するのは無駄な摩擦になる。

---

# 第4章 Step 1 — 黒いウィンドウを出す

## 目標

SDL2 を使って黒い 640x480 のウィンドウを表示し、×ボタンで閉じられるようにする。

## 4.1 背景知識: ゲームループ

SDL2 はもともとゲーム開発用ライブラリだ。ターミナルエミュレータにも同じ**ゲームループ**パターンを使う:

```
while (実行中) {
    1. イベント処理（キー入力、ウィンドウ操作）
    2. 状態更新（画面バッファの変更）
    3. 描画（画面のレンダリング）
    4. フレーム待機（16ms ≒ 60fps）
}
```

このループは毎秒約60回まわる。各反復を「フレーム」と呼ぶ。

## 4.2 SDL2 の初期化手順

SDL2 の初期化は以下の順序で行う:

1. `SDL_Init(SDL_INIT_VIDEO)` — SDL2 サブシステムの初期化
2. `SDL_CreateWindow(...)` — ウィンドウの作成
3. `SDL_CreateRenderer(...)` — レンダラー（描画エンジン）の作成

レンダラーのフラグ:
- `SDL_RENDERER_ACCELERATED` — GPU アクセラレーションを使用
- `SDL_RENDERER_PRESENTVSYNC` — モニターの垂直同期に合わせて描画（ティアリング防止）

## 4.3 実装

### Makefile

```makefile
CC = clang
CFLAGS = $(shell sdl2-config --cflags) -Wall -Wextra
LDFLAGS = $(shell sdl2-config --libs)
TARGET = myterm

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)
```

**解説:**
- `sdl2-config --cflags` は `-I/opt/homebrew/include/SDL2 -D_THREAD_SAFE` のようなインクルードパスを出力する
- `sdl2-config --libs` は `-L/opt/homebrew/lib -lSDL2` のようなリンクフラグを出力する
- `-Wall -Wextra` は警告を多めに出す設定。バグの早期発見に有効

### main.c （Step 1 完全版）

```c
/* myterm - Step 1: 黒いウィンドウを出す */

#include <SDL.h>
#include <stdio.h>

#define WINDOW_W 640
#define WINDOW_H 480

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* --- SDL2 初期化 --- */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "myterm",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* --- メインループ --- */
    int running = 1;
    SDL_Event event;

    while (running) {
        /* イベント処理 */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        /* 描画 */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);  /* 黒 */
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        /* フレームレート制御 (VSYNCが効かない場合の安全策) */
        SDL_Delay(16);
    }

    /* --- 終了処理 --- */
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

**コード解説:**

- `(void)argc; (void)argv;` — 未使用引数の警告を抑制。SDL2 は `SDL.h` 内で `main` を `SDL_main` にマクロ再定義しており、`argc`/`argv` の引数シグネチャが必須
- `SDL_CreateRenderer(window, -1, ...)` — 第2引数の `-1` は「最適なドライバを自動選択」の意味
- `SDL_PollEvent()` — イベントキューからイベントを1つ取り出す。キューが空なら即座に 0 を返す（ブロックしない）
- `SDL_Delay(16)` — 16ミリ秒スリープ。1000ms / 60fps ≒ 16ms。VSYNC が効いている場合は冗長だが、効かない環境での CPU 100% を防ぐ

## 4.4 ビルドと動作確認

```bash
make
./myterm
```

**期待される結果:**
- 黒い 640x480 のウィンドウが画面中央に表示される
- ウィンドウの×ボタン (赤丸) をクリックすると終了する
- Cmd-Q でも終了する（SDL2 が macOS のメニューバーを自動設定するため）

**確認すること:**
- Activity Monitor で CPU 使用率が数%以下であること
- コンソールにエラーメッセージが出ていないこと

### コラム: macOS でのフォーカス問題

ターミナルから `./myterm` を起動した場合、myterm のウィンドウにキーボードフォーカスが当たらないことがある。これは macOS の仕様で、.app バンドルではないプログラムは「バックグラウンドアプリ」扱いになるため。ウィンドウをクリックするか、Cmd-Tab で切り替えれば解決する。

---

# 第5章 Step 2 — フォントを読み込んで文字を表示する

## 目標

SDL2_ttf を使って Menlo フォントを読み込み、"Hello, terminal!" という文字列を緑色で画面に表示する。さらに、ウィンドウサイズを 80 文字 x 24 行のグリッドに合わせる。

## 5.1 背景知識: フォントレンダリング

コンピュータ上で文字を表示するには、以下のプロセスが必要:

1. **フォントファイル読み込み** — .ttf/.ttc ファイルには各文字の形（グリフ）がベジェ曲線で記述されている
2. **ラスタライズ** — ベジェ曲線をピクセルに変換（指定されたサイズで）
3. **テクスチャ化** — ピクセルデータを GPU に転送
4. **描画** — テクスチャを画面の指定位置に配置

SDL2_ttf は 1〜2 を `TTF_RenderText_Blended()` で、3 を `SDL_CreateTextureFromSurface()` で行う。

### レンダリングモード

SDL2_ttf には3つのレンダリングモードがある:

| 関数 | 品質 | 速度 | 背景 |
|------|------|------|------|
| `TTF_RenderText_Solid` | 低 | 速い | 透明 |
| `TTF_RenderText_Shaded` | 中 | 中 | 指定色で塗りつぶし |
| `TTF_RenderText_Blended` | 高 | 遅い | 透明（アンチエイリアス付き） |

本書では **Blended** を使う。アンチエイリアスのおかげで文字が滑らかに表示される。毎フレーム呼ぶと遅いが、変更時のみ呼ぶ（dirty フラグパターン）ので問題ない。

## 5.2 実装

### Makefile の更新

```makefile
CC = clang
CFLAGS = $(shell sdl2-config --cflags) $(shell pkg-config --cflags SDL2_ttf) -Wall -Wextra
LDFLAGS = $(shell sdl2-config --libs) $(shell pkg-config --libs SDL2_ttf)
TARGET = myterm

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)
```

### main.c （Step 2 完全版）

```c
/* myterm - Step 2: フォントを読み込んで文字を表示する */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>

#define COLS 80
#define ROWS 24
#define FONT_PATH "/System/Library/Fonts/Menlo.ttc"
#define FONT_SIZE 16

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* --- SDL2 初期化 --- */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    /* --- フォント読み込み --- */
    TTF_Font *font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (!font) {
        fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /* --- セルサイズの計測 --- */
    int char_w, char_h;
    TTF_SizeText(font, "M", &char_w, &char_h);
    fprintf(stderr, "Cell size: %d x %d\n", char_w, char_h);

    int win_w = COLS * char_w;
    int win_h = ROWS * char_h;

    /* --- ウィンドウ作成 (80x24 グリッドサイズ) --- */
    SDL_Window *window = SDL_CreateWindow(
        "myterm",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    /* --- "Hello, terminal!" テクスチャを生成 --- */
    SDL_Color green = {0, 255, 0, 255};
    SDL_Surface *surface = TTF_RenderText_Blended(font, "Hello, terminal!", green);
    if (!surface) {
        fprintf(stderr, "TTF_RenderText_Blended failed: %s\n", TTF_GetError());
        /* テクスチャ生成に失敗しても続行（黒い画面を表示） */
    }

    SDL_Texture *texture = NULL;
    SDL_Rect dst_rect = {0, 0, 0, 0};
    if (surface) {
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        dst_rect.w = surface->w;
        dst_rect.h = surface->h;
        SDL_FreeSurface(surface);  /* Texture にコピー済みなので Surface は解放 */
    }

    /* --- メインループ --- */
    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        /* テキストを描画 */
        if (texture) {
            SDL_RenderCopy(renderer, texture, NULL, &dst_rect);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    /* --- 終了処理 --- */
    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
```

**コード解説:**

- `TTF_SizeText(font, "M", &char_w, &char_h)` — 文字 "M" のピクセルサイズを取得。等幅フォントでは全文字が同じ幅なので、これで1セルのサイズがわかる。Menlo 16pt では約 10x19 ピクセル
- `SDL_FreeSurface(surface)` — Surface は Texture に変換したら不要。メモリリークを防ぐために必ず解放する
- テクスチャの生成はループの外で1回だけ行う。毎フレーム生成すると非常に遅い

## 5.3 ビルドと動作確認

```bash
brew install sdl2_ttf  # まだの場合
make clean && make
./myterm
```

**期待される結果:**
- ウィンドウサイズが約 800x456 になる（80 * 10 x 24 * 19）
- 左上に "Hello, terminal!" が緑色で表示される
- 背景は黒

### コラム: 等幅フォントとプロポーショナルフォント

ターミナルエミュレータでは**等幅 (monospace) フォント**が必須だ。等幅フォントでは 'i' も 'W' も同じ幅なので、画面バッファを2Dの文字配列として扱える。

プロポーショナルフォントでは文字ごとに幅が異なるため、1セル = 1文字の前提が崩れる。

macOS の代表的な等幅フォント:
- **Menlo** — macOS のデフォルト等幅フォント（本書で使用）
- **SF Mono** — Apple の新しい等幅フォント
- **Monaco** — 古い macOS のデフォルト
- **Courier New** — クラシックなタイプライター風フォント

---

# 第6章 Step 3 — キーボード入力を受け取る

## 目標

キーボードから文字を入力し、画面バッファに書き込み、複数行で表示する。Enter で改行、Backspace で1文字削除、スクロールにも対応する。

## 6.1 背景知識: SDL2 のテキスト入力

SDL2 でキーボード入力を扱う場合、2つのイベントを使い分ける必要がある:

### SDL_KEYDOWN — 物理キーイベント

```c
case SDL_KEYDOWN:
    event.key.keysym.sym;  // キーコード (SDLK_RETURN, SDLK_BACKSPACE, ...)
    event.key.keysym.mod;  // 修飾キー (KMOD_CTRL, KMOD_SHIFT, ...)
```

**用途**: Enter, Backspace, 矢印キー, Ctrl+キー など、文字を生成しない操作

### SDL_TEXTINPUT — テキスト入力イベント

```c
case SDL_TEXTINPUT:
    event.text.text;  // UTF-8 文字列 (例: "a", "A", "あ")
```

**用途**: 印字可能な文字の入力。OS のキーボードレイアウトや入力メソッドが適用された後の文字が届く。

### なぜ両方必要なのか

| キー操作 | KEYDOWN | TEXTINPUT |
|---------|---------|-----------|
| 'a' を押す | `SDLK_a` が来る | `"a"` が来る |
| Shift+'a' | `SDLK_a` + `KMOD_SHIFT` | `"A"` が来る |
| Enter | `SDLK_RETURN` が来る | 来ない |
| Backspace | `SDLK_BACKSPACE` が来る | 来ない |
| Ctrl+C | `SDLK_c` + `KMOD_CTRL` | 来ない |

通常の文字入力には `SDL_TEXTINPUT` を使い、制御キーには `SDL_KEYDOWN` を使うのが正しい使い方だ。

## 6.2 背景知識: 画面バッファ

ターミナルの画面は **ROWS 行 x COLS 列** の2D文字配列で表現する:

```c
#define COLS 80
#define ROWS 24
char screen[ROWS][COLS + 1];  // +1 は null 終端用
int cursor_x = 0;  // 列 (0〜COLS-1)
int cursor_y = 0;  // 行 (0〜ROWS-1)
```

文字を書き込む操作:
```
screen[cursor_y][cursor_x] = 'A';
cursor_x++;
```

スクロールの操作:
```
memmove(screen[0], screen[1], (ROWS - 1) * (COLS + 1));  // 全行を1つ上にずらす
memset(screen[ROWS - 1], ' ', COLS);                      // 最終行をクリア
screen[ROWS - 1][COLS] = '\0';
cursor_y = ROWS - 1;
```

`memmove` は `memcpy` と違い、コピー元とコピー先が重なっていても安全に動作する。スクロールでは隣接する行をコピーするので `memmove` が必須だ。

## 6.3 実装

### main.c （Step 3 完全版）

```c
/* myterm - Step 3: キーボード入力を受け取る */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#define COLS 80
#define ROWS 24
#define FONT_PATH "/System/Library/Fonts/Menlo.ttc"
#define FONT_SIZE 16

/* --- グローバル状態 --- */
static char screen[ROWS][COLS + 1];  /* 画面バッファ (+1 for null) */
static int cursor_x = 0;
static int cursor_y = 0;

/* --- 画面バッファ操作 --- */

static void screen_init(void)
{
    for (int i = 0; i < ROWS; i++) {
        memset(screen[i], ' ', COLS);
        screen[i][COLS] = '\0';
    }
}

static void screen_scroll(void)
{
    memmove(screen[0], screen[1], (ROWS - 1) * (COLS + 1));
    memset(screen[ROWS - 1], ' ', COLS);
    screen[ROWS - 1][COLS] = '\0';
    cursor_y = ROWS - 1;
}

static void screen_putchar(char c)
{
    if (c == '\n') {
        cursor_y++;
        if (cursor_y >= ROWS) screen_scroll();
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c >= 0x20 && c <= 0x7E) {
        screen[cursor_y][cursor_x] = c;
        cursor_x++;
        if (cursor_x >= COLS) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= ROWS) screen_scroll();
        }
    }
}

/* --- 描画 --- */

static void render_screen(SDL_Renderer *renderer, TTF_Font *font,
                          int char_w, int char_h)
{
    SDL_Color green = {0, 255, 0, 255};

    for (int row = 0; row < ROWS; row++) {
        /* 全スペースの行はスキップ (TTF_RenderText_Blended が NULL を返すため) */
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
}

/* --- メイン --- */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

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
    SDL_StartTextInput();

    int running = 1;
    int dirty = 1;  /* 初回描画のため */
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_TEXTINPUT:
                /* 通常の文字入力 */
                for (int i = 0; event.text.text[i]; i++) {
                    screen_putchar(event.text.text[i]);
                }
                dirty = 1;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_RETURN) {
                    /* Enter: 改行 */
                    screen_putchar('\r');
                    screen_putchar('\n');
                    dirty = 1;
                } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
                    /* Backspace: 1文字削除 */
                    if (cursor_x > 0) {
                        cursor_x--;
                        screen[cursor_y][cursor_x] = ' ';
                        dirty = 1;
                    }
                }
                break;
            }
        }

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

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
```

**コード解説:**

- **`dirty` フラグ** — 画面に変更があった時だけ再描画する。変更がなければ `SDL_Delay(16)` でCPUを休ませる。毎フレーム24行のテクスチャを生成するのは重いので、必要な時だけ行う
- **`screen_putchar()`** — 1文字を画面バッファに書き込む汎用関数。後のステップで PTY からの出力にも使う
- **`render_screen()`** — 全スペースの行をスキップすることで `TTF_RenderText_Blended` が NULL を返す問題を回避
- **`SDL_StartTextInput()`** — デスクトップでは暗黙的に有効だが、明示的に呼ぶことで意図を明確にする

## 6.4 ビルドと動作確認

```bash
make clean && make
./myterm
```

**確認項目:**
1. 文字を打つ → 画面左上から文字が表示される
2. Enter を押す → カーソルが次の行に移動する
3. Backspace を押す → 1文字消える
4. 24行以上入力する → 画面がスクロールする
5. ウィンドウを閉じる → 正常に終了する

### コラム: dirty フラグパターン

「変更があった時だけ再描画する」パターンは GUI プログラミングの基本だ。

```
入力あり → dirty = 1
メインループ:
  if (dirty) {
      再描画();
      dirty = 0;
  } else {
      スリープ();  // CPUを休ませる
  }
```

ゲームでは毎フレーム画面が変わるので常に再描画するが、ターミナルでは入力がなければ画面は変わらない。dirty フラグで不要な再描画をスキップすることで、CPU使用率を大幅に下げられる。

---

# 第7章 Step 4 — PTYを開いてシェルを起動する

## 目標

疑似端末 (PTY) を作成し、`/bin/sh` を子プロセスとして起動する。PTY からの出力を stderr にデバッグ表示する。このステップでファイル分割も行う。

## 7.1 背景知識: PTY の仕組み

### PTY とは何か

PTY (Pseudo Terminal) は、物理端末をソフトウェアでエミュレートするカーネルの機能だ。2つのファイルディスクリプタのペアで構成される:

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────┐
│  myterm          │     │  カーネル          │     │  /bin/sh     │
│  (親プロセス)     │     │                   │     │  (子プロセス) │
│                  │     │  ┌──────────────┐ │     │              │
│  write(master)───┼────→│  │line discipline│─┼────→│  read(stdin)  │
│                  │     │  │              │ │     │              │
│  read(master) ←──┼─────│  │  エコー       │←┼─────│  write(stdout)│
│                  │     │  │  行編集       │ │     │              │
└─────────────────┘     │  │  シグナル     │ │     └─────────────┘
                        │  └──────────────┘ │
                        │  master    slave   │
                        └──────────────────┘
```

### line discipline が行うこと

line discipline はマスターとスレーブの間に自動的に入る処理層:

**入力方向 (マスター → スレーブ):**

| 機能 | termios フラグ | 動作 |
|------|---------------|------|
| エコー | `ECHO` | 入力文字を出力側にコピー（画面に表示） |
| 正規モード | `ICANON` | Enter まで行をバッファリング。Backspace で1文字削除 |
| シグナル | `ISIG` | Ctrl-C → SIGINT、Ctrl-Z → SIGTSTP、Ctrl-\\ → SIGQUIT |

**出力方向 (スレーブ → マスター):**

| 機能 | termios フラグ | 動作 |
|------|---------------|------|
| NL→CRNL変換 | `ONLCR` | `\n` を `\r\n` に変換 |
| 出力処理 | `OPOST` | 出力後処理の有効/無効 |

**重要**: エコーは line discipline が行うので、myterm が自前で入力文字を画面に表示する必要はない。入力 → PTY に write → line discipline がエコー → myterm が read で受け取って表示、という流れになる。

### forkpty()

`forkpty()` は PTY の作成と子プロセスの fork を一括で行う便利関数:

```c
#include <util.h>  // macOS
// Linux では #include <pty.h>

int master_fd;
pid_t pid = forkpty(&master_fd, NULL, NULL, &ws);

// これは以下と同等:
//   1. openpty(&master, &slave, ...) で PTYペアを作成
//   2. fork() でプロセスを複製
//   3. 子プロセスで:
//      a. setsid() で新しいセッションリーダーになる
//      b. ioctl(slave, TIOCSCTTY, 0) で制御端末を設定
//      c. dup2(slave, 0/1/2) で stdin/stdout/stderr をスレーブに接続
//      d. close(master) でマスターを閉じる（子では不要）
//   4. 親プロセスで:
//      a. close(slave) でスレーブを閉じる（親では不要）
```

手動で行うと10行以上になり、setsid や TIOCSCTTY の順序を間違えやすい。`forkpty()` ならこれが1行で済む。

### TERM 環境変数

シェルや各種コマンドは `TERM` 環境変数を見て、どのようなエスケープシーケンスを出力するかを決める:

| TERM | 意味 |
|------|------|
| `dumb` | エスケープシーケンスを出力しない。最も単純 |
| `xterm` | 基本的なANSIエスケープシーケンスをサポート |
| `xterm-256color` | 256色をサポート |

本ステップでは `TERM=dumb` に設定する。これにより、シェルプロンプトやコマンドの出力がプレーンテキストになり、エスケープシーケンスのパーサーなしで表示できる。

## 7.2 ファイル分割

ここで `main.c` 1ファイルから、複数ファイル構成にリファクタリングする。

### pty.h

```c
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
```

### pty.c

```c
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
```

### render.h

```c
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
```

### render.c

```c
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
```

### main.c （Step 4 完全版）

```c
/* myterm - Step 4: PTYを開いてシェルを起動する */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>

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

    /* --- 画面バッファ初期化 --- */
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
    fprintf(stderr, "Shell started (pid=%d)\n", shell_pid);

    /* --- メインループ --- */
    SDL_StartTextInput();
    int running = 1;
    int dirty = 1;
    SDL_Event event;
    char pty_buf[4096];

    while (running) {
        /* イベント処理 */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            /* キー入力は Step 5 で PTY に接続する。
               ここではまだ何もしない。 */
        }

        /* PTY から読み取り（デバッグ出力） */
        int n = pty_read(master_fd, pty_buf, sizeof(pty_buf) - 1);
        if (n > 0) {
            pty_buf[n] = '\0';
            fprintf(stderr, "PTY output (%d bytes): [%s]\n", n, pty_buf);
            /* 画面バッファにも書き込む */
            for (int i = 0; i < n; i++) {
                screen_putchar(pty_buf[i]);
            }
            dirty = 1;
        } else if (n < 0) {
            /* EOF: シェルが終了した */
            fprintf(stderr, "Shell exited\n");
            running = 0;
        }

        /* 描画 */
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
```

### Makefile （Step 4以降）

```makefile
CC = clang
CFLAGS = $(shell sdl2-config --cflags) $(shell pkg-config --cflags SDL2_ttf) -Wall -Wextra
LDFLAGS = $(shell sdl2-config --libs) $(shell pkg-config --libs SDL2_ttf) -lutil
SRCS = main.c pty.c render.c
HDRS = pty.h render.h
TARGET = myterm

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)
```

## 7.3 ビルドと動作確認

```bash
make clean && make
./myterm
```

**期待される結果:**
- ウィンドウが開き、シェルプロンプト（`sh-3.2$ ` のような文字列）が表示される
- stderr にもデバッグ出力が表示される
- まだキー入力は PTY に送られないので、何を打ってもシェルに届かない
- ウィンドウを閉じると終了する

**確認すること:**
- `ps aux | grep sh` で子プロセスのシェルが起動していること
- プログラム終了後にシェルプロセスが残っていないこと

### コラム: なぜ Enter は '\n' ではなく '\r' なのか

物理端末の時代、Enter キー（Return キー）は**キャリッジリターン (CR, 0x0D, '\r')** を送信していた。キャリッジとはタイプライターの印字ヘッドのことで、Return キーはヘッドを行頭に戻す動作に由来する。

line discipline がこの CR を受け取り、必要に応じて CRLF（`\r\n`）に変換してシェルに渡す。この歴史的な慣例が現在のターミナルエミュレータにも引き継がれている。

---

# 第8章 Step 5 — 入出力を接続する

## 目標

キーボード入力を PTY に送り、PTY からの出力を画面に表示する。これで実際にコマンドが実行できるターミナルになる。さらに Ctrl-C などの制御キーと、子プロセスの終了検出を実装する。

## 8.1 背景知識: ASCII 制御文字

ASCII コードの 0x00〜0x1F は**制御文字**と呼ばれ、Ctrl+キーの組み合わせで入力できる:

```
Ctrl-A = 0x01 (SOH)     Ctrl-N = 0x0E (SO)
Ctrl-B = 0x02 (STX)     Ctrl-O = 0x0F (SI)
Ctrl-C = 0x03 (ETX)  ← SIGINT    Ctrl-P = 0x10 (DLE)
Ctrl-D = 0x04 (EOT)  ← EOF       Ctrl-Q = 0x11 (DC1) XON
Ctrl-E = 0x05 (ENQ)     Ctrl-R = 0x12 (DC2)
Ctrl-F = 0x06 (ACK)     Ctrl-S = 0x13 (DC3) XOFF
Ctrl-G = 0x07 (BEL)  ← ベル音    Ctrl-T = 0x14 (DC4)
Ctrl-H = 0x08 (BS)   ← バックスペース  Ctrl-U = 0x15 (NAK)
Ctrl-I = 0x09 (HT)   ← タブ      Ctrl-V = 0x16 (SYN)
Ctrl-J = 0x0A (LF)   ← 改行      Ctrl-W = 0x17 (ETB)
Ctrl-K = 0x0B (VT)      Ctrl-X = 0x18 (CAN)
Ctrl-L = 0x0C (FF)   ← 画面クリア Ctrl-Y = 0x19 (EM)
Ctrl-M = 0x0D (CR)   ← Enter     Ctrl-Z = 0x1A (SUB) ← SIGTSTP
```

計算方法: **Ctrl + 文字 の制御コード = 文字のASCIIコード - 0x40**
- Ctrl-C: 'C' (0x43) - 0x40 = 0x03
- Ctrl-D: 'D' (0x44) - 0x40 = 0x04
- Ctrl-Z: 'Z' (0x5A) - 0x40 = 0x1A

小文字でも大文字でも同じ制御コードになる（'c' = 0x63, 0x63 & 0x1F = 0x03）。

## 8.2 背景知識: エコーの流れ

PTY にエコーを任せる場合のデータフロー:

```
1. ユーザーが 'a' を押す
2. myterm が write(master_fd, "a", 1) する
3. line discipline が ECHO フラグにより 'a' を出力側にコピー
4. myterm が read(master_fd) で 'a' を受け取る
5. myterm が screen_putchar('a') で画面に表示
```

つまり、入力した文字は PTY を一周して戻ってくる。myterm 側で入力文字を直接表示すると**二重表示**になるので注意。

## 8.3 実装

main.c のメインループを修正する。`pty.c`, `pty.h`, `render.c`, `render.h` は Step 4 のまま変更なし。

### main.c （Step 5 完全版）

```c
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
```

**コード解説:**

- **`SDL_TEXTINPUT` → `pty_write()`**: 文字入力をそのまま PTY に送る。画面への表示は line discipline のエコーが行うので、ここでは `screen_putchar` を呼ばない
- **`SDLK_RETURN` → `"\r"`**: Enter は CR（0x0D）を送る。LF ではない
- **`SDLK_BACKSPACE` → `"\x7f"`**: macOS のデフォルト VERASE は DEL（0x7F）
- **Ctrl+キー**: `key - SDLK_a + 1` で制御コードに変換。例: Ctrl-C = `SDLK_c` - `SDLK_a` + 1 = 3 = 0x03
- **`shell_alive` フラグ**: シェル終了後も画面は開いたまま。任意のキーで閉じる

## 8.4 ビルドと動作確認

```bash
make clean && make
./myterm
```

**確認項目:**

1. シェルプロンプトが表示される
2. `echo hello` と打って Enter → `hello` が表示される
3. `ls /` と打って Enter → ルートディレクトリの内容が表示される
4. `/bin/ls -la /tmp` → ファイル一覧が表示される
5. `sleep 5` → Ctrl-C → すぐにプロンプトに戻る
6. `exit` → "[Process exited]" メッセージ → 任意のキーで終了

### コラム: DEL (0x7F) と BS (0x08) の歴史

Backspace キーが送る文字コードには2つの流派がある:

- **BS (0x08, Ctrl-H)**: 古い端末の標準。カーソルを1つ戻す
- **DEL (0x7F)**: 紙テープ時代の名残。紙テープのすべてのビットを穴あけ（=データ消去）する文字

macOS / Linux のデフォルトは DEL (0x7F)。`stty -a` コマンドで `erase` の設定を確認できる:
```
$ stty -a
...
erase = ^?;    ← ^? は DEL (0x7F) のこと
...
```

---

# 第9章 Step 6 — カーソルと仕上げ

## 目標

カーソルを点滅させ、ターミナルとしての完成度を高める。オプションとして ANSI エスケープシーケンスの基本的な処理も扱う。

## 9.1 背景知識: カーソルの点滅

ターミナルのカーソルは通常、500ms ごとに表示/非表示を切り替えて点滅する。キー入力があった時はリセットして常に表示状態にする（入力中にカーソルが消えると見づらいため）。

## 9.2 実装: カーソル点滅

render.c にカーソル点滅のロジックを追加する。

### render.h に追加

```c
/* カーソル点滅の更新。dirty なら 1 を返す */
int cursor_blink_update(void);

/* カーソル点滅をリセット（入力時に呼ぶ） */
void cursor_blink_reset(void);
```

### render.c の変更

```c
/* render.c に追加する部分 */

static int cursor_visible = 1;
static Uint32 last_blink = 0;
#define BLINK_INTERVAL 500  /* ミリ秒 */

int cursor_blink_update(void)
{
    Uint32 now = SDL_GetTicks();
    if (now - last_blink >= BLINK_INTERVAL) {
        cursor_visible = !cursor_visible;
        last_blink = now;
        return 1;  /* 再描画が必要 */
    }
    return 0;
}

void cursor_blink_reset(void)
{
    cursor_visible = 1;
    last_blink = SDL_GetTicks();
}
```

`render_screen()` のカーソル描画部分を変更:

```c
    /* カーソル描画（点滅対応） */
    if (cursor_visible) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_Rect cursor_rect = {
            cursor_x * char_w, cursor_y * char_h,
            char_w, char_h
        };
        SDL_RenderFillRect(renderer, &cursor_rect);
    }
```

### main.c のメインループに追加

```c
        /* カーソル点滅 */
        if (cursor_blink_update()) {
            dirty = 1;
        }

        /* イベント処理で入力があった箇所に追加: */
        cursor_blink_reset();
```

## 9.3 （オプション）ANSI エスケープシーケンスの処理

ここからは発展的な内容だ。`TERM=dumb` のままでも基本的なコマンドは動くが、エスケープシーケンスを処理すると `clear`、`ls --color`、`vi` などのプログラムがまともに動くようになる。

### エスケープシーケンスとは

ANSI エスケープシーケンスは ESC (0x1B) で始まるバイト列:

```
ESC [ パラメータ ; パラメータ 最終文字
```

例:
```
\x1b[2J     → 画面クリア (J = Erase in Display, 2 = 全画面)
\x1b[H      → カーソルをホーム (0,0) に移動
\x1b[10;5H  → カーソルを10行5列に移動
\x1b[A      → カーソルを1行上に
\x1b[B      → カーソルを1行下に
\x1b[C      → カーソルを1列右に
\x1b[D      → カーソルを1列左に
\x1b[K      → カーソルから行末までクリア
\x1b[31m    → 文字色を赤に（SGR: Select Graphic Rendition）
\x1b[0m     → 属性リセット
```

### 状態マシンの設計

エスケープシーケンスのパーサーは状態マシンで実装する:

```
        ESC (0x1b)           '[' (0x5b)          最終文字 (0x40-0x7E)
NORMAL ──────────→ ESC_SEEN ──────────→ CSI_PARAM ────────────────→ NORMAL
                      │                    ↑  │
                      │ ESC/その他          │  │ 数字/';' (パラメータ蓄積)
                      └──→ NORMAL          └──┘
```

### パーサーの実装例

```c
/* render.c に追加 */

enum { STATE_NORMAL, STATE_ESC, STATE_CSI };

static int esc_state = STATE_NORMAL;
static int esc_params[8];
static int esc_nparam = 0;
static int esc_cur_param = 0;

/* screen_putchar を screen_process_byte に改名し、
   エスケープシーケンスのパースを追加 */
void screen_process_byte(char c)
{
    switch (esc_state) {
    case STATE_NORMAL:
        if (c == '\x1b') {
            esc_state = STATE_ESC;
        } else {
            screen_putchar_raw(c);  /* 元の screen_putchar のロジック */
        }
        break;

    case STATE_ESC:
        if (c == '[') {
            esc_state = STATE_CSI;
            esc_nparam = 0;
            esc_cur_param = 0;
            memset(esc_params, 0, sizeof(esc_params));
        } else {
            esc_state = STATE_NORMAL;  /* 未対応シーケンス → 無視 */
        }
        break;

    case STATE_CSI:
        if (c >= '0' && c <= '9') {
            esc_cur_param = esc_cur_param * 10 + (c - '0');
        } else if (c == ';') {
            if (esc_nparam < 8) {
                esc_params[esc_nparam++] = esc_cur_param;
            }
            esc_cur_param = 0;
        } else if (c >= 0x40 && c <= 0x7E) {
            /* 最終文字: シーケンス確定 */
            if (esc_nparam < 8) {
                esc_params[esc_nparam++] = esc_cur_param;
            }
            csi_dispatch(c);
            esc_state = STATE_NORMAL;
        } else {
            esc_state = STATE_NORMAL;  /* 不正なバイト → リセット */
        }
        break;
    }
}

static void csi_dispatch(char final)
{
    int p0 = (esc_nparam > 0) ? esc_params[0] : 0;
    int p1 = (esc_nparam > 1) ? esc_params[1] : 0;

    switch (final) {
    case 'H': case 'f':
        /* カーソル移動: \x1b[行;列H */
        cursor_y = (p0 > 0 ? p0 - 1 : 0);
        cursor_x = (p1 > 0 ? p1 - 1 : 0);
        if (cursor_y >= ROWS) cursor_y = ROWS - 1;
        if (cursor_x >= COLS) cursor_x = COLS - 1;
        break;
    case 'A':
        /* カーソル上移動 */
        cursor_y -= (p0 > 0 ? p0 : 1);
        if (cursor_y < 0) cursor_y = 0;
        break;
    case 'B':
        /* カーソル下移動 */
        cursor_y += (p0 > 0 ? p0 : 1);
        if (cursor_y >= ROWS) cursor_y = ROWS - 1;
        break;
    case 'C':
        /* カーソル右移動 */
        cursor_x += (p0 > 0 ? p0 : 1);
        if (cursor_x >= COLS) cursor_x = COLS - 1;
        break;
    case 'D':
        /* カーソル左移動 */
        cursor_x -= (p0 > 0 ? p0 : 1);
        if (cursor_x < 0) cursor_x = 0;
        break;
    case 'J':
        /* 画面クリア */
        if (p0 == 2) {
            for (int i = 0; i < ROWS; i++) {
                memset(screen[i], ' ', COLS);
            }
            cursor_x = 0;
            cursor_y = 0;
        }
        break;
    case 'K':
        /* 行クリア (カーソル位置から行末まで) */
        if (p0 == 0) {
            memset(&screen[cursor_y][cursor_x], ' ', COLS - cursor_x);
        }
        break;
    case 'm':
        /* SGR (色設定) → 無視 */
        break;
    }
}
```

このパーサーを有効にする場合は、pty.c の `setenv("TERM", "dumb", 1)` を `setenv("TERM", "xterm", 1)` に変更する。

## 9.4 最終的なファイル構成

```
myterm/
├── main.c       (~120行)  イベントループ、SDL初期化、入力振り分け
├── pty.c        (~60行)   forkpty、read/write
├── pty.h        (~20行)   PTY関数宣言
├── render.c     (~200行)  画面バッファ、テキスト描画、カーソル、ESCパーサー
├── render.h     (~25行)   描画関数・画面状態の宣言
├── Makefile     (~15行)   ビルドルール
└── plan.md
```

## 9.5 ビルドと最終動作確認

```bash
make clean && make
./myterm
```

**最終確認チェックリスト:**

- [ ] `echo hello` が動作する
- [ ] `ls /` の出力が正しく表示される
- [ ] `/bin/cat` でテキスト入力・表示ができる
- [ ] Ctrl-C でプロセスを中断できる
- [ ] Ctrl-D で cat を終了できる
- [ ] `exit` でシェルが終了し、メッセージが表示される
- [ ] カーソルが点滅する
- [ ] 24行を超える出力でスクロールが動作する
- [ ] ウィンドウを閉じて正常に終了する
- [ ] CPU 使用率が低い（数%以下）

### コラム: ターミナルエミュレータの性能

現代のターミナルエミュレータは `cat large_file.txt` で大量のテキストを高速に表示する必要がある。本書の myterm は毎フレーム24行のテクスチャを再生成するため、大量の出力では遅い。

高速化のアプローチ:
- **行単位のキャッシュ**: 変更された行のテクスチャのみ再生成する
- **文字単位のテクスチャアトラス**: 各文字のグリフを1枚のテクスチャにまとめ、UV座標で切り出す
- **GPU シェーダー**: Alacritty や kitty が採用。グリフデータを GPU に送り、シェーダーで描画

---

# 付録A APIリファレンス早見表

## SDL2

| 関数 | 用途 | 使用ステップ |
|------|------|-------------|
| `SDL_Init(SDL_INIT_VIDEO)` | SDL2 初期化 | 1 |
| `SDL_Quit()` | SDL2 終了 | 1 |
| `SDL_CreateWindow(...)` | ウィンドウ作成 | 1 |
| `SDL_DestroyWindow(w)` | ウィンドウ破棄 | 1 |
| `SDL_CreateRenderer(...)` | レンダラー作成 | 1 |
| `SDL_DestroyRenderer(r)` | レンダラー破棄 | 1 |
| `SDL_PollEvent(&e)` | イベント取得 (非ブロック) | 1 |
| `SDL_SetRenderDrawColor(r,R,G,B,A)` | 描画色設定 | 1 |
| `SDL_RenderClear(r)` | 画面クリア | 1 |
| `SDL_RenderPresent(r)` | 画面更新 | 1 |
| `SDL_RenderCopy(r,t,src,dst)` | テクスチャ描画 | 2 |
| `SDL_RenderFillRect(r,rect)` | 矩形塗りつぶし | 6 |
| `SDL_Delay(ms)` | スリープ | 1 |
| `SDL_GetTicks()` | 経過ミリ秒取得 | 6 |
| `SDL_StartTextInput()` | テキスト入力有効化 | 3 |
| `SDL_SetWindowSize(w,W,H)` | ウィンドウサイズ変更 | 2 |
| `SDL_GetError()` | エラーメッセージ取得 | 1 |

## SDL2_ttf

| 関数 | 用途 | 使用ステップ |
|------|------|-------------|
| `TTF_Init()` | TTF 初期化 | 2 |
| `TTF_Quit()` | TTF 終了 | 2 |
| `TTF_OpenFont(path, ptsize)` | フォント読込 | 2 |
| `TTF_CloseFont(f)` | フォント解放 | 2 |
| `TTF_RenderText_Blended(f,text,color)` | テキスト→Surface | 2 |
| `TTF_SizeText(f,text,&w,&h)` | テキストサイズ計測 | 2 |
| `SDL_CreateTextureFromSurface(r,s)` | Surface→Texture | 2 |
| `SDL_FreeSurface(s)` | Surface 解放 | 2 |
| `SDL_DestroyTexture(t)` | Texture 解放 | 2 |
| `SDL_QueryTexture(t,NULL,NULL,&w,&h)` | Texture サイズ取得 | 2 |

## POSIX (PTY / プロセス)

| 関数 | ヘッダ | 用途 | 使用ステップ |
|------|--------|------|-------------|
| `forkpty(&master,NULL,NULL,&ws)` | `<util.h>` | PTY作成+fork | 4 |
| `execl(path,arg0,...,NULL)` | `<unistd.h>` | プログラム実行 | 4 |
| `_exit(status)` | `<unistd.h>` | 即座にプロセス終了 | 4 |
| `read(fd,buf,size)` | `<unistd.h>` | 読み取り | 4 |
| `write(fd,buf,size)` | `<unistd.h>` | 書き込み | 5 |
| `close(fd)` | `<unistd.h>` | fd を閉じる | 4 |
| `fcntl(fd,F_SETFL,flags)` | `<fcntl.h>` | fd のフラグ設定 | 4 |
| `setenv(name,value,overwrite)` | `<stdlib.h>` | 環境変数設定 | 4 |
| `waitpid(pid,&status,flags)` | `<sys/wait.h>` | 子プロセスの終了待ち | 5 |
| `ioctl(fd,TIOCSWINSZ,&ws)` | `<sys/ioctl.h>` | ターミナルサイズ設定 | 6(opt) |

---

# 付録B トラブルシューティング

## ビルドエラー

### `sdl2-config: command not found`

SDL2 がインストールされていない。`brew install sdl2` を実行。

### `pkg-config: command not found` または SDL2_ttf が見つからない

```bash
brew install pkg-config sdl2_ttf
```

### `fatal error: 'SDL.h' file not found`

Makefile で `sdl2-config --cflags` の出力がインクルードパスに含まれていない。Makefile の `CFLAGS` を確認。

### `Undefined symbols: _forkpty`

リンクフラグに `-lutil` が不足。Makefile の `LDFLAGS` に追加。

## 実行時の問題

### ウィンドウにキーボードフォーカスが当たらない

macOS の仕様。ウィンドウをクリックするか Cmd-Tab で切り替え。

### 文字が表示されない（黒い画面のまま）

- フォントパスが正しいか確認: `ls /System/Library/Fonts/Menlo.ttc`
- `TTF_OpenFont` の戻り値を確認（stderr にエラーが出ていないか）

### 文字が二重に表示される

PTY のエコーと myterm の両方で文字を表示している。`SDL_TEXTINPUT` で `screen_putchar` を呼んでいないか確認。文字の表示は PTY からの読み取りのみで行う（Step 5）。

### Backspace が効かない

`0x7F` (DEL) の代わりに `0x08` (BS) を送ってみる:
```c
pty_write(master_fd, "\x08", 1);
```
`stty -a` で `erase` の設定を確認。

### CPU 使用率が 100% になる

- `SDL_Delay(16)` が入っているか確認
- `SDL_RENDERER_PRESENTVSYNC` フラグを確認
- dirty フラグが正しくリセットされているか確認

### シェルプロンプトが表示されない

- `pty_spawn` が 0 を返しているか確認
- `pty_read` の戻り値を stderr に出力してデバッグ
- `O_NONBLOCK` が設定されているか確認

---

# 付録C さらに先へ

myterm をさらに発展させるためのアイデア:

## 色のサポート
- SGR エスケープシーケンス (`\x1b[31m` 等) を解釈し、各セルに前景色/背景色を持たせる
- 16色 → 256色 → 24bit True Color と段階的に対応可能

## スクロールバック
- 画面外にスクロールした行を保持するバッファ（通常数千行）
- Shift+PageUp/Down でスクロールバック

## 選択とコピー
- マウスドラッグでテキスト選択
- Cmd-C でクリップボードにコピー

## Unicode 対応
- UTF-8 デコーダの実装
- CJK文字（全角文字）の幅2セル対応
- `TTF_RenderUTF8_Blended` の使用

## タブ
- 複数のシェルセッションをタブで切り替え

## 設定ファイル
- フォント、フォントサイズ、色テーマなどを設定ファイルから読む

---

### コラム: 著名なターミナルエミュレータのアーキテクチャ

| ターミナル | 言語 | 描画 | 特徴 |
|-----------|------|------|------|
| **xterm** | C | X11/Xlib | 1984年〜。ANSI/VT互換のリファレンス実装 |
| **GNOME Terminal** | C | VTE (GTK) | VTEライブラリが端末エミュレーション部分を担当 |
| **Terminal.app** | Obj-C | AppKit | macOS 標準。非公開ソース |
| **iTerm2** | Obj-C/Swift | Metal | macOS 用高機能ターミナル。GPU描画 |
| **Alacritty** | Rust | OpenGL/Metal | GPU アクセラレーション。設定ファイルのみ（GUIなし） |
| **kitty** | C+Python | OpenGL | 画像表示対応。独自プロトコル |
| **WezTerm** | Rust | OpenGL/Metal | 組み込みの Lua スクリプティング |
| **Windows Terminal** | C++ | DirectX | Microsoft 公式。2019年〜。オープンソース |

本書の myterm は xterm の最小再現に相当する。ここから色やUnicodeを足していけば、実用的なターミナルエミュレータに近づいていく。

---

**おわりに**

おめでとう。約700行のCコードで、キーボードからモニターまでの全レイヤーを通るターミナルエミュレータが完成した。

物理端末の時代から50年以上が経った今も、PTY と line discipline という仕組みは UNIX 系 OS の根幹に息づいている。このプロジェクトを通じて、普段何気なく使っているターミナルの裏側にある奥深い世界に触れることができたなら幸いだ。
