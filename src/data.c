#include "data.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#endif

static Song songs[MAX_SONGS];              /* 当前歌单，按文件中的顺序保存。 */
static unsigned char favorites[MAX_SONGS]; /* 与歌曲下标对应的收藏标记。 */
static int song_count;                     /* 当前有效歌曲数。 */

/* 在 list 的 count 首歌曲中查找 id；返回下标，未找到返回 -1。 */
static int find_index(const Song *list, int count, int id)
{
    for (int i = 0; i < count; ++i) { /* 当前检查的歌曲下标。 */
        if (list[i].id == id) {
            return i;
        }
    }

    return -1;
}

/* 从 file 读取一行到容量为 size 的 line；返回 1、0、-1 表示成功、结束、错误。 */
static int read_line(FILE *file, char *line, size_t size)
{
    size_t length = 0;           /* 已读取的字节数，不含结尾零字符。 */
    int ch;                      /* 当前输入字节或 EOF。 */
    int first = ftell(file) == 0; /* 只允许文件开头出现 UTF-8 BOM。 */

    while ((ch = fgetc(file)) != EOF && ch != '\n') {
        if (ch == '\0' || length + 1 >= size) {
            return -1;
        }

        line[length++] = (char)ch;
    }

    if (ferror(file)) {
        return -1;
    }
    if (ch == EOF && length == 0) {
        return 0;
    }

    /* 统一处理 CRLF，并保留 C 字符串的结尾零字符。 */
    if (length > 0 && line[length - 1] == '\r') {
        --length;
    }
    line[length] = '\0';

    if (first && length >= 3 && memcmp(line, "\xef\xbb\xbf", 3) == 0) {
        memmove(line, line + 3, length - 2);
    }

    return strchr(line, '\r') ? -1 : 1;
}

/* 把十进制 text 转为 int 并写入 id；完整且未溢出返回 0，否则返回 -1。 */
static int parse_id(const char *text, int *id)
{
    char *end;     /* 数值转换结束的位置，用于拒绝编号后面的杂字符。 */
    long value;    /* strtol 的结果，在转换为 int 前检查范围。 */

    errno = 0;
    value = strtol(text, &end, 10);
    if (text == end || *end != '\0' || errno == ERANGE ||
        value < INT_MIN || value > INT_MAX) {
        return -1;
    }

    *id = (int)value;
    return 0;
}

/* 拆分可修改的 line 并写入 song；要求五个非空字段，成功返回 0，错误返回 -1。 */
static int parse_song(char *line, Song *song)
{
    char *fields[5] = {line}; /* 编号、歌名、歌手、专辑和 WAV 路径的起始位置。 */
    char *outputs[] = {       /* 四个文本字段的目标数组。 */
        song->title, song->artist, song->album, song->path
    };
    const size_t sizes[] = {  /* 四个目标数组的容量，包含结尾零字符。 */
        sizeof song->title, sizeof song->artist,
        sizeof song->album, sizeof song->path
    };

    for (int i = 1; i < 5; ++i) { /* 下一个待拆分字段的下标。 */
        char *separator = strchr(fields[i - 1], '|'); /* 当前字段后的分隔符。 */
        if (!separator) {
            return -1;
        }

        *separator = '\0';
        fields[i] = separator + 1;
    }

    if (strchr(fields[4], '|') || parse_id(fields[0], &song->id) != 0 || song->id <= 0) {
        return -1;
    }

    for (int i = 0; i < 4; ++i) { /* 待复制的文本字段下标。 */
        if (fields[i + 1][0] == '\0' || strlen(fields[i + 1]) >= sizes[i]) {
            return -1;
        }

        strcpy(outputs[i], fields[i + 1]);
    }
    return 0;
}

/* 读取 path 到 list/count；损坏、重复或超过容量返回 -1，成功返回 0。 */
static int load_songs(const char *path, Song *list, int *count)
{
    FILE *file = fopen(path, "rb"); /* 歌单输入文件，按字节保留 UTF-8 内容。 */
    char line[sizeof(Song) + 32];  /* 容纳整条记录、编号、分隔符和 BOM。 */
    int status = 0;                /* read_line 的结果；-1 表示读取或解析失败。 */

    if (!file) {
        return -1;
    }

    while ((status = read_line(file, line, sizeof line)) > 0) {
        if (line[0] == '\0') {
            continue;
        }

        if (*count == MAX_SONGS || parse_song(line, &list[*count]) != 0 ||
            find_index(list, *count, list[*count].id) >= 0) {
            status = -1;
            break;
        }
        ++*count;
    }

    if (fclose(file) != 0) {
        status = -1;
    }

    return status < 0 ? -1 : 0;
}

/* 从 path 读取收藏到 flags，按 list/count 匹配；缺文件视为空收藏，其他错误返回 -1。 */
static int load_favorites(const char *path, const Song *list, int count,
                         unsigned char *flags)
{
    FILE *file = fopen(path, "rb"); /* 收藏输入文件。 */
    char line[64];                 /* 单个十进制编号及 BOM、换行的缓冲区。 */
    int status = 0;                /* 当前行的读取或解析结果。 */

    if (!file) {
        return errno == ENOENT ? 0 : -1;
    }

    while ((status = read_line(file, line, sizeof line)) > 0) {
        int id;    /* 当前收藏记录的歌曲编号。 */
        int index; /* 编号在歌单中的下标；不存在为 -1。 */
        if (line[0] == '\0') {
            continue;
        }

        if (parse_id(line, &id) != 0) {
            status = -1;
            break;
        }
        index = find_index(list, count, id);
        if (index >= 0) {
            flags[index] = 1; /* 自动去重，忽略无效编号。 */
        }
    }

    if (fclose(file) != 0) {
        status = -1;
    }

    return status < 0 ? -1 : 0;
}

