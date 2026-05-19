/*
 * 俄罗斯方块 (Tetris) - Windows 控制台版
 * 使用 <windows.h> 和 <conio.h> 实现
 * 功能：7种标准方块、自动下落、方向键/WASD控制、旋转、消行计分
 *       等级加速、触顶结束、落地缓冲（0.5s内可移动/旋转，最多2次微调）
 *       旋转防抖（200ms冷却），无闪烁渲染
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <conio.h>
#include <time.h>
#include <stdarg.h>

/* ================== 宏定义 ================== */
#define BOARD_W    10
#define BOARD_H    20
#define BUFFER      2
#define TOTAL_H    (BOARD_H + BUFFER)

#define SCR_W   (3 + BOARD_W * 2 + 22)
#define SCR_H   (BOARD_H + 3)

#define PIECE_I  0
#define PIECE_J  1
#define PIECE_L  2
#define PIECE_O  3
#define PIECE_S  4
#define PIECE_T  5
#define PIECE_Z  6
#define PIECE_MAX 7

#define KEY_UP    72
#define KEY_DOWN  80
#define KEY_LEFT  75
#define KEY_RIGHT 77

/* 落地缓冲时间（毫秒） */
#define LOCK_DELAY_MS 500
/* 旋转冷却时间（毫秒）：防止按住按键连续旋转 */
#define ROTATE_COOLDOWN_MS 200

#define COL_DEF  7
#define COL_I    11
#define COL_J    9
#define COL_L    6
#define COL_O    14
#define COL_S    10
#define COL_T    13
#define COL_Z    12

/* ================== 方块形状 ================== */
static const int SHAPES[PIECE_MAX][4][4] = {
    { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} },
    { {1,0,0},   {1,1,1},   {0,0,0},   {0,0,0}   },
    { {0,0,1},   {1,1,1},   {0,0,0},   {0,0,0}   },
    { {0,1,1},   {0,1,1},   {0,0,0},   {0,0,0}   },
    { {0,1,1},   {1,1,0},   {0,0,0},   {0,0,0}   },
    { {0,1,0},   {1,1,1},   {0,0,0},   {0,0,0}   },
    { {1,1,0},   {0,1,1},   {0,0,0},   {0,0,0}   }
};

static const int PIECE_SZ[PIECE_MAX] = { 4, 3, 3, 3, 3, 3, 3 };
static const int PIECE_CLR[PIECE_MAX] = { COL_I, COL_J, COL_L, COL_O, COL_S, COL_T, COL_Z };

/* ================== 全局变量 ================== */
static int board[TOTAL_H][BOARD_W];
static int cur_type, cur_rot, cur_x, cur_y;
static int next_type;
static int score, lines_cleared, level;
static int game_over;

/* === 落地缓冲机制 === */
static int  lock_delay_active;
static DWORD lock_delay_start;
static int  lock_delay_moves;

/* 旋转防抖 */
static DWORD last_rotate_time;

static HANDLE hConOut;

/* ================== 屏幕缓冲区 ================== */
#define BUF_W SCR_W
#define BUF_H SCR_H
static CHAR_INFO buf[BUF_H][BUF_W];

#define BUF_SET(x, y, ch, col) do { \
    buf[y][x].Char.UnicodeChar = (ch); \
    buf[y][x].Attributes = (WORD)(col); \
} while(0)

/* ================== 函数声明 ================== */
static void init_game(void);
static void spawn_piece(void);
static void get_shape(int type, int rot, int out[4][4]);
static int  check_collision(int x, int y, int rot);
static void lock_piece(void);
static void clear_lines(void);
static int  has_ground_below(void);
static void finalize_piece(void);
static int  move_left(void);
static int  move_right(void);
static int  move_down(void);
static int  rotate_piece(void);
static int  hard_drop(void);
static int  get_ghost_y(void);
static void handle_input(void);
static void render_frame(void);
static void show_game_over(void);
static int  get_drop_interval(void);
static void try_lock(void);

