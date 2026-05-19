#ifndef GAME_H
#define GAME_H

#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <stdlib.h>
#include <time.h>

#define MAP_WIDTH 20
#define MAP_HEIGHT 20
#define MAX_BULLETS 10
#define MAX_ENEMIES 3

// 坦克结构体
typedef struct {
    int x;
    int y;
    char direction; // '^', 'v', '<', '>'
} Player;

// 子弹结构体
typedef struct {
    int x;
    int y;
    char direction;
    int active;     // 1为激活，0为消失
} Bullet;

// 敌军结构体
typedef struct {
    int x;
    int y;
    char direction;
    int active;
} Enemy;

// 全局变量共享声明
extern char map[MAP_HEIGHT][MAP_WIDTH];
extern Player player;
extern Bullet bullets[MAX_BULLETS];
extern Enemy enemies[MAX_ENEMIES];
extern int score;
extern int game_over;

// 函数声明
void init_game();
void draw_map();
void update_game(char input);
void shoot_bullet();
void update_bullets();
void update_enemies();
char get_input();
void game_loop();

#endif