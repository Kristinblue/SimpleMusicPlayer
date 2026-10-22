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

static Song songs[MAX_SONGS];              /* 歌曲列表，按文件顺序保存。 */
static unsigned char favorites[MAX_SONGS]; /* 对应歌曲的收藏标记。 */
static int song_count;                     /* 当前歌曲数量。 */

/*
 * 功能：在歌曲数组中按歌曲编号查找对应位置。
 * 参数：list 为待查歌曲数组；count 为数组中有效歌曲数量；id 为要查找的歌曲编号。
 * 返回：int；找到时返回歌曲在数组中的下标，未找到时返回 -1。
 */
static int find_index(const Song *list, int count, int id)
{
    for (int i = 0; i < count; ++i) { /* 只在有效歌曲范围内查找。 */
        if (list[i].id == id) { /* 编号匹配时进入返回分支。 */
            return i; /* 返回匹配歌曲的下标。 */
        }
    }

    return -1; /* 所有歌曲都不匹配时表示未找到。 */
}

/*
 * 功能：从文件流读取一行文本，移除行尾换行符，并处理首行 UTF-8 BOM。
 * 参数：file 为已打开的输入文件流；line 为接收文本的缓冲区；size 为缓冲区总字节数。
 * 返回：int；成功读到一行返回 1，正常到达文件末尾返回 0，读取错误或行过长返回 -1。
 */
static int read_line(FILE *file, char *line, size_t size)
{
    size_t length = 0;            /* 行长度，不含结尾零字符。 */
    int ch;                       /* 当前字符或 EOF。 */

    while ((ch = fgetc(file)) != EOF && ch != '\n') { /* 利用fgetc()逐字节读到换行或文件尾。 */ //文件读到末尾时，无法再向后会返回EOF，利用这个来判断有没有读完

        if (!ch || length + 1 >= size) { /* 拒绝嵌入零字节和超长行。 */
            return -1; /* 当前记录无法安全保存。 */
        }

        line[length++] = (char)ch; /* 保存字符并推进写入位置。 */
    }

    if (ch == EOF && !length) return 0; /* 没有读到任何内容即表示结束。 */

    if (length && line[length - 1] == '\r') --length; /* 去掉 CRLF 中的回车。 */
    line[length] = '\0'; /* 将读取字节变成以零结尾的 C 字符串。 */

    return strchr(line, '\r') ? -1 : 1; /* 其他位置的回车代表格式错误。 */
}

/*
 * 功能：将完整的十进制数字文本转换为 int，并拒绝空文本、尾随字符和越界值。
 * 参数：text 为待转换的零结尾字符串；id 为接收转换结果的输出指针。
 * 返回：int；成功转换返回 0 并写入 *id，格式错误或超出 int 范围返回 -1。
 */
static int parse_id(const char *text, int *id)
{
    char *end;       /* strtol 停止转换的位置。 */
    long value;      /* 暂存转换结果并检查范围。 */
    value = strtol(text, &end, 10); /* 按十进制读取文本整数。 */

    if (text == end || *end ) { /* 拒绝空文本、尾随字符 */
        return -1; /* 当前文本不是有效 int。 */
    }

    *id = (int)value; /* 确认范围安全后写入 int 输出参数。 */
    return 0; /* 表示整个文本都成功转换。 */
}

/*
 * 功能：把非空零结尾字符串完整复制到指定容量的目标字符数组。
 * 参数：to 为目标字符数组；size 为目标数组总字节数；from 为源字符串。
 * 返回：int；复制成功返回 0，源字符串为空或目标空间不足时返回 -1。
 */
static int copy_field(char *to, size_t size, const char *from)
{
    size_t length = strlen(from); /* UTF-8 字节长度。 */
    if (!length || length >= size) { /* 空字段或放不下时拒绝复制。 */
        return -1; /* 不写入不完整字段。 */
    }

    memcpy(to, from, length + 1); /* 连同结尾零字符一起复制。 */
    return 0; /* 表示字段复制完成。 */
}

/*
 * 功能：解析一条“编号|歌名|歌手|专辑|WAV路径”记录并填入歌曲结构体。
 * 参数：line 为可修改的记录字符串，函数会将分隔符改为字符串结束符；song 为接收结果的输出结构体。
 * 返回：int；字段格式、编号及所有字段均有效时返回 0，否则返回 -1。
 */