/* ================== 方块旋转 ================== */

static void get_shape(int type, int rot, int out[4][4]) {
    int sz = PIECE_SZ[type];
    int tmp[4][4] = {0};
    for (int r = 0; r < sz; r++)
        for (int c = 0; c < sz; c++)
            tmp[r][c] = SHAPES[type][r][c];

    for (int n = 0; n < rot; n++) {
        int rot90[4][4] = {0};
        for (int i = 0; i < sz; i++)
            for (int j = 0; j < sz; j++)
                rot90[j][sz - 1 - i] = tmp[i][j];
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                tmp[i][j] = rot90[i][j];
    }

    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[r][c] = tmp[r][c];
}

/* ================== 碰撞检测 ================== */

static int check_collision(int x, int y, int rot) {
    int shape[4][4];
    get_shape(cur_type, rot, shape);

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape[r][c]) {
                int bx = x + c;
                int by = y + r;
                if (bx < 0 || bx >= BOARD_W || by >= TOTAL_H) return 1;
                if (by < 0) continue;
                if (board[by][bx]) return 1;
            }
        }
    }
    return 0;
}

/* ================== 方块操作 ================== */

static void spawn_piece(void) {
    cur_type = next_type;
    next_type = rand() % PIECE_MAX;
    cur_rot = 0;
    cur_x = (BOARD_W - PIECE_SZ[cur_type]) / 2;
    cur_y = 0;
    lock_delay_active = 0;
    lock_delay_moves = 0;
    last_rotate_time = 0;

    if (check_collision(cur_x, cur_y, cur_rot))
        game_over = 1;
}

static void lock_piece(void) {
    int shape[4][4];
    get_shape(cur_type, cur_rot, shape);
    int color = PIECE_CLR[cur_type];

    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (shape[r][c]) {
                int by = cur_y + r, bx = cur_x + c;
                if (by >= 0 && by < TOTAL_H && bx >= 0 && bx < BOARD_W)
                    board[by][bx] = color;
            }
}

static void clear_lines(void) {
    int cleared = 0;
    for (int row = TOTAL_H - 1; row >= 0; row--) {
        int full = 1;
        for (int col = 0; col < BOARD_W; col++)
            if (!board[row][col]) { full = 0; break; }

        if (full) {
            for (int r = row; r > 0; r--)
                for (int c = 0; c < BOARD_W; c++)
                    board[r][c] = board[r - 1][c];
            for (int c = 0; c < BOARD_W; c++) board[0][c] = 0;
            cleared++;
            row++;
        }
    }

    if (cleared > 0) {
        lines_cleared += cleared;
             if (cleared == 1) score += 100 * level;
        else if (cleared == 2) score += 300 * level;
        else if (cleared == 3) score += 500 * level;
        else if (cleared == 4) score += 800 * level;
        level = lines_cleared / 10 + 1;
    }
}

static void finalize_piece(void) {
    lock_delay_active = 0;
    lock_delay_moves = 0;
    lock_piece();
    clear_lines();
    spawn_piece();
}

static int has_ground_below(void) {
    return check_collision(cur_x, cur_y + 1, cur_rot);
}

/*
 * 向左移动。如果在缓冲期内，移动后重新计时。
 * 如果移动后下方空了（悬空），取消缓冲继续下落
 * 每段缓冲期最多微调 2 次
 */
static int move_left(void) {
    if (!check_collision(cur_x - 1, cur_y, cur_rot)) {
        cur_x--;
        if (lock_delay_active) {
            lock_delay_moves++;
            if (lock_delay_moves >= 2) {
                finalize_piece();
            } else {
                lock_delay_start = GetTickCount64();
                if (!has_ground_below()) lock_delay_active = 0;
            }
        }
        return 1;
    }
    return 0;
}

/*
 * 向右移动。同上。
 */
