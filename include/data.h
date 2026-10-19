#ifndef SIMPLE_MUSIC_DATA_H
#define SIMPLE_MUSIC_DATA_H

#include "song.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 数据模块实现在 src/data.c；接口不依赖 Qt，界面负责显示错误。 */
/* 输出数组容量按 Song 元素个数计算；不足时返回 -1，不部分写入。
 * 结果为空时允许 out 为 NULL、capacity 为 0；负容量始终无效。
 */

/* song_file：UTF-8 歌单文件名；favorite_file：收藏编号文件名。
 * 返回 0 成功，-1 失败；缺失收藏文件视为空收藏，加载失败保留旧数据。
 * 歌单每行：正整数编号|歌名|歌手|专辑|WAV路径；字段非空且不能含 | 或换行。
 * 收藏每行一个整数；不存在的编号忽略，重复编号去重，非整数记录报错。
 */
int data_load(const char *song_file, const char *favorite_file);

/* out：调用者的歌曲数组；capacity：数组容量；返回复制数量，失败返回 -1。 */
int data_get_songs(Song *out, int capacity);
/* song_id：歌曲编号；out：歌曲副本；找到返回 0，未找到或空指针返回 -1。 */
int data_find_song(int song_id, Song *out);
/* out/capacity：输出数组及容量；按歌单顺序返回收藏数量，失败返回 -1。 */
int data_get_favorites(Song *out, int capacity);
/* song_id：歌曲编号；返回 1 已收藏、0 未收藏、-1 编号不存在。 */
int data_is_favorite(int song_id);
/* song_id：待收藏编号；返回 0 成功（含重复添加），-1 编号不存在。 */
int data_add_favorite(int song_id);
/* song_id：待取消收藏编号；返回 0 成功（含重复取消），-1 编号不存在。 */
int data_remove_favorite(int song_id);
/* favorite_file：输出文件名；先写同目录 .tmp 文件再替换，返回 0 成功、-1 失败。
 * 保存失败保留旧文件；已有 .tmp 文件时拒绝保存，避免覆盖其他文件。
 */
int data_save_favorites(const char *favorite_file);

#ifdef __cplusplus
}
#endif

#endif