static int parse_song(char *line, Song *song)
{
    char *field[5] = {line}; /* 各字段的起始位置。 */

    for (int i = 0; i < 4; ++i) {
        char *bar = strchr(field[i], '|'); //寻找字符中的 '|',以此来区分不同信息
        *bar = '\0'; /* 把分隔符改成字符串结尾，截出当前字段。 */
        field[i + 1] = bar + 1; /* 记录下一个字段的起始地址。 */
    }

    if (strchr(field[4], '|') || parse_id(field[0], &song->id) || song->id <= 0) { /* 检查字段数量及正整数编号。 */
        return -1; /* 字段数量或编号不符合格式。 */
    }

    return copy_field(song->title, sizeof song->title, field[1]) || /* 依次检查并复制歌名、歌手、专辑和路径。 */
                   copy_field(song->artist, sizeof song->artist, field[2]) ||
                   copy_field(song->album, sizeof song->album, field[3]) ||
                   copy_field(song->path, sizeof song->path, field[4]) ? -1 : 0;
}

/*
 * 功能：读取并校验歌单文件中的所有歌曲记录，同时拒绝重复编号和超出数组上限的数据。
 * 参数：path 为歌单文件路径；list 为接收歌曲的输出数组；count 为有效歌曲数量的输入/输出指针，读取结果会累加到该数量。
 * 返回：int；文件读取、记录校验及关闭均成功时返回 0，任一环节失败返回 -1。
 */
static int load_songs(const char *path, Song *list, int *count)
{
    FILE *file = fopen(path, "rb"); /* 歌单文件。 */
    char line[sizeof(Song) + 32];    /* 一行记录缓冲区。 */
    int status = 0;                  /* read_line 的返回状态。 */

    if (!file) return -1; /* 歌单打不开时无法继续加载。 */

    while ((status = read_line(file, line, sizeof line)) > 0) {
        if (!line[0]) continue; /* 空行不代表歌曲记录。 */

        if (*count == MAX_SONGS || parse_song(line, &list[*count]) ||
            find_index(list, *count, list[*count].id) >= 0) { /* 检查容量、格式和重复编号。 */
            status = -1; /* 记录不合法，停止读取。 */
            break; /* 不再访问后续记录。 */
        }

        ++*count; /* 通过检查后才增加有效歌曲数。 */
    }

    if (fclose(file)) status = -1; /* 关闭失败也按读取失败处理。 */
    return status < 0 ? -1 : 0; /* 把内部状态转换为接口约定的结果。 */
}

/*
 * 功能：读取收藏文件中的歌曲编号并设置收藏标记；文件不存在视为空收藏，歌单中不存在的编号忽略。
 * 参数：path 为收藏文件路径；list 为当前歌单数组；count 为歌单中的有效歌曲数量；flags 为接收收藏标记的输出数组。
 * 返回：int；读取和校验成功或收藏文件不存在时返回 0，文件损坏、读取或关闭失败时返回 -1。
 */
static int load_favorites(const char *path, const Song *list, int count,
                          unsigned char *flags)
{
    FILE *file = fopen(path, "rb"); /* 收藏文件。 */
    char line[64];                   /* 每行一个编号。 */
    int status = 0;                  /* read_line 的返回状态。 */

    if (!file) return errno == ENOENT ? 0 : -1; /* 文件不存在为空收藏，其他错误失败。 */

    while ((status = read_line(file, line, sizeof line)) > 0) {
        int id;                      /* 文件中的歌曲编号。 */
        int index;                   /* 对应的歌曲下标。 */
        if (!line[0]) continue; /* 忽略空行。 */
        if (parse_id(line, &id)) { /* 收藏文件中的非数字编号视为损坏。 */
            status = -1; /* 标记输入文件损坏。 */
            break; /* 停止读取收藏编号。 */
        }

        index = find_index(list, count, id); /* 查这个编号是否在歌单中。 */
        if (index >= 0) flags[index] = 1; /* 标记收藏并自动去重。 */
    }
    if (fclose(file)) status = -1; /* 检查文件关闭错误。 */
    return status < 0 ? -1 : 0; /* 返回成功或失败给 data_load。 */
}