/* song_file/favorite_file 是输入文件名；全部读取成功返回 0，失败返回 -1 并保留旧数据。 */
int data_load(const char *song_file, const char *favorite_file)
{
    Song loaded[MAX_SONGS] = {0};         /* 暂存新歌单，避免失败时破坏当前歌单。 */
    unsigned char flags[MAX_SONGS] = {0}; /* 暂存新收藏标记。 */
    int count = 0;                        /* 新歌单的有效歌曲数。 */

    if (!song_file || !*song_file || !favorite_file || !*favorite_file) {
        return -1;
    }

    if (load_songs(song_file, loaded, &count) != 0 ||
        load_favorites(favorite_file, loaded, count, flags) != 0) {
        return -1;
    }

    /* 两个文件都读取成功后，再一次性替换当前数据。 */
    memcpy(songs, loaded, sizeof songs);
    memcpy(favorites, flags, sizeof favorites);
    song_count = count;
    return 0;
}

/* 将歌单复制到容量为 capacity 的 out；返回数量，容量不足或参数无效返回 -1。 */
int data_get_songs(Song *out, int capacity)
{
    if (capacity < song_count || (song_count > 0 && !out)) {
        return -1;
    }

    if (song_count > 0) {
        memcpy(out, songs, (size_t)song_count * sizeof *out);
    }

    return song_count;
}

/* 将 song_id 对应的歌曲复制到 out；找到返回 0，不存在或 out 为空返回 -1。 */
int data_find_song(int song_id, Song *out)
{
    int index = find_index(songs, song_count, song_id); /* 待查询歌曲的下标。 */

    if (index < 0 || !out) {
        return -1;
    }

    *out = songs[index];
    return 0;
}

/* 按歌单顺序复制收藏到容量为 capacity 的 out；返回数量，错误返回 -1 且不部分写入。 */
int data_get_favorites(Song *out, int capacity)
{
    int count = 0; /* 先统计需要的容量，再用于记录实际复制数量。 */
    for (int i = 0; i < song_count; ++i) { /* 当前统计的歌曲下标。 */
        count += favorites[i];
    }

    if (capacity < count || (count > 0 && !out)) {
        return -1;
    }

    count = 0;
    for (int i = 0; i < song_count; ++i) { /* 当前复制的歌曲下标。 */
        if (favorites[i]) {
            out[count] = songs[i];
            ++count;
        }
    }

    return count;
}

/* 查询 song_id：已收藏返回 1，未收藏返回 0，编号不存在返回 -1。 */
int data_is_favorite(int song_id)
{
    int index = find_index(songs, song_count, song_id); /* 待查询歌曲的下标。 */

    return index < 0 ? -1 : favorites[index];
}

/* 把 song_id 的收藏标记设为 value；成功返回 0，编号不存在返回 -1。 */
static int set_favorite(int song_id, unsigned char value)
{
    int index = find_index(songs, song_count, song_id); /* 待修改歌曲的下标。 */

    if (index < 0) {
        return -1;
    }

    favorites[index] = value;
    return 0;
}

/* 收藏 song_id，重复添加也返回 0；歌曲不存在返回 -1。 */
int data_add_favorite(int song_id)
{
    return set_favorite(song_id, 1);
}

/* 取消 song_id 的收藏，重复取消也返回 0；歌曲不存在返回 -1。 */
int data_remove_favorite(int song_id)
{
    return set_favorite(song_id, 0);
}

/* 独占创建 path 文件；返回输出流，创建失败返回 NULL，不覆盖已有文件。 */
static FILE *create_temporary_file(const char *path)
{
#ifdef _WIN32
    int descriptor = _open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_TEXT,
                           _S_IREAD | _S_IWRITE); /* 独占创建的临时文件描述符。 */
    FILE *file; /* 文件描述符对应的标准输出流。 */

    if (descriptor < 0) {
        return NULL;
    }

    file = _fdopen(descriptor, "w");
    if (!file) {
        _close(descriptor);
        remove(path);
    }

    return file;
#else
    return fopen(path, "wx");
#endif
}

/* 保存收藏编号到 favorite_file；成功返回 0，失败返回 -1，不先删除旧文件。 */
int data_save_favorites(const char *favorite_file)
{
    char *temporary; /* 与目标同目录的临时文件名，以便写完后替换。 */
    FILE *file;      /* 临时输出文件，使用独占创建避免覆盖已有临时文件。 */
    int result = 0;  /* 写入、关闭和替换过程的最终结果。 */

    if (!favorite_file || !*favorite_file) {
        return -1;
    }

    temporary = malloc(strlen(favorite_file) + sizeof ".tmp");
    if (!temporary) {
        return -1;
    }

    strcpy(temporary, favorite_file);
    strcat(temporary, ".tmp");
    file = create_temporary_file(temporary);
    if (!file) {
        free(temporary);
        return -1;
    }

    for (int i = 0; i < song_count; ++i) { /* 按歌单顺序保存的歌曲下标。 */
        if (favorites[i] && fprintf(file, "%d\n", songs[i].id) < 0) {
            result = -1;
        }
    }

    /* 关闭时也会刷新缓冲区，失败时禁止替换旧文件。 */
    if (fclose(file) != 0) {
        result = -1;
    }

    if (result == 0) {
#ifdef _WIN32
        /* Windows 的 rename 不能覆盖旧文件，使用支持替换的文件移动操作。 */
        if (!MoveFileExA(temporary, favorite_file, MOVEFILE_REPLACE_EXISTING)) {
            result = -1;
        }
#else
        if (rename(temporary, favorite_file) != 0) {
            result = -1;
        }
#endif
    }

    if (result != 0) {
        remove(temporary);
    }
    free(temporary);

    return result;
}
