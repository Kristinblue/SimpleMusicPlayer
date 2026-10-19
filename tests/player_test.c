#include "player.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

/* 在项目根目录运行；测试 main 不放进 src，避免与 Qt 入口冲突。 */
int main(int argc, char **argv)
{
    Song song = {1, "Test song", "", "", "musiclist/testmusic.wav"};
    int smoke_test = argc > 1 && strcmp(argv[1], "--smoke") == 0;

    if (player_play(1) != -1 || player_init() != 0 ||
        player_set_playlist(NULL, 1) != -1 ||
        player_set_playlist(&song, MAX_SONGS + 1) != -1 ||
        player_set_playlist(&song, 1) != 0 ||
        player_play(999) != -1) {
        fputs("Initialization or parameter checks failed.\n", stderr);
        player_close();
        return 1;
    }

    /* 修改调用者的数据，验证模块确实保存了自己的歌单副本。 */
    song.id = 2;
    song.path[0] = '\0';
    if (player_play(1) != 0) {
        fputs("Playback failed. Run from D:/myplayer and check the WAV file.\n", stderr);
        player_close();
        return 1;
    }

    puts("Playback started. Press Enter to close the audio.");
    fflush(stdout);
    if (smoke_test) {
        Sleep(3000); /* 只在测试入口等待，播放模块自身不等待。 */
        if (player_play(1) != 0 || player_set_playlist(NULL, 0) != 0 ||
            player_play(1) != -1) {
            fputs("Replay or playlist clearing failed.\n", stderr);
            player_close();
            return 1;
        }
    } else {
        (void)getchar();
    }

    player_close();
    player_close();
    puts("Resources released. Test passed.");
    return 0;
}