/*
 * 功能：加载歌单及收藏文件；先在临时数组中完成读取和校验，全部成功后才更新模块内部数据。
 * 参数：song_file 为 UTF-8 歌单文件路径；favorite_file 为收藏编号文件路径。
 * 返回：int；两份数据成功加载后返回 0；参数无效或读取、校验失败时返回 -1，并保留上次成功加载的数据。
 */
int data_load(const char *song_file, const char *favorite_file)
{
    Song loaded[MAX_SONGS] = {0};         /* 临时歌单。 */
    unsigned char flags[MAX_SONGS] = {0}; /* 临时收藏标记。 */
    int count = 0;                        /* 临时歌曲数量。 */

    if (!song_file || !*song_file || !favorite_file || !*favorite_file ||
        load_songs(song_file, loaded, &count) ||
        load_favorites(favorite_file, loaded, count, flags)) { /* 任一路径无效或文件加载失败。 */
        return -1; /* 保留旧数据并报告失败。 */
    }

    memcpy(songs, loaded, sizeof songs); /* 校验成功后才替换正式歌单。 */
    memcpy(favorites, flags, sizeof favorites); /* 同步替换收藏标记。 */
    song_count = count; /* 更新当前有效歌曲数量。 */
    return 0; /* 新歌单和收藏均已加载成功。 */
}

/*
 * 功能：按歌单顺序将全部歌曲复制到调用者提供的数组，不暴露模块内部数组。
 * 参数：out 为接收歌曲副本的输出数组；capacity 为 out 可容纳的 Song 元素数量。
 * 返回：int；成功返回复制的歌曲数量（歌单为空时返回 0），容量不足或所需输出数组为空时返回 -1。
 */
int data_get_songs(Song *out, int capacity)
{
    if (capacity < song_count || (song_count && !out)) { /* 容量不足或需要输出却没有数组时失败。 */
        return -1; /* 不进行部分复制。 */
    }

    if (song_count) memcpy(out, songs, (size_t)song_count * sizeof *out); /* 复制歌曲，不暴露内部数组。 */
    return song_count; /* 返回复制的歌曲数量。 */
}

/*
 * 功能：按歌曲编号查找歌曲，并将找到的歌曲复制给调用者。
 * 参数：song_id 为要查找的歌曲编号；out 为接收歌曲副本的输出结构体指针。
 * 返回：int；找到歌曲且 out 有效时返回 0，歌曲不存在或 out 为空时返回 -1。
 */
int data_find_song(int song_id, Song *out)
{
    int index = find_index(songs, song_count, song_id); /* 歌曲下标。 */
    if (index < 0 || !out) { /* 找不到歌曲或输出地址无效时失败。 */
        return -1; /* 查询失败时不写入输出参数。 */
    }

    *out = songs[index]; /* 将找到的歌曲复制给调用者。 */
    return 0; /* 表示查询成功。 */
}

/*
 * 功能：按歌单顺序将所有已收藏歌曲复制到调用者提供的数组。
 * 参数：out 为接收收藏歌曲副本的输出数组；capacity 为 out 可容纳的 Song 元素数量。
 * 返回：int；成功返回复制的收藏歌曲数量（没有收藏时返回 0），容量不足或所需输出数组为空时返回 -1；失败时不部分复制。
 */
int data_get_favorites(Song *out, int capacity)
{
    int count = 0; /* 收藏数量，也用作输出下标。 */

    for (int i = 0; i < song_count; ++i) count += favorites[i]; /* 先数收藏，避免容量不足时部分写出。 */

    if (capacity < count || (count && !out)) { /* 确认空间足够后才开始复制。 */
        return -1; /* 容量不足时不写入部分结果。 */
    }

    count = 0; /* 重新从输出数组的第一个位置开始。 */
    for (int i = 0; i < song_count; ++i)
        if (favorites[i]) out[count++] = songs[i]; /* 按歌单顺序复制收藏歌曲。 */
    return count; /* 返回实际复制的收藏数。 */
}

/*
 * 功能：查询指定歌曲当前是否已收藏。
 * 参数：song_id 为要查询的歌曲编号。
 * 返回：int；已收藏返回 1，未收藏返回 0，歌单中不存在该编号返回 -1。
 */
int data_is_favorite(int song_id)
{
    int index = find_index(songs, song_count, song_id); /* 歌曲下标。 */

    return index < 0 ? -1 : favorites[index]; /* 不存在返回 -1，否则返回 0 或 1。 */
}

