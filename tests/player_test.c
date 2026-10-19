#include "player.h"

#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void print_status(void)
{
    const char *state = "stopped";
    if (player_get_state() == PLAYER_PLAYING) {
        state = "playing";
    } else if (player_get_state() == PLAYER_PAUSED) {
        state = "paused";
    }
    printf("State: %s, song ID: %d, position: %d / %d ms\n", state,
           player_get_current_id(), player_get_position(), player_get_duration());
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAILED at line %d: %s\n", __LINE__, #condition); \
        goto failed; \
    } \
} while (0)

static int wait_for_position(int minimum_ms)
{
    ULONGLONG deadline = GetTickCount64() + 4000;
    do {
        int position = player_get_position();
        if (position < 0) return -1;
        if (position >= minimum_ms) return 0;
        Sleep(50);
    } while (GetTickCount64() < deadline);
    fputs("Playback position did not advance in time.\n", stderr);
    print_status();
    return -1;
}

static int wait_for_song(int expected_id, PlayerState expected_state)
{
    ULONGLONG deadline = GetTickCount64() + 4000;
    do {
        if (player_update() != 0) {
            return -1;
        }
        if (player_get_current_id() == expected_id &&
            player_get_state() == expected_state &&
            (expected_state == PLAYER_STOPPED || player_get_position() < 1000)) {
            return 0;
        }
        Sleep(50);
    } while (GetTickCount64() < deadline);
    fputs("Timed out waiting for the expected song end transition.\n", stderr);
    print_status();
    return -1;
}

/* 将实际歌曲定位到末尾附近，测试自然结束，不等待整首歌。 */
static int check_song_end(int expected_id, PlayerState expected_state)
{
    int duration = player_get_duration();
    if (duration < 1000 || player_seek(duration - 300) != 0) {
        return -1;
    }
    return wait_for_song(expected_id, expected_state);
}

static int test_remaining_functions(void)
{
    /* 故意使用不连续编号，验证切歌按歌单顺序而非编号加减。 */
    Song songs[] = {
        {11, "普通朋友", "NUFU2R", "", "musiclist/普通朋友.wav"},
        {42, "姐妹", "NUFU2R", "", "musiclist/sisters.wav"},
        {105, "London Town", "", "", "musiclist/piano.wav"}
    };
    Song broken = {7, "Missing", "", "", "musiclist/does-not-exist.wav"};
    int duration;
    int position;

    CHECK(player_next() == -1);
    CHECK(player_previous() == -1);
    CHECK(player_seek(0) == -1);
    CHECK(player_update() == 0);
    CHECK(player_set_mode((PlayerMode)-1) == -1);
    CHECK(player_set_mode((PlayerMode)3) == -1);
    CHECK(player_set_volume(-1) == -1);
    CHECK(player_set_volume(101) == -1);
    CHECK(player_set_volume(0) == 0); /* 后续自动检查静音进行。 */
    CHECK(player_set_playlist(songs, 3) == 0);
    CHECK(player_next() == -1); /* 尚未选择歌曲。 */
    CHECK(player_play(11) == 0);
    CHECK(player_pause() == 0);
    CHECK(player_previous() == -1);
    CHECK(player_get_current_id() == 11);
    CHECK(player_get_state() == PLAYER_PAUSED);
    CHECK(player_next() == 42);
    CHECK(player_get_state() == PLAYER_PLAYING);
    CHECK(player_next() == 105);
    CHECK(player_pause() == 0);
    CHECK(player_next() == -1);
    CHECK(player_get_current_id() == 105);
    CHECK(player_get_state() == PLAYER_PAUSED);
    CHECK(player_previous() == 42);

    for (int mode = PLAYER_LIST_LOOP; mode <= PLAYER_SINGLE_LOOP; ++mode) {
        CHECK(player_set_mode((PlayerMode)mode) == 0);
        CHECK(player_play(11) == 0);
        CHECK(player_previous() == 105);
        CHECK(player_next() == 11); /* 单曲循环的手动切歌也可首尾相接。 */
    }
    puts("PASS: manual previous/next, sparse IDs and mode boundaries.");

    CHECK(player_set_mode(PLAYER_ORDER) == 0);
    duration = player_get_duration();
    CHECK(player_pause() == 0);
    position = player_get_position();
    CHECK(player_seek(-1) == -1);
    CHECK(player_seek(duration + 1) == -1);
    CHECK(player_get_position() == position);
    CHECK(player_seek(3000) == 0);
    CHECK(player_get_state() == PLAYER_PAUSED);
    Sleep(250);
    CHECK(abs(player_get_position() - 3000) <= 80);
    CHECK(player_update() == 0);
    CHECK(player_get_current_id() == 11);
    CHECK(player_resume() == 0);
    CHECK(player_seek(5000) == 0);
    CHECK(player_get_state() == PLAYER_PLAYING);
    CHECK(wait_for_position(5101) == 0);
    CHECK(player_stop() == 0);
    CHECK(player_seek(2000) == 0);
    CHECK(player_get_state() == PLAYER_STOPPED);
    Sleep(250);
    CHECK(abs(player_get_position() - 2000) <= 80);
    CHECK(player_set_volume(100) == 0);
    CHECK(player_set_volume(0) == 0);
    CHECK(player_play(42) == 0); /* 切歌时会重新应用保存的音量。 */
    puts("PASS: seek while playing/paused/stopped, ranges and volume commands.");

    CHECK(player_play(11) == 0);
    CHECK(check_song_end(42, PLAYER_PLAYING) == 0);
    CHECK(player_play(105) == 0);
    CHECK(check_song_end(105, PLAYER_STOPPED) == 0);
    CHECK(player_update() == 0);
    CHECK(player_get_current_id() == 105);
    CHECK(player_get_state() == PLAYER_STOPPED);
    puts("PASS: ordered automatic next and stop after the last song.");

    CHECK(player_set_mode(PLAYER_LIST_LOOP) == 0);
    CHECK(player_play(105) == 0);
    CHECK(check_song_end(11, PLAYER_PLAYING) == 0);
    CHECK(player_set_mode(PLAYER_SINGLE_LOOP) == 0);
    CHECK(player_play(42) == 0);
    CHECK(check_song_end(42, PLAYER_PLAYING) == 0);
    puts("PASS: automatic list wrap and single-song repeat.");

    CHECK(player_set_mode(PLAYER_ORDER) == 0);
    CHECK(player_play(11) == 0);
    CHECK(player_pause() == 0);
    CHECK(player_seek(player_get_duration()) == 0);
    CHECK(player_update() == 0);
    CHECK(player_get_state() == PLAYER_PAUSED);
    CHECK(player_get_current_id() == 11);
    CHECK(player_resume() == 0);
    CHECK(wait_for_song(42, PLAYER_PLAYING) == 0);
    CHECK(player_seek(player_get_duration()) == 0);
    CHECK(wait_for_song(105, PLAYER_PLAYING) == 0);
    CHECK(player_stop() == 0);
    CHECK(player_update() == 0);
    CHECK(player_get_state() == PLAYER_STOPPED);

    CHECK(player_set_playlist(&songs[0], 1) == 0);
    CHECK(player_set_mode(PLAYER_LIST_LOOP) == 0);
    CHECK(player_play(11) == 0);
    CHECK(player_next() == 11);
    CHECK(check_song_end(11, PLAYER_PLAYING) == 0);
    CHECK(player_set_mode(PLAYER_SINGLE_LOOP) == 0);
    CHECK(check_song_end(11, PLAYER_PLAYING) == 0);
    puts("PASS: seek to end, manual stop and one-song playlists.");

    songs[1].id = songs[0].id;
    CHECK(player_set_playlist(songs, 3) == -1);
    CHECK(player_get_current_id() == 11);
    CHECK(player_get_state() == PLAYER_PLAYING);
    CHECK(player_set_playlist(&broken, 1) == 0);
    CHECK(player_get_current_id() == -1);
    CHECK(player_get_state() == PLAYER_STOPPED);
    CHECK(player_play(7) == -1);
    CHECK(player_get_current_id() == -1);
    CHECK(player_get_duration() == 0);
    CHECK(player_set_playlist(NULL, 0) == 0);
    CHECK(player_next() == -1);
    CHECK(player_previous() == -1);
    CHECK(player_seek(0) == -1);
    CHECK(player_update() == 0);
    puts("PASS: invalid playlists, missing files and clearing during playback.");
    return 0;

failed:
    return -1;
}

static int interactive_test(void)
{
    Song songs[] = {
        {11, "普通朋友", "NUFU2R", "", "musiclist/普通朋友.wav"},
        {42, "姐妹", "NUFU2R", "", "musiclist/sisters.wav"},
        {105, "London Town", "", "", "musiclist/piano.wav"}
    };
    int volume = 70;
    if (player_set_playlist(songs, 3) != 0 || player_play(11) != 0) {
        fputs("Cannot load test songs. Run from D:/myplayer.\n", stderr);
        return -1;
    }
    puts("Press a key (Enter is not required):");
    puts("p=pause r=resume s=stop b=restart a=previous n=next q=quit");
    puts("o=ordered l=list loop i=single loop -=quieter +=louder");
    puts("[=back 10 seconds ]=forward 10 seconds ?=status");
    print_status();
    for (;;) {
        int old_id = player_get_current_id();
        PlayerState old_state = player_get_state();
        if (player_update() != 0) {
            puts("Automatic playback update failed.");
        }
        if (old_id != player_get_current_id() || old_state != player_get_state()) {
            print_status();
        }
        if (_kbhit()) {
            int result = 0;
            int key = _getch();
            if (key == 'q') {
                break;
            }
            switch (key) {
            case 'p': result = player_pause(); break;
            case 'r': result = player_resume(); break;
            case 's': result = player_stop(); break;
            case 'b': result = player_play(player_get_current_id()); break;
            case 'a': result = player_previous(); break;
            case 'n': result = player_next(); break;
            case 'o': result = player_set_mode(PLAYER_ORDER); break;
            case 'l': result = player_set_mode(PLAYER_LIST_LOOP); break;
            case 'i': result = player_set_mode(PLAYER_SINGLE_LOOP); break;
            case '-':
            case '+': {
                int value = volume + (key == '+' ? 10 : -10);
                if (value < 0) value = 0;
                if (value > 100) value = 100;
                result = player_set_volume(value);
                if (result == 0) volume = value;
                printf("Volume: %d\n", volume);
                break;
            }
            case '[':
            case ']': {
                int position = player_get_position();
                int duration = player_get_duration();
                if (position < 0 || duration < 0) {
                    result = -1;
                    break;
                }
                position += key == ']' ? 10000 : -10000;
                if (position < 0) position = 0;
                if (position > duration) position = duration;
                result = player_seek(position);
                break;
            }
            case '?': break;
            default: continue;
            }
            printf("Result: %s\n", result >= 0 ? "OK" : "failed");
            print_status();
        }
        Sleep(50); /* 模拟界面的定时调用；等待不在播放模块里。 */
    }
    return 0;
}

/* 在项目根目录运行；测试 main 不放进 src，避免与 Qt 入口冲突。 */
int main(int argc, char **argv)
{
    Song song = {1, "Test song", "", "", "musiclist/普通朋友.wav"};
    int smoke_test = argc > 1 && strcmp(argv[1], "--smoke") == 0;

    CHECK(player_play(1) == -1);
    CHECK(player_pause() == -1);
    CHECK(player_resume() == -1);
    CHECK(player_stop() == -1);
    CHECK(player_next() == -1);
    CHECK(player_previous() == -1);
    CHECK(player_seek(0) == -1);
    CHECK(player_set_volume(70) == -1);
    CHECK(player_set_mode(PLAYER_ORDER) == -1);
    CHECK(player_update() == -1);
    CHECK(player_init() == 0);
    CHECK(player_get_state() == PLAYER_STOPPED);
    CHECK(player_get_current_id() == -1);
    CHECK(player_get_position() == 0);
    CHECK(player_get_duration() == 0);
    CHECK(player_pause() == -1);
    CHECK(player_resume() == -1);
    CHECK(player_stop() == -1);
    CHECK(player_set_playlist(NULL, 1) == -1);
    CHECK(player_set_playlist(&song, MAX_SONGS + 1) == -1);
    CHECK(player_set_playlist(&song, 1) == 0);
    CHECK(player_play(999) == -1);

    /* 修改调用者的数据，验证模块确实保存了自己的歌单副本。 */
    song.id = 2;
    song.path[0] = '\0';
    CHECK(player_play(1) == 0);
    CHECK(player_get_state() == PLAYER_PLAYING);
    CHECK(player_get_current_id() == 1);
    CHECK(player_get_duration() > 5000);

    if (smoke_test) {
        int paused_position;
        int resumed_position;
        Sleep(800); /* 等待只发生在测试入口，不在播放模块中。 */
        CHECK(player_pause() == 0);
        CHECK(player_get_state() == PLAYER_PAUSED);
        paused_position = player_get_position();
        CHECK(paused_position >= 200);
        CHECK(player_pause() == 0);
        CHECK(player_play(999) == -1);
        Sleep(400);
        CHECK(abs(player_get_position() - paused_position) <= 80);
        print_status();

        CHECK(player_resume() == 0);
        CHECK(player_get_state() == PLAYER_PLAYING);
        CHECK(player_resume() == 0);
        CHECK(wait_for_position(paused_position + 100) == 0);
        resumed_position = player_get_position();
        CHECK(resumed_position >= paused_position + 100);
        print_status();

        CHECK(player_stop() == 0);
        CHECK(player_get_state() == PLAYER_STOPPED);
        CHECK(player_get_current_id() == 1);
        CHECK(player_get_position() == 0);
        CHECK(player_resume() == -1);
        CHECK(player_pause() == -1);
        CHECK(player_stop() == 0);
        Sleep(250);
        CHECK(player_get_position() == 0);
        print_status();

        CHECK(player_play(1) == 0);
        CHECK(player_get_position() < 500);
        CHECK(player_pause() == 0);
        CHECK(player_stop() == 0); /* 暂停时也能停止并归零。 */
        CHECK(player_get_position() == 0);
        CHECK(player_set_playlist(NULL, 0) == 0);
        CHECK(player_play(1) == -1);
        CHECK(test_remaining_functions() == 0);
    } else {
        CHECK(interactive_test() == 0);
    }

    player_close();
    player_close();
    CHECK(player_get_current_id() == -1);
    CHECK(player_get_position() == 0);
    CHECK(player_get_state() == PLAYER_STOPPED);
    puts("Resources released. Test passed.");
    return 0;

failed:
    player_close();
    return 1;
}
