/*
 * Snake Game - Windows Console Version (C Language)
 * Use <windows.h> and <conio.h>
 * Controls: WASD / Arrow Keys  |  ESC to quit
 * Features: food growth, collision detection, score display
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <conio.h>
#include <time.h>

/* ====== Macros ====== */
#define WIDTH  50
#define HEIGHT 24
#define MAX_SNAKE_LEN (WIDTH * HEIGHT)

/* Direction */
typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

/* Snake node */
typedef struct {
    int x;
    int y;
} SnakeNode;

/* ====== Globals ====== */
static SnakeNode snake[MAX_SNAKE_LEN];
static int snake_len;
static Direction dir;
static Direction next_dir;
static int food_x, food_y;
static int score;
static int game_over;

/* Store the tail position before move, used to clear it in drawing */
static int prev_tail_x, prev_tail_y;

static HANDLE hConsole;

/* ====== Functions ====== */
static void init_game(void);
static void spawn_food(void);
static void handle_input(void);
static void move_snake(void);
static int  check_collision(int head_x, int head_y);
static void draw_frame(void);
static void gotoxy(int x, int y);
static void hide_cursor(void);
static void show_game_over(void);

/* Move cursor to (x, y) */
static void gotoxy(int x, int y) {
    COORD coord;
    coord.X = (SHORT)x;
    coord.Y = (SHORT)y;
    SetConsoleCursorPosition(hConsole, coord);
}