static int move_right(void) {
    if (!check_collision(cur_x + 1, cur_y, cur_rot)) {
        cur_x++;
        if (lock_delay_active) {
            lock_delay_moves++;
            if (lock_delay_moves >= 2) {
                finalize_piece();
            } else {
                lock_delay_start = GetTickCount64();
                if (!has_ground_below()) lock_delay_active = 0;
            }
        }
        return 1;
    }
    return 0;
}

/*
 * 尝试向下移动一格。
 * - 如果可以下落 → 正常下落，清除缓冲状态
 * - 如果无法下落 → 进入锁定缓冲（500ms 内可继续操作）
 * - 若已经在缓冲期内按 ↓ → 强制固定
 */
static int move_down(void) {
    if (!check_collision(cur_x, cur_y + 1, cur_rot)) {
        cur_y++;
        lock_delay_active = 0;
        return 1;
    }

    if (!lock_delay_active) {
        lock_delay_active = 1;
        lock_delay_start = GetTickCount64();
        lock_delay_moves = 0;
        return 0;
    }

    finalize_piece();
    return 0;
}

/*
 * 旋转方块。有 200ms 冷却防抖，防止按住按键连续旋转。
 * 如果在缓冲期内，旋转后重新计时。
 * 如果旋转后下方空了（悬空），取消缓冲恢复正常下落
 */
static int rotate_piece(void) {
    DWORD now = GetTickCount64();

    if (now - last_rotate_time < ROTATE_COOLDOWN_MS)
        return 0;

    int nr = (cur_rot + 1) % 4;
    if (!check_collision(cur_x, cur_y, nr)) {
        cur_rot = nr;
        last_rotate_time = now;
        if (lock_delay_active) {
            lock_delay_start = now;
            if (!has_ground_below()) lock_delay_active = 0;
        }
        return 1;
    }
    int kicks[] = { -1, 1, -2, 2 };
    for (int i = 0; i < 4; i++)
        if (!check_collision(cur_x + kicks[i], cur_y, nr)) {
            cur_x += kicks[i]; cur_rot = nr;
            last_rotate_time = now;
            if (lock_delay_active) {
                lock_delay_start = now;
                if (!has_ground_below()) lock_delay_active = 0;
            }
            return 1;
        }
    return 0;
}

/*
 * 硬降：快速落到幽灵位置，然后进入 500ms 缓冲期
 * 玩家可继续微调（左右/旋转），缓冲超时或按 ↓ 才固定
 */
static int hard_drop(void) {
    lock_delay_active = 0;

    int n = 0;
    while (!check_collision(cur_x, cur_y + 1, cur_rot)) { cur_y++; n++; }
    score += n * 2;

    lock_delay_active = 1;
    lock_delay_start = GetTickCount64();
    lock_delay_moves = 0;

    return n;
}

static int get_ghost_y(void) {
    int gy = cur_y;
    while (!check_collision(cur_x, gy + 1, cur_rot)) gy++;
    return gy;
}

/* 检查缓冲是否超时 */
static void try_lock(void) {
    if (!lock_delay_active) return;

    DWORD elapsed = GetTickCount64() - lock_delay_start;
    if (elapsed >= LOCK_DELAY_MS) {
        finalize_piece();
    }
}

/* ================== 输入处理 ================== */

static void handle_input(void) {
    if (!_kbhit()) return;
    int ch = _getch();

    if (ch == 0xE0 || ch == 0x00) {
        ch = _getch();
        switch (ch) {
            case KEY_LEFT:  move_left();  break;
            case KEY_RIGHT: move_right(); break;
            case KEY_DOWN:  move_down();  break;
            case KEY_UP:    rotate_piece(); break;
        }
        return;
    }

    switch (ch) {
        case 'a': case 'A': move_left();   break;
        case 'd': case 'D': move_right();  break;
        case 's': case 'S': move_down();   break;
        case 'w': case 'W': rotate_piece(); break;
        case ' ':           hard_drop();   break;
        case 27:            game_over = 1; break;
    }

    while (_kbhit()) _getch();
}

