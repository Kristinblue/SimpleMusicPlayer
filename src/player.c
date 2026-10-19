#include "player.h"

#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#include <digitalv.h>

/* 只在本文件内保存状态，不依赖数据模块的内部变量。 */
static struct {
    Song songs[MAX_SONGS];
    int count;
    int current_index; /* 数组下标；-1 表示没有当前歌曲。 */
    PlayerState state;
    PlayerMode mode;
    int volume;
    int initialized;
    MCIDEVICEID device_id; /* 0 表示没有打开音频设备。 */
} player = {
    .current_index = -1,
    .state = PLAYER_STOPPED,
    .mode = PLAYER_ORDER,
    .volume = 70
};

int player_init(void)
{
    /* 重复初始化时，先释放上一次使用的资源。 */
    player_close();
    player.initialized = 1;
    return 0;
}

void player_close(void)
{
    if (player.device_id != 0) {
        MCI_GENERIC_PARMS params = {0};
        mciSendCommandW(player.device_id, MCI_CLOSE, MCI_WAIT,
                        (DWORD_PTR)&params);
    }

    memset(&player, 0, sizeof(player));
    player.current_index = -1;
    player.state = PLAYER_STOPPED;
    player.mode = PLAYER_ORDER;
    player.volume = 70;
}

int player_set_playlist(const Song *songs, int count)
{
    if (!player.initialized || count < 0 || count > MAX_SONGS ||
        (count > 0 && songs == NULL)) {
        return -1;
    }

    /* 先检查完整列表，避免无效输入破坏原来的歌单。 */
    for (int i = 0; i < count; ++i) {
        if (songs[i].id <= 0 || songs[i].path[0] == '\0' ||
            memchr(songs[i].path, '\0', sizeof(songs[i].path)) == NULL) {
            return -1;
        }
        for (int j = 0; j < i; ++j) {
            if (songs[i].id == songs[j].id) {
                return -1;
            }
        }
    }

    /* 关闭旧音频即停止旧播放；关闭失败时保留原来的状态。 */
    if (player.device_id != 0) {
        MCI_GENERIC_PARMS params = {0};
        if (mciSendCommandW(player.device_id, MCI_CLOSE, MCI_WAIT,
                            (DWORD_PTR)&params) != 0) {
            return -1;
        }
        player.device_id = 0;
    }

    memset(player.songs, 0, sizeof(player.songs));
    if (count > 0) {
        memcpy(player.songs, songs, (size_t)count * sizeof(Song));
    }
    player.count = count;
    player.current_index = -1;
    player.state = PLAYER_STOPPED;
    return 0;
}

int player_play(int song_id)
{
    int index = -1;
    WCHAR path[512];
    MCI_OPEN_PARMSW open_params = {0};
    MCI_SET_PARMS time_params = {0};
    MCI_DGV_SETAUDIO_PARMSW volume_params = {0};
    MCI_PLAY_PARMS play_params = {0};
    MCI_GENERIC_PARMS close_params = {0};

    if (!player.initialized) {
        return -1;
    }
    for (int i = 0; i < player.count; ++i) {
        if (player.songs[i].id == song_id) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return -1;
    }

    /* 歌单路径是 UTF-8，Windows 的宽字符接口使用 UTF-16。 */
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            player.songs[index].path, -1,
                            path, (int)(sizeof(path) / sizeof(path[0]))) == 0) {
        return -1;
    }

    if (player.device_id != 0) {
        if (mciSendCommandW(player.device_id, MCI_CLOSE, MCI_WAIT,
                            (DWORD_PTR)&close_params) != 0) {
            return -1;
        }
        player.device_id = 0;
    }
    player.current_index = -1;
    player.state = PLAYER_STOPPED;

    /* 使用支持独立音量控制的 MCI 驱动打开音频。 */
    open_params.lpstrDeviceType = L"mpegvideo";
    open_params.lpstrElementName = path;
    if (mciSendCommandW(0, MCI_OPEN,
                        MCI_OPEN_TYPE | MCI_OPEN_ELEMENT | MCI_WAIT,
                        (DWORD_PTR)&open_params) != 0) {
        return -1;
    }
    player.device_id = open_params.wDeviceID;

    time_params.dwTimeFormat = MCI_FORMAT_MILLISECONDS;
    if (mciSendCommandW(player.device_id, MCI_SET,
                        MCI_SET_TIME_FORMAT | MCI_WAIT,
                        (DWORD_PTR)&time_params) != 0) {
        goto playback_failed;
    }

    /* MCI 的音量范围为 0~1000，公共接口使用 0~100。 */
    volume_params.dwItem = MCI_DGV_SETAUDIO_VOLUME;
    volume_params.dwValue = (DWORD)(player.volume * 10);
    if (mciSendCommandW(player.device_id, MCI_SETAUDIO,
                        MCI_DGV_SETAUDIO_ITEM | MCI_DGV_SETAUDIO_VALUE | MCI_WAIT,
                        (DWORD_PTR)&volume_params) != 0) {
        goto playback_failed;
    }

    /* 从头播放。不加 MCI_WAIT，避免等待整首歌结束。 */
    play_params.dwFrom = 0;
    if (mciSendCommandW(player.device_id, MCI_PLAY, MCI_FROM,
                        (DWORD_PTR)&play_params) != 0) {
        goto playback_failed;
    }
    player.current_index = index;
    player.state = PLAYER_PLAYING;
    return 0;

playback_failed:
    if (mciSendCommandW(player.device_id, MCI_CLOSE, MCI_WAIT,
                        (DWORD_PTR)&close_params) == 0) {
        player.device_id = 0;
    }
    return -1;
}
