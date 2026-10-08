#ifndef SIMPLE_MUSIC_DATA_H
#define SIMPLE_MUSIC_DATA_H

#include "song.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A 实现于 src/data.c。目前只有声明，界面尚未调用。 */
/* 操作类函数：0 成功，-1 失败；查询类函数见各自说明。 */
/* 输出数组容量不足时返回 -1，不越界，不静默截断。 */
int data_load(const char *song_file, const char *favorite_file);
int data_get_songs(Song *out, int capacity); /* 返回复制数量，失败 -1 */
int data_find_song(int song_id, Song *out);  /* 0 找到，-1 不存在 */
int data_get_favorites(Song *out, int capacity); /* 返回数量，失败 -1 */
int data_is_favorite(int song_id);          /* 1 是，0 否，-1 无此歌曲 */
int data_add_favorite(int song_id);         /* 重复添加仍返回 0 */
int data_remove_favorite(int song_id);      /* 原本未收藏仍返回 0 */
int data_save_favorites(const char *favorite_file);

#ifdef __cplusplus
}
#endif

#endif