/* ================== 绘制（纯内存 + 一次性写入） ================== */

static void draw_cell(int sx, int sy, int color, int ghost) {
    if (sx < 0 || sx >= BOARD_W * 2 || sy < 0 || sy >= BOARD_H) return;
    int px = 1 + sx;
    int py = 1 + sy;
    if (ghost) BUF_SET(px, py, '.', 8);
    else       BUF_SET(px, py, '#', color);
}

static void blit(int px, int py, int type, int rot, int ghost) {
    int shape[4][4];
    get_shape(type, rot, shape);

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shape[r][c]) {
                int by = py + r;
                int bx = px + c;
                if (by >= BUFFER && by < TOTAL_H && bx >= 0 && bx < BOARD_W) {
                    draw_cell(bx * 2, by - BUFFER,
                              ghost ? 8 : PIECE_CLR[type], ghost);
                }
            }
        }
    }
}

static void buf_printf(int x, int y, int color, const char *fmt, ...) {
    char tmp[80];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    for (int i = 0; tmp[i] && x + i < BUF_W; i++)
        BUF_SET(x + i, y, tmp[i], color);
}

/* ================== 核心渲染 ================== */

static void render_frame(void) {
    static int first = 1;

    if (first) {
        for (int y = 0; y < BUF_H; y++)
            for (int x = 0; x < BUF_W; x++)
                BUF_SET(x, y, ' ', COL_DEF);

        BUF_SET(0, 0, '+', COL_DEF);
        for (int x = 1; x < 1 + BOARD_W * 2; x++) BUF_SET(x, 0, '-', COL_DEF);
        BUF_SET(1 + BOARD_W * 2, 0, '+', COL_DEF);

        for (int y = 1; y <= BOARD_H; y++) {
            BUF_SET(0, y, '|', COL_DEF);
            BUF_SET(1 + BOARD_W * 2, y, '|', COL_DEF);
        }

        BUF_SET(0, 1 + BOARD_H, '+', COL_DEF);
        for (int x = 1; x < 1 + BOARD_W * 2; x++) BUF_SET(x, 1 + BOARD_H, '-', COL_DEF);
        BUF_SET(1 + BOARD_W * 2, 1 + BOARD_H, '+', COL_DEF);

        int px = 3 + BOARD_W * 2;
        buf_printf(px, 0, COL_DEF, "--- TETRIS ---");
        buf_printf(px, 2, COL_DEF, "Next:");
        buf_printf(px, 8, COL_DEF, "Score: 0");
        buf_printf(px, 9, COL_DEF, "Lines: 0");
        buf_printf(px, 10, COL_DEF, "Level: 1");
        buf_printf(px, 12, COL_DEF, "Controls:");
        buf_printf(px, 13, COL_DEF, "W/Up  : Rotate");
        buf_printf(px, 14, COL_DEF, "A/Left: Left");
        buf_printf(px, 15, COL_DEF, "D/Right: Right");
        buf_printf(px, 16, COL_DEF, "S/Down: Drop");
        buf_printf(px, 17, COL_DEF, "Space : Hard Drop");
        buf_printf(px, 18, COL_DEF, "ESC   : Quit");

        first = 0;
    }

    for (int y = 1; y <= BOARD_H; y++)
        for (int x = 1; x < 1 + BOARD_W * 2; x++)
            BUF_SET(x, y, ' ', COL_DEF);

    for (int r = BUFFER; r < TOTAL_H; r++)
        for (int c = 0; c < BOARD_W; c++)
            if (board[r][c])
                draw_cell(c * 2, r - BUFFER, board[r][c], 0);

    if (!game_over) {
        int gy = get_ghost_y();
        if (gy != cur_y) blit(cur_x, gy, cur_type, cur_rot, 1);
    }

    if (!game_over) blit(cur_x, cur_y, cur_type, cur_rot, 0);

    int px = 3 + BOARD_W * 2;
    buf_printf(px, 8, COL_DEF, "Score: %d   ", score);
    buf_printf(px, 9, COL_DEF, "Lines: %d   ", lines_cleared);
    buf_printf(px, 10, COL_DEF, "Level: %d   ", level);

    for (int y = 3; y <= 6; y++)
        for (int x = px; x < px + 12; x++)
            BUF_SET(x, y, ' ', COL_DEF);

    int shape[4][4];
    get_shape(next_type, 0, shape);
    int sz = PIECE_SZ[next_type];
    for (int r = 0; r < sz; r++)
        for (int c = 0; c < sz; c++)
            if (shape[r][c])
                BUF_SET(px + c * 2, 3 + r, '#', PIECE_CLR[next_type]);

    SMALL_RECT rect = { 0, 0, BUF_W - 1, BUF_H - 1 };
    COORD buf_sz   = { BUF_W, BUF_H };
    COORD zero     = { 0, 0 };
    WriteConsoleOutputW(hConOut, (CHAR_INFO *)buf, buf_sz, zero, &rect);
}

