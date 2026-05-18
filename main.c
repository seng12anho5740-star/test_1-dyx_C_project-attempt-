#include "game.h"

int main() {
    // 【核心修复】强制将 Windows 控制台的输出编码设置为 UTF-8 (65001)
    SetConsoleOutputCP(65001);

    // 隐藏终端的光标
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursor_info;
    GetConsoleCursorInfo(handle, &cursor_info);
    cursor_info.bVisible = FALSE;
    SetConsoleCursorInfo(handle, &cursor_info);

    system("cls");
    init_game();
    game_loop();

    // 游戏结束恢复光标显示
    cursor_info.bVisible = TRUE;
    SetConsoleCursorInfo(handle, &cursor_info);
    
    return 0;
}