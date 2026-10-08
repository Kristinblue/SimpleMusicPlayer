#ifndef SIMPLE_MUSIC_PLAYER_H
#define SIMPLE_MUSIC_PLAYER_H

#include "song.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PlayerMode {
    PLAYER_ORDER, PLAYER_LIST_LOOP, PLAYER_SINGLE_LOOP
} PlayerMode;

typedef enum PlayerState {
    PLAYER_STOPPED, PLAYER_PLAYING, PLAYER_PAUSED
} PlayerState;

/* B 实现于 src/player.c，使用 Windows MCI（winmm 已链接）。 */
/* 操作类函数：0 成功，-1 失败。播放必须立即返回，不能阻塞窗口。 */
int player_init(void);
void player_close(void);
int player_set_playlist(const Song *songs, int count); /* 内部复制列表 */
int player_play(int song_id);
int player_pause(void);
int player_resume(void);
int player_stop(void);
int player_previous(void); /* 返回新的歌曲 ID，失败 -1 */
int player_next(void);     /* 返回新的歌曲 ID，失败 -1 */
int player_set_mode(PlayerMode mode);
int player_seek(int position_ms);
int player_set_volume(int volume); /* 0~100 */
int player_get_current_id(void);   /* 没有歌曲时 -1 */
int player_get_position(void);     /* 毫秒，没有歌曲时 0，失败 -1 */
int player_get_duration(void);     /* 毫秒，没有歌曲时 0，失败 -1 */
PlayerState player_get_state(void);
int player_update(void); /* 定时检测结束，并按模式处理自动切歌 */

#ifdef __cplusplus
}
#endif

#endif