/* ================== 游戏结束 ================== */

static void show_game_over(void) {
    int cx = (1 + BOARD_W) / 2;
    int cy = BOARD_H / 2;

    buf_printf(cx - 6, cy - 2, COL_DEF, "==============");
    buf_printf(cx - 6, cy - 1, COL_DEF, "  GAME OVER! ");
    buf_printf(cx - 6, cy,     COL_DEF, "  Score: %d  ", score);
    buf_printf(cx - 6, cy + 1, COL_DEF, "  Lines: %d  ", lines_cleared);
    buf_printf(cx - 6, cy + 2, COL_DEF, "==============");

    SMALL_RECT rect = { 0, 0, BUF_W - 1, BUF_H - 1 };
    COORD buf_sz   = { BUF_W, BUF_H };
    COORD zero     = { 0, 0 };
    WriteConsoleOutputW(hConOut, (CHAR_INFO *)buf, buf_sz, zero, &rect);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConOut, &csbi);
    COORD pos = { 0, (SHORT)(BOARD_H + 2) };
    SetConsoleCursorPosition(hConOut, pos);
    printf("Press any key to exit...");

    _getch();
}

/* ================== 初始化 ================== */

static void init_game(void) {
    for (int r = 0; r < TOTAL_H; r++)
        for (int c = 0; c < BOARD_W; c++)
            board[r][c] = 0;

    score = 0; lines_cleared = 0; level = 1; game_over = 0;
    lock_delay_active = 0;
    last_rotate_time = 0;
    srand((unsigned int)time(NULL));
    next_type = rand() % PIECE_MAX;
    spawn_piece();
}

static int get_drop_interval(void) {
    int v = 500 - (level - 1) * 40;
    return (v < 80) ? 80 : v;
}

/* ================== 主函数 ================== */

int main(void) {
    SetConsoleOutputCP(CP_UTF8);

    hConOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConOut == INVALID_HANDLE_VALUE) {
        printf("Failed to get console handle!\n");
        return 1;
    }

    SetConsoleTitle(TEXT("Tetris - C Classic"));

    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hConOut, &ci);
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(hConOut, &ci);

    init_game();
    render_frame();

    DWORD last_drop = GetTickCount64();

    while (!game_over) {
        handle_input();

        DWORD now = GetTickCount64();

        if (!lock_delay_active) {
            if (now - last_drop >= (DWORD)get_drop_interval()) {
                move_down();
                last_drop = now;
            }
        }

        try_lock();
        render_frame();
        Sleep(15);
    }

    show_game_over();

    ci.bVisible = TRUE;
    SetConsoleCursorInfo(hConOut, &ci);
    return 0;
}
