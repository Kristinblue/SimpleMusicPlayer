#ifndef SIMPLE_MUSIC_SONG_H
#define SIMPLE_MUSIC_SONG_H

/* 三个人共用。文本统一 UTF-8，路径可相对于工程根目录。 */
#define MAX_SONGS 200
typedef struct Song {
    int id;                /* 唯一正整数编号，不是数组下标 */
    char title[128];
    char artist[128];
    char album[128];
    char path[512];         /* 本地 WAV 文件路径 */
} Song;

#endif
