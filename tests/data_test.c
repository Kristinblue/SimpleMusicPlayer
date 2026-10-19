#include "data.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static const char *song_file = "songs.txt";         /* 测试目录中的临时歌单。 */
static const char *favorite_file = "favorites.txt"; /* 测试目录中的临时收藏文件。 */
static const char *sample =                       /* 不连续编号及中文字段。 */
    "20|晴天|歌手甲|夏日|music/20.wav\n"
    "3|夜空|歌手乙|夜航|music/03.wav\n";

/* 将 text 按字节写入 path；无返回值，任何文件错误使测试失败。 */
static void write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb"); /* 当前测试输入文件。 */
    assert(file);
    assert(fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(fclose(file) == 0);
}

/* 验证 path 的内容等于 text；无返回值，不一致时断言失败。 */
static void expect_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "rb"); /* 待检查的输出文件。 */
    char actual[1024] = {0};        /* 保存实际内容，并保留结尾零字符。 */
    assert(file);
    assert(fread(actual, 1, sizeof actual - 1, file) < sizeof actual - 1);
    assert(!ferror(file));
    assert(fclose(file) == 0);
    /* 文本写出在 Windows 上使用 CRLF，比较前统一为 LF。 */
    for (char *p = actual; *p; ++p) { /* 当前待检查的输出字节。 */
        if (*p == '\r') memmove(p, p + 1, strlen(p));
    }
    assert(strcmp(actual, text) == 0);
}

/* 验证八个公开接口、复制语义、幂等收藏及保存重载；无参数和返回值。 */
static void test_operations(void)
{
    Song out[MAX_SONGS]; /* 调用者的输出数组，可安全独立修改。 */
    Song song;           /* 单曲查询输出。 */

    assert(data_get_songs(NULL, 0) == 0);
    assert(data_get_songs(NULL, -1) == -1);
    write_text(song_file, sample);
    remove(favorite_file);
    assert(data_load(song_file, favorite_file) == 0);
    assert(data_get_songs(out, MAX_SONGS) == 2);
    assert(out[0].id == 20 && out[1].id == 3);
    assert(strcmp(out[0].title, "晴天") == 0);
    assert(strcmp(out[1].path, "music/03.wav") == 0);
    out[0].id = -10;
    assert(data_find_song(20, &song) == 0 && song.id == 20);
    assert(data_find_song(999, &song) == -1);
    assert(data_find_song(20, NULL) == -1);
    out[0].id = -123;
    assert(data_get_songs(out, 1) == -1 && out[0].id == -123);
    assert(data_get_songs(NULL, 2) == -1);
    assert(data_get_favorites(NULL, 0) == 0);
    assert(data_get_favorites(NULL, -1) == -1);
    assert(data_is_favorite(20) == 0);
    assert(data_is_favorite(999) == -1);
    assert(data_add_favorite(999) == -1);
    assert(data_remove_favorite(999) == -1);
    assert(data_add_favorite(3) == 0);
    assert(data_add_favorite(3) == 0);
    assert(data_add_favorite(20) == 0);
    assert(data_is_favorite(3) == 1);
    assert(data_get_favorites(out, 1) == -1 && out[0].id == -123);
    assert(data_get_favorites(NULL, 2) == -1);
    assert(data_get_favorites(out, MAX_SONGS) == 2);
    assert(out[0].id == 20 && out[1].id == 3); /* 必须按歌单顺序，而非添加顺序。 */
    assert(data_remove_favorite(20) == 0);
    assert(data_remove_favorite(20) == 0);
    assert(data_save_favorites(favorite_file) == 0);
    expect_text(favorite_file, "3\n");
    assert(data_load(song_file, favorite_file) == 0);
    assert(data_get_favorites(out, MAX_SONGS) == 1 && out[0].id == 3);
    write_text(favorite_file, "3\r\n3\r\n999\r\n0\r\n-2\r\n20\r\n");
    assert(data_load(song_file, favorite_file) == 0);
    assert(data_get_favorites(out, MAX_SONGS) == 2 && out[0].id == 20);
    assert(data_save_favorites(favorite_file) == 0); /* 验证覆盖已有文件。 */
    expect_text(favorite_file, "20\n3\n");
    assert(data_remove_favorite(20) == 0 && data_remove_favorite(3) == 0);
    assert(data_save_favorites(favorite_file) == 0);
    expect_text(favorite_file, "");
}