/*
 * 功能：把指定歌曲的收藏标记设为给定状态；重复设置相同状态也视为成功。
 * 参数：song_id 为要修改收藏状态的歌曲编号；value 为收藏标记，1 表示已收藏，0 表示未收藏。
 * 返回：int；歌曲存在并完成设置时返回 0，歌曲编号不存在时返回 -1。
 */
static int set_favorite(int song_id, unsigned char value)
{
    int index = find_index(songs, song_count, song_id); /* 歌曲下标。 */

    if (index < 0) return -1; /* 只允许修改歌单里存在的歌曲。 */

    favorites[index] = value; /* value 为 1 表示收藏，为 0 表示取消。 */
    return 0; /* 重复设置同一状态也算成功。 */
}

/*
 * 功能：将指定歌曲加入收藏。
 * 参数：song_id 为要收藏的歌曲编号。
 * 返回：int；歌曲存在时返回 0（已收藏时重复添加仍成功），歌曲不存在时返回 -1。
 */
int data_add_favorite(int song_id)
{
    return set_favorite(song_id, 1); /* 将该歌曲收藏标记设为 1。 */
}

/*
 * 功能：取消指定歌曲的收藏状态。
 * 参数：song_id 为要取消收藏的歌曲编号。
 * 返回：int；歌曲存在时返回 0（未收藏时重复取消仍成功），歌曲不存在时返回 -1。
 */
int data_remove_favorite(int song_id)
{
    return set_favorite(song_id, 0); /* 将该歌曲收藏标记设为 0。 */
}

/*
 * 功能：把当前收藏歌曲编号保存到文本文件；先写入同目录下的独占 .tmp 临时文件，成功后再替换目标文件。
 * 参数：favorite_file 为要保存到的收藏文件路径，不能为空。
 * 返回：int；写入、关闭及替换全部成功时返回 0，参数无效、临时文件创建失败或其他文件操作失败时返回 -1。
 */
int data_save_favorites(const char *favorite_file)
{
    char *temporary; /* 临时文件路径。 */
    FILE *file;      /* 临时文件流。 */
    int result = 0;  /* 写入或替换失败标记。 */

    if (!favorite_file || !*favorite_file) return -1; /* 拒绝空文件路径。 */

    temporary = malloc(strlen(favorite_file) + sizeof ".tmp"); /* 为临时文件名分配空间。 */
    if (!temporary) return -1; /* 内存不足时不能安全生成路径。 */

    strcpy(temporary, favorite_file); /* 先复制正式文件路径。 */
    strcat(temporary, ".tmp"); /* 在路径末尾追加临时文件扩展名。 */
#ifdef _WIN32
    {
        int fd = _open(temporary, _O_WRONLY | _O_CREAT | _O_EXCL | _O_TEXT,
                       _S_IREAD | _S_IWRITE); /* 不覆盖已有临时文件。 */

        if (fd < 0) { free(temporary); return -1; } /* 独占创建失败时释放路径并返回。 */
        file = _fdopen(fd, "w"); /* 把文件描述符转换为 stdio 文件流。 */

        if (!file) { _close(fd); remove(temporary); } /* 转换失败时关闭并删除临时文件。 */
    }
#else
    file = fopen(temporary, "wx"); /* 以独占方式新建临时文件，避免覆盖旧临时文件。 */
#endif

    if (!file) { free(temporary); return -1; } /* 临时文件创建失败，释放路径后返回。 */

    for (int i = 0; i < song_count; ++i)
        if (favorites[i] && fprintf(file, "%d\n", songs[i].id) < 0) { /* 每个收藏编号写一行。 */
            result = -1; /* 记录写入失败。 */
        }

    if (fclose(file)) result = -1; /* 刷新缓冲区并检查写盘结果。 */

    if (!result) { /* 只有完整写入成功后才替换收藏文件。 */
#ifdef _WIN32
        if (!MoveFileExA(temporary, favorite_file, MOVEFILE_REPLACE_EXISTING)) result = -1; /* Windows 上用新文件替换旧文件。 */
#else
        if (rename(temporary, favorite_file)) result = -1; /* 其他系统用 rename 完成替换。 */
#endif
    }

    if (result) remove(temporary); /* 失败时删除未完成的临时文件。 */
    free(temporary); /* 释放动态分配的临时路径。 */
    return result; /* 返回保存或替换结果。 */
}

