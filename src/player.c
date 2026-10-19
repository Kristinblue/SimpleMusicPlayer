#include "player.h"

#include <limits.h>
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

static int player_apply_volume(MCIDEVICEID device_id, int volume)
{
    MCI_DGV_SETAUDIO_PARMSW params = {0};
    params.dwItem = MCI_DGV_SETAUDIO_VOLUME;
    params.dwValue = (DWORD)(volume * 10); /* MCI 使用 0~1000。 */
    return mciSendCommandW(device_id, MCI_SETAUDIO,
                          MCI_DGV_SETAUDIO_ITEM | MCI_DGV_SETAUDIO_VALUE | MCI_WAIT,
                          (DWORD_PTR)&params) == 0 ? 0 : -1;
}

int player_init(void)
{
    /* 重复初始化时，先释放上一次使用的资源。 */
    player_close();
    if (player.device_id != 0) {
        return -1;
    }
    player.initialized = 1;
    return 0;
}

void player_close(void)
{
    if (player.device_id != 0) {
        MCI_GENERIC_PARMS params = {0};
        if (mciSendCommandW(player.device_id, MCI_CLOSE, MCI_WAIT,
                            (DWORD_PTR)&params) != 0) {
            return; /* 保留设备编号，下次关闭时仍可重试。 */
        }
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

    /* 每次切歌都应用已保存的音量。 */
    if (player_apply_volume(player.device_id, player.volume) != 0) {
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

int player_pause(void)
{
    MCI_GENERIC_PARMS params = {0};

    if (!player.initialized || player.device_id == 0 ||
        player.state == PLAYER_STOPPED) {
        return -1;
    }
    if (player.state == PLAYER_PAUSED) {
        return 0; /* 重复暂停不改变进度。 */
    }
    if (mciSendCommandW(player.device_id, MCI_PAUSE, MCI_WAIT,
                        (DWORD_PTR)&params) != 0) {
        return -1;
    }
    player.state = PLAYER_PAUSED;
    return 0;
}

int player_resume(void)
{
    MCI_PLAY_PARMS params = {0};

    if (!player.initialized || player.device_id == 0 ||
        player.state == PLAYER_STOPPED) {
        return -1;
    }
    if (player.state == PLAYER_PLAYING) {
        return 0; /* 重复继续不从头播放。 */
    }
    /* 不传 MCI_FROM，从暂停位置继续；不等待歌曲播放结束。 */
    if (mciSendCommandW(player.device_id, MCI_PLAY, 0,
                        (DWORD_PTR)&params) != 0) {
        return -1;
    }
    player.state = PLAYER_PLAYING;
    return 0;
}

int player_stop(void)
{
    MCI_SEEK_PARMS params = {0};

    if (!player.initialized || player.device_id == 0) {
        return -1;
    }
    /* seek 会停止播放并定位；仅 stop 不保证所有驱动都将进度归零。 */
    if (mciSendCommandW(player.device_id, MCI_SEEK,
                        MCI_SEEK_TO_START | MCI_WAIT,
                        (DWORD_PTR)&params) != 0) {
        return -1;
    }
    player.state = PLAYER_STOPPED;
    return 0;
}

static int player_change_song(int direction)
{
    int index;

    if (!player.initialized || player.count == 0 || player.current_index < 0) {
        return -1;
    }
    index = player.current_index + direction;
    if (index < 0 || index >= player.count) {
        if (player.mode == PLAYER_ORDER) {
            return -1; /* 顺序模式到边界时保留原来的歌曲和状态。 */
        }
        index = index < 0 ? player.count - 1 : 0;
    }
    if (player_play(player.songs[index].id) != 0) {
        return -1;
    }
    return player.songs[index].id;
}

int player_previous(void)
{
    return player_change_song(-1);
}

int player_next(void)
{
    return player_change_song(1);
}

int player_set_mode(PlayerMode mode)
{
    if (!player.initialized || (mode != PLAYER_ORDER &&
        mode != PLAYER_LIST_LOOP && mode != PLAYER_SINGLE_LOOP)) {
        return -1;
    }
    player.mode = mode;
    return 0;
}

int player_seek(int position_ms)
{
    int duration;
    PlayerState previous_state;
    MCI_SEEK_PARMS seek_params = {0};
    MCI_PLAY_PARMS play_params = {0};

    if (!player.initialized || player.device_id == 0 ||
        player.current_index < 0 || position_ms < 0) {
        return -1;
    }
    duration = player_get_duration();
    if (duration < 0 || position_ms > duration) {
        return -1;
    }
    previous_state = player.state;
    seek_params.dwTo = (DWORD)position_ms;
    if (mciSendCommandW(player.device_id, MCI_SEEK, MCI_TO | MCI_WAIT,
                        (DWORD_PTR)&seek_params) != 0) {
        return -1;
    }

    /* MCI_SEEK 会停止设备；原来在播放时，应从新位置继续。 */
    if (previous_state == PLAYER_PLAYING && position_ms < duration) {
        if (mciSendCommandW(player.device_id, MCI_PLAY, 0,
                            (DWORD_PTR)&play_params) != 0) {
            player.state = PLAYER_STOPPED;
            return -1;
        }
    }
    /* 暂停/停止时拖动保持原状态；播放时跳至末尾由 update 处理结束。 */
    player.state = previous_state;
    return 0;
}

int player_set_volume(int volume)
{
    if (!player.initialized || volume < 0 || volume > 100) {
        return -1;
    }
    if (player.device_id != 0 &&
        player_apply_volume(player.device_id, volume) != 0) {
        return -1;
    }
    player.volume = volume;
    return 0;
}

/* 查询真实设备进度，便于测试以及后续界面显示。 */
static int player_query_time(DWORD item)
{
    MCI_STATUS_PARMS params = {0};

    if (player.device_id == 0) {
        return 0;
    }
    params.dwItem = item;
    if (mciSendCommandW(player.device_id, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT,
                        (DWORD_PTR)&params) != 0 || params.dwReturn > INT_MAX) {
        return -1;
    }
    return (int)params.dwReturn;
}

int player_get_current_id(void)
{
    if (player.current_index < 0 || player.current_index >= player.count) {
        return -1;
    }
    return player.songs[player.current_index].id;
}

int player_get_position(void)
{
    return player_query_time(MCI_STATUS_POSITION);
}

int player_get_duration(void)
{
    return player_query_time(MCI_STATUS_LENGTH);
}

PlayerState player_get_state(void)
{
    return player.state;
}

int player_update(void)
{
    MCI_STATUS_PARMS params = {0};
    int duration;
    int position;
    int next_index;

    if (!player.initialized) {
        return -1;
    }
    if (player.state != PLAYER_PLAYING || player.device_id == 0 ||
        player.current_index < 0) {
        return 0;
    }
    params.dwItem = MCI_STATUS_MODE;
    if (mciSendCommandW(player.device_id, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT,
                        (DWORD_PTR)&params) != 0) {
        return -1;
    }
    if (params.dwReturn != MCI_MODE_STOP) {
        return 0; /* 仍在播放或设备正在准备，不提前切歌。 */
    }
    duration = player_get_duration();
    position = player_get_position();
    if (duration < 0 || position < 0) {
        return -1;
    }
    player.state = PLAYER_STOPPED;
    if (position < duration) {
        return -1; /* 异常提前停止，不当作正常结束去切下一首。 */
    }

    next_index = player.current_index;
    if (player.mode != PLAYER_SINGLE_LOOP) {
        ++next_index;
        if (next_index == player.count) {
            if (player.mode == PLAYER_ORDER) {
                return 0; /* 最后一首自然结束，保留歌曲和末尾进度。 */
            }
            next_index = 0;
        }
    }
    return player_play(player.songs[next_index].id);
}