/* 验证损坏输入被拒绝且保留旧数据、BOM、CRLF 和容量边界；无参数和返回值。 */
static void test_input(void)
{
    const char *bad[] = { /* 各种损坏的歌单记录。 */
        "1|a|b|c\n", "1|a|b|c|d|e\n", "1||b|c|d\n",
        "0|a|b|c|d\n", "-1|a|b|c|d\n", "2147483648|a|b|c|d\n",
        "1x|a|b|c|d\n", "1|a|b|c|d\n1|e|f|g|h\n",
        "1|a\rb|c|d|e\n"
    };
    Song out[MAX_SONGS]; /* 查询失败后用于确认旧歌单没有变化。 */
    FILE *file;          /* 构造边界和二进制损坏输入的文件。 */

    write_text(song_file, sample);
    write_text(favorite_file, "3\n");
    assert(data_load(song_file, favorite_file) == 0);
    assert(data_load(NULL, favorite_file) == -1);
    assert(data_load(song_file, NULL) == -1);
    assert(data_load("", favorite_file) == -1);
    assert(data_load(song_file, "") == -1);
    assert(data_load("missing/songs.txt", favorite_file) == -1);
    for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i) { /* 损坏样例下标。 */
        write_text(song_file, bad[i]);
        assert(data_load(song_file, favorite_file) == -1);
        assert(data_get_songs(out, MAX_SONGS) == 2 && out[0].id == 20);
        assert(data_is_favorite(3) == 1);
    }
    write_text(song_file, sample);
    write_text(favorite_file, "3\ninvalid\n");
    assert(data_load(song_file, favorite_file) == -1 && data_is_favorite(3) == 1);
    write_text(favorite_file, "99999999999999999999999999999999999999\n");
    assert(data_load(song_file, favorite_file) == -1);
    write_text(song_file, "\xef\xbb\xbf" "20|晴天|歌手|专辑|music/20.wav\r\n\r\n");
    write_text(favorite_file, "\xef\xbb\xbf" "20\r\n");
    assert(data_load(song_file, favorite_file) == 0 && data_is_favorite(20) == 1);

    file = fopen(song_file, "wb");
    assert(file);
    assert(fwrite("1|a|b|c|d\0junk", 1, 14, file) == 14);
    assert(fclose(file) == 0);
    assert(data_load(song_file, favorite_file) == -1);

    /* 正好填满文本数组时必须有结尾零字符；再增加一个字节则拒绝。 */
    for (int extra = 0; extra <= 1; ++extra) { /* 是否超过歌名字段容量。 */
        file = fopen(song_file, "wb");
        assert(file);
        fputs("1|", file);
        for (int i = 0; i < 127 + extra; ++i) fputc('a', file); /* 歌名字节下标。 */
        fputs("|b|c|", file);
        for (int i = 0; i < 511; ++i) fputc('d', file); /* 路径字节下标。 */
        assert(fclose(file) == 0);
        assert(data_load(song_file, favorite_file) == (extra ? -1 : 0));
        if (!extra) {
            assert(data_find_song(1, out) == 0);
            assert(strlen(out[0].title) == 127 && strlen(out[0].path) == 511);
        }
    }
    file = fopen(song_file, "wb");
    assert(file);
    for (int i = 1; i <= MAX_SONGS; ++i) /* 写满歌单容量的歌曲编号。 */
        assert(fprintf(file, "%d|a|b|c|d\n", i) > 0);
    assert(fclose(file) == 0);
    assert(data_load(song_file, favorite_file) == 0);
    assert(data_get_songs(out, MAX_SONGS) == MAX_SONGS);
    file = fopen(song_file, "ab");
    assert(file);
    assert(fprintf(file, "%d|a|b|c|d\n", MAX_SONGS + 1) > 0);
    assert(fclose(file) == 0);
    assert(data_load(song_file, favorite_file) == -1);
    assert(data_get_songs(out, MAX_SONGS) == MAX_SONGS);
    file = fopen(song_file, "wb");
    assert(file);
    for (int i = 0; i < 2000; ++i) fputc('a', file); /* 超长记录的字节下标。 */
    assert(fclose(file) == 0);
    assert(data_load(song_file, favorite_file) == -1);
    write_text(song_file, "");
    assert(data_load(song_file, favorite_file) == 0);
    assert(data_get_songs(NULL, 0) == 0 && data_get_favorites(NULL, 0) == 0);
}

/* 验证保存失败不破坏旧收藏文件；无参数和返回值。 */
static void test_save_failure(void)
{
    write_text(song_file, sample);
    write_text(favorite_file, "3\n");
    assert(data_load(song_file, favorite_file) == 0);
    assert(data_add_favorite(20) == 0);
    assert(data_save_favorites(NULL) == -1);
    assert(data_save_favorites("") == -1);
    assert(data_save_favorites("missing/favorites.txt") == -1);
    write_text("favorites.txt.tmp", "keep temporary file");
    assert(data_save_favorites(favorite_file) == -1);
    expect_text(favorite_file, "3\n");
    expect_text("favorites.txt.tmp", "keep temporary file");
    assert(remove("favorites.txt.tmp") == 0);
#ifdef _WIN32
    {
        HANDLE locked = CreateFileA(favorite_file, GENERIC_READ, FILE_SHARE_READ,
                                   NULL, OPEN_EXISTING, 0, NULL); /* 禁止替换的旧文件。 */
        FILE *temporary; /* 检查替换失败后是否清理临时文件。 */
        assert(locked != INVALID_HANDLE_VALUE);
        assert(data_save_favorites(favorite_file) == -1);
        assert(CloseHandle(locked));
        expect_text(favorite_file, "3\n");
        temporary = fopen("favorites.txt.tmp", "rb");
        assert(!temporary);
    }
#endif
    assert(data_save_favorites(favorite_file) == 0);
    expect_text(favorite_file, "20\n3\n");
}

/* 独立测试入口：在专用临时目录运行；成功返回 0，断言失败则终止。 */
int main(void)
{
    test_operations();
    test_input();
    test_save_failure();
    assert(remove(song_file) == 0);
    assert(remove(favorite_file) == 0);
    puts("data module tests passed");
    return 0;
}