/* Hide blinking cursor */
static void hide_cursor(void) {
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

/* Initialize game state */
static void init_game(void) {
    snake_len = 2;
    int start_x = WIDTH / 2;
    int start_y = HEIGHT / 2;

    /* head at rightmost, body extends to the left
       so moving RIGHT has open space ahead */
    for (int i = 0; i < snake_len; i++) {
        snake[i].x = start_x - i;
        snake[i].y = start_y;
    }

    dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    score = 0;
    game_over = 0;

    srand((unsigned int)time(NULL));
    spawn_food();
}

/* Spawn food at a random empty cell */
static void spawn_food(void) {
    int free_cells = 0;

    /* Count empty cells */
    for (int y = 1; y < HEIGHT - 1; y++) {
        for (int x = 1; x < WIDTH - 1; x++) {
            int occupied = 0;
            for (int i = 0; i < snake_len; i++) {
                if (snake[i].x == x && snake[i].y == y) {
                    occupied = 1;
                    break;
                }
            }
            if (!occupied) free_cells++;
        }
    }

    if (free_cells == 0) {
        game_over = 1;  /* Snake fills entire area --> win */
        return;
    }

    /* Pick a random empty cell */
    int target = rand() % free_cells;
    int count = 0;
    for (int y = 1; y < HEIGHT - 1; y++) {
        for (int x = 1; x < WIDTH - 1; x++) {
            int occupied = 0;
            for (int i = 0; i < snake_len; i++) {
                if (snake[i].x == x && snake[i].y == y) {
                    occupied = 1;
                    break;
                }
            }
            if (!occupied) {
                if (count == target) {
                    food_x = x;
                    food_y = y;
                    return;
                }
                count++;
            }
        }
    }
}

/* Non-blocking keyboard input handling */
static void handle_input(void) {
    if (!_kbhit()) return;

    int ch = _getch();

    /* Arrow keys: first byte is 0xE0 or 0x00 */
    if (ch == 0xE0 || ch == 0x00) {
        /* Must wait for the second byte */
        ch = _getch();
        switch (ch) {
            case 72:  /* Up */
                if (dir != DIR_DOWN) next_dir = DIR_UP;
                break;
            case 80:  /* Down */
                if (dir != DIR_UP) next_dir = DIR_DOWN;
                break;
            case 75:  /* Left */
                if (dir != DIR_RIGHT) next_dir = DIR_LEFT;
                break;
            case 77:  /* Right */
                if (dir != DIR_LEFT) next_dir = DIR_RIGHT;
                break;
        }
        return;
    }

    /* WASD */
    switch (ch) {
        case 'w': case 'W':
            if (dir != DIR_DOWN) next_dir = DIR_UP;
            break;
        case 's': case 'S':
            if (dir != DIR_UP) next_dir = DIR_DOWN;
            break;
        case 'a': case 'A':
            if (dir != DIR_RIGHT) next_dir = DIR_LEFT;
            break;
        case 'd': case 'D':
            if (dir != DIR_LEFT) next_dir = DIR_RIGHT;
            break;
        case 27:   /* ESC */
            game_over = 1;
            break;
    }

    /* Drain any remaining input in buffer to avoid lag/overflow */
    while (_kbhit()) _getch();
}

/* Check if (head_x, head_y) collides with wall or body */
static int check_collision(int head_x, int head_y) {
    /* Only wall collision causes game over, hitting yourself is allowed */
    if (head_x <= 0 || head_x >= WIDTH - 1 ||
        head_y <= 0 || head_y >= HEIGHT - 1) {
        return 1;
    }

    return 0;
}

/* Move the snake one step forward */
static void move_snake(void) {
    dir = next_dir;

    int new_head_x = snake[0].x;
    int new_head_y = snake[0].y;

    switch (dir) {
        case DIR_UP:    new_head_y--; break;
        case DIR_DOWN:  new_head_y++; break;
        case DIR_LEFT:  new_head_x--; break;
        case DIR_RIGHT: new_head_x++; break;
    }

    /* Remember current tail position before shifting, so we can clear it later */
    prev_tail_x = snake[snake_len - 1].x;
    prev_tail_y = snake[snake_len - 1].y;

    /* Collision detection */
    if (check_collision(new_head_x, new_head_y)) {
        game_over = 1;
        return;
    }

    int ate = (new_head_x == food_x && new_head_y == food_y);

    /* Shift body */
    for (int i = snake_len - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0].x = new_head_x;
    snake[0].y = new_head_y;

    if (ate) {
        snake_len++;
        score += 10;
        spawn_food();
    }
}

/* Draw one frame */
static void draw_frame(void) {
    static int first_draw = 1;

    if (first_draw) {
        system("cls");

        /* Top border */
        gotoxy(0, 0);
        for (int x = 0; x < WIDTH; x++) putchar('#');
        /* Bottom border */
        gotoxy(0, HEIGHT - 1);
        for (int x = 0; x < WIDTH; x++) putchar('#');
        /* Left & Right borders */
        for (int y = 1; y < HEIGHT - 1; y++) {
            gotoxy(0, y);          putchar('#');
            gotoxy(WIDTH - 1, y);  putchar('#');
        }

        /* Info bar */
        gotoxy(0, HEIGHT);
        printf("=== Snake Game ===  WASD / Arrow Keys to move  |  ESC to quit");

        first_draw = 0;
    }

    /* Clear the old tail position (snake has already moved) */
    gotoxy(prev_tail_x, prev_tail_y);
    putchar(' ');

    /* Draw body (all segments except head) */
    for (int i = 1; i < snake_len; i++) {
        gotoxy(snake[i].x, snake[i].y);
        putchar('o');
    }

    /* Draw head */
    gotoxy(snake[0].x, snake[0].y);
    putchar('O');

    /* Draw food */
    gotoxy(food_x, food_y);
    putchar('@');

    /* Score */
    gotoxy(0, HEIGHT + 1);
    printf("Score: %d    ", score);
}

/* Show game over screen */
static void show_game_over(void) {
    gotoxy(WIDTH / 2 - 7, HEIGHT / 2 - 2);
    printf("==============");
    gotoxy(WIDTH / 2 - 7, HEIGHT / 2 - 1);
    printf("  GAME OVER!  ");
    gotoxy(WIDTH / 2 - 7, HEIGHT / 2);
    printf("  Final Score: %d ", score);
    gotoxy(WIDTH / 2 - 7, HEIGHT / 2 + 1);
    printf("==============");

    gotoxy(0, HEIGHT + 3);
    printf("Press any key to exit...");
    _getch();
}

/* ====== Entry Point ====== */
int main(void) {
    /* Set console output code page to UTF-8 for proper character display */
    SetConsoleOutputCP(CP_UTF8);

    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        printf("Failed to get console handle!\n");
        return 1;
    }

    /* Set console title */
    SetConsoleTitle(TEXT("Snake Game - C Classic"));

    /* Hide cursor */
    hide_cursor();

    /* Initialize game */
    init_game();

    /* Main game loop: ~10 FPS */
    while (!game_over) {
        handle_input();
        move_snake();
        draw_frame();
        Sleep(200);
    }

    /* Show game over screen */
    show_game_over();

    /* Restore cursor visibility */
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    return 0;
}
