#ifndef SIMPLE_MUSIC_MAIN_H
#define SIMPLE_MUSIC_MAIN_H

// Qt 基础：窗口和字符串。
#include <QWidget>
#include <QString>

// Qt 控件：文字、列表、按钮和滑块。
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>

// Qt 定时器：定时读取播放进度。
#include <QTimer>

// 项目共用数据：歌曲结构和最大歌曲数。
#include "song.h"

// 主窗口声明；具体函数实现在 src/main.cpp 中。
// 使用已有信号连接回调，不需要自定义信号或 Q_OBJECT。
class MusicWindow : public QWidget {
public:
    MusicWindow(); // 创建窗口、读取数据并初始化播放模块，无参数。
    ~MusicWindow() override; // 关闭窗口时释放播放资源，无参数。

private:
    Song songs[MAX_SONGS] = {}; // 数据模块返回的歌曲副本，用于显示。
    int songCount = 0; // 实际从歌单文件加载的歌曲数。
    int selected = -1; // 界面选中的数组下标，-1 表示未选择。
    bool playerReady = false; // 播放模块是否初始化成功。
    int mode = 0; // 与 PlayerMode 的三个枚举值对应。
    int volumeValue = 70; // 上次成功设置的音量，失败时恢复滑块。
    QTimer *playbackTimer; // 定时检测播放结束并更新界面。

    // 窗口中的控件。
    QListWidget *songList; // 左侧歌单。
    QListWidget *favoriteList; // 右侧收藏夹。
    QLabel *songCountLabel; // 歌曲数量。
    QLabel *favoriteCount; // 收藏数量。
    QLabel *currentSong; // 当前选中的歌名和歌手。
    QLabel *elapsed; // 当前播放时间。
    QLabel *duration; // 当前歌曲总时长。
    QPushButton *playButton; // 播放和暂停按钮。
    QPushButton *modeButton; // 播放模式按钮。
    QSlider *progress; // 播放进度滑块。
    QSlider *volume; // 音量滑块。

    // 创建界面。
    // title 为标题，favoritePanel 指定是否为收藏面板；返回创建的面板。
    QWidget *createListPanel(QString title, bool favoritePanel);
    QWidget *createControls(); // 创建播放控制区，返回面板，无参数。
    // 向 list 添加歌曲行；index 为歌曲下标，isFavorite 决定爱心外观，无返回值。
    void addSongRow(QListWidget *list, int index, bool isFavorite = false);

    // 数据读取与列表显示。
    void refreshLists(); // 刷新两侧列表，无参数、无返回值。
    int findSongIndex(int id); // 按歌曲编号查找数组下标，不存在时返回 -1。
    void loadData(); // 读取歌单和收藏，失败时提示，无参数、无返回值。
    void selectSong(int index); // 选择歌曲下标，必要时停止旧播放，无返回值。
    void showSelectedSong(int index); // 只同步此下标的文字和选中行，无返回值。
    void onFavoriteClicked(int index); // 收藏按钮回调，index 为歌曲下标，无返回值。
    void showSongInfo(int index); // 显示此下标对应的歌曲信息，无返回值。

    // 播放控制。
    void togglePlay(); // 播放、暂停或继续，无参数、无返回值。
    void changeSong(int direction); // -1 上一首，1 下一首，无返回值。
    void updatePlaybackUi(); // 查询真实状态和进度并刷新，无参数、无返回值。
    void showPlaybackError(QString message); // 弹窗显示错误原因，无返回值。
};

#endif
