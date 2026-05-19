#include "game.h"

char map[MAP_HEIGHT][MAP_WIDTH];
Player player;
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
int score = 0;
int game_over = 0;
int enemy_move_cooldown = 0; // 控制敌军移动速度

// 初始化游戏
void init_game() {
    srand((unsigned int)time(NULL));
    score = 0;
    game_over = 0;

    // 1. 初始化地图边界
    for (int i = 0; i < MAP_HEIGHT; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            if (i == 0 || i == MAP_HEIGHT - 1 || j == 0 || j == MAP_WIDTH - 1) {
                map[i][j] = '#';
            } else {
                map[i][j] = ' ';
            }
        }
    }

    // 2. 初始化玩家
    player.x = MAP_WIDTH / 2;
    player.y = MAP_HEIGHT - 3;
    player.direction = '^';

    // 3. 初始化子弹
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = 0;
    }

    // 4. 初始化敌军
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].x = 2 + (rand() % (MAP_WIDTH - 4));
        enemies[i].y = 2 + (rand() % 4); // 生成在上方
        enemies[i].direction = 'v';
        enemies[i].active = 1;
    }
}

// 玩家开火
void shoot_bullet() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = 1;
            bullets[i].direction = player.direction;
            // 根据坦克方向，子弹生成在坦克前方一格
            bullets[i].x = player.x;
            bullets[i].y = player.y;
            if (player.direction == '^') bullets[i].y--;
            if (player.direction == 'v') bullets[i].y++;
            if (player.direction == '<') bullets[i].x--;
            if (player.direction == '>') bullets[i].x++;
            break;
        }
    }
}

// 更新子弹逻辑
void update_bullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;

        // 子弹移动
        if (bullets[i].direction == '^') bullets[i].y--;
        else if (bullets[i].direction == 'v') bullets[i].y++;
        else if (bullets[i].direction == '<') bullets[i].x--;
        else if (bullets[i].direction == '>') bullets[i].x++;

        // 碰撞检测：打中墙壁
        if (map[bullets[i].y][bullets[i].x] == '#') {
            bullets[i].active = 0;
            continue;
        }

        // 碰撞检测：打中敌军
        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (enemies[j].active && bullets[i].x == enemies[j].x && bullets[i].y == enemies[j].y) {
                bullets[i].active = 0;
                enemies[j].active = 0;
                score += 100; // 得分
                // 重新生成该敌军
                enemies[j].x = 2 + (rand() % (MAP_WIDTH - 4));
                enemies[j].y = 2 + (rand() % 4);
                enemies[j].active = 1;
                break;
            }
        }
    }
}

// 敌军 AI 随机移动
void update_enemies() {
    enemy_move_cooldown++;
    if (enemy_move_cooldown < 3) return; // 让敌军移动得比玩家慢一点
    enemy_move_cooldown = 0;

    char dirs[] = {'w', 's', 'a', 'd'};

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;

        // 随机概率改变方向
        if (rand() % 5 == 0) {
            char rand_dir = dirs[rand() % 4];
            if (rand_dir == 'w') enemies[i].direction = '^';
            if (rand_dir == 's') enemies[i].direction = 'v';
            if (rand_dir == 'a') enemies[i].direction = '<';
            if (rand_dir == 'd') enemies[i].direction = '>';
        }

        // 尝试向前走
        int next_x = enemies[i].x;
        int next_y = enemies[i].y;
        if (enemies[i].direction == '^') next_y--;
        if (enemies[i].direction == 'v') next_y++;
        if (enemies[i].direction == '<') next_x--;
        if (enemies[i].direction == '>') next_x++;

        // 撞墙检测
        if (map[next_y][next_x] != '#') {
            enemies[i].x = next_x;
            enemies[i].y = next_y;
        }

        // 撞击玩家检测
        if (enemies[i].x == player.x && enemies[i].y == player.y) {
            game_over = 1;
        }
    }
}

// 更新玩家状态
void update_game(char input) {
    if (input == 'w' || input == 'W') {
        player.direction = '^';
        if (map[player.y - 1][player.x] != '#') player.y--;
    } else if (input == 's' || input == 'S') {
        player.direction = 'v';
        if (map[player.y + 1][player.x] != '#') player.y++;
    } else if (input == 'a' || input == 'A') {
        player.direction = '<';
        if (map[player.y][player.x - 1] != '#') player.x--;
    } else if (input == 'd' || input == 'D') {
        player.direction = '>';
        if (map[player.y][player.x + 1] != '#') player.x++;
    } else if (input == ' ') { // 空格键开火
        shoot_bullet();
    }
}

// 绘制游戏画面
void draw_map() {
    COORD coord = {0, 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);

    printf("========= 终端坦克大战 =========   得分: %d\n", score);

    for (int i = 0; i < MAP_HEIGHT; i++) {
        for (int j = 0; j < MAP_WIDTH; j++) {
            // 1. 画玩家
            if (i == player.y && j == player.x) {
                printf("%c", player.direction);
                continue;
            }

            // 2. 画敌军
            int is_enemy = 0;
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (enemies[e].active && i == enemies[e].y && j == enemies[e].x) {
                    printf("E"); // Enemy
                    is_enemy = 1;
                    break;
                }
            }
            if (is_enemy) continue;

            // 3. 画子弹
            int is_bullet = 0;
            for (int b = 0; b < MAX_BULLETS; b++) {
                if (bullets[b].active && i == bullets[b].y && j == bullets[b].x) {
                    printf("*");
                    is_bullet = 1;
                    break;
                }
            }
            if (is_bullet) continue;

            // 4. 画地图原生物（墙壁或空地）
            printf("%c", map[i][j]);
        }
        printf("\n");
    }
    printf("操作说明: WASD移动，空格开火，Q退出\n");
}

char get_input() {
    if (_kbhit()) return _getch();
    return '\0';
}

void game_loop() {
    char input;
    while (!game_over) {
        input = get_input();
        if (input == 'q' || input == 'Q') break;

        update_game(input);
        update_bullets();
        update_enemies();
        draw_map();

        Sleep(30); // 丝滑帧率
    }

    if (game_over) {
        system("cls");
        printf("\n\n      GAME OVER! 游戏结束！\n");
        printf("      您的最终得分是: %d 分\n\n", score);
    }
}