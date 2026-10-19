//hyf的

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "data.h"
#include "player.h"
#include "song.h"

// 将毫秒转换为分:秒；milliseconds 为进度或时长，返回显示文本。
static QString formatTime(int milliseconds)
{
    int seconds = qMax(0, milliseconds) / 1000; // 已经过的整秒数。
    return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

// 小类只用来把控件、界面状态和回调放在一起，全部 Qt 代码都在本文件。
// 使用已有信号连接 lambda，不需要自定义信号、Q_OBJECT 或 .ui 文件。
class MusicWindow : public QWidget {
public:
    MusicWindow();
    ~MusicWindow() override;

private:
    Song songs[MAX_SONGS] = {}; // 数据模块返回的歌曲副本，用于显示。
    int songCount = 0; // 实际从歌单文件加载的歌曲数。
    int selected = -1; // 数组下标；传给 A/B 时使用 songs[selected].id。
    bool playerReady = false; // 播放模块是否初始化成功。
    PlayerState displayedState = PLAYER_STOPPED; // 上一次显示的实际播放状态。
    int mode = 0; // 与 PlayerMode 的三个枚举值对应。
    int volumeValue = 70; // 上次成功设置的音量，失败时恢复滑块。
    QTimer *playbackTimer; // 定时检测播放结束并更新界面。

    QListWidget *songList;
    QListWidget *favoriteList;
    QLabel *songCountLabel;
    QLabel *favoriteCount;
    QLabel *currentSong;
    QLabel *elapsed;
    QLabel *duration;
    QLabel *status;
    QPushButton *playButton;
    QPushButton *modeButton;
    QSlider *progress;
    QSlider *volume; // 音量滑块。

    QWidget *createListPanel(const QString &title, bool favoritePanel);
    QWidget *createControls();
    void addSongRow(QListWidget *list, int index, bool isFavorite = false); // 只负责显示
    void refreshLists();
    void loadData(); // 读取文件并刷新界面；失败时显示错误，无参数、无返回值。
    void selectSong(int index);
    void showSelectedSong(int index); // 只显示选中歌曲，不停止或开始播放。
    void onFavoriteClicked(int index);
    void showSongInfo(int index);
    void togglePlay();
    void changeSong(int direction);
    void updatePlaybackUi(); // 读取实际状态、歌曲编号、进度和时长，无参数和返回值。
    void showPlaybackError(const QString &message); // 显示播放错误，message 为原因，无返回值。
};

// ① 窗口整体布局
MusicWindow::MusicWindow()
{
    setWindowTitle(QStringLiteral("Simple Music Player"));
    setObjectName("window");
    resize(1200, 760);
    setMinimumSize(960, 600);

    // 这段 QSS 只负责颜色和外观；先看下面的布局和 connect 即可。
    setStyleSheet(QStringLiteral(R"(
        QWidget { color: #293448; font-family: 'Microsoft YaHei'; font-size: 13px; }
        QWidget#window { background: #f5f6fa; }
        QWidget#panel { background: white; border-radius: 14px; }
        QLabel#heading { font-size: 19px; font-weight: bold; }
        QLabel#muted { color: #8790a0; font-size: 11px; }
        QListWidget { border: none; background: transparent; outline: none; }
        QListWidget::item { border-radius: 8px; }
        QListWidget::item:selected { background: #edf0ff; }
        QPushButton { border: none; border-radius: 8px; background: transparent; }
        QPushButton:hover { background: #edf0f6; }
        QPushButton#heart { color: #939bab; font-family: 'Segoe UI Symbol'; font-size: 25px; }
        QPushButton#heart[favorite="true"] { color: #e55370; }
        QPushButton#more { color: #8991a0; font-size: 21px; }
        QPushButton#play { background: #5f6ee8; border-radius: 28px; }
        QPushButton#play:hover { background: #7180f0; }
        QSlider::groove:horizontal { height: 4px; background: #e5e8ef; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #7481e9; border-radius: 2px; }
        QSlider::handle:horizontal { width: 12px; margin: -4px 0; background: #7481e9; border-radius: 6px; }
        QScrollBar:vertical { width: 8px; background: transparent; margin: 0; }
        QScrollBar::handle:vertical { background: #d8dde7; min-height: 30px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 24);
    root->setSpacing(18);
    auto *header = new QHBoxLayout;
    auto *name = new QLabel(QStringLiteral("Simple Music"), this);
    name->setObjectName("heading");
    status = new QLabel(QStringLiteral("收藏自动保存 · 请选择歌曲"), this);
    status->setObjectName("muted");
    header->addWidget(name);
    header->addStretch();
    header->addWidget(status);
    root->addLayout(header);

    // 1 : 2 : 1，左右各约四分之一；中间上方留空，下方放播放控制。
    auto *columns = new QHBoxLayout;
    columns->setSpacing(18);
    QWidget *left = createListPanel(QStringLiteral("歌单"), false);
    QWidget *right = createListPanel(QStringLiteral("收藏夹"), true);
    auto *center = new QWidget(this);
    auto *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->addStretch();
    centerLayout->addWidget(createControls());
    left->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    center->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    right->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    columns->addWidget(left, 1);
    columns->addWidget(center, 2);
    columns->addWidget(right, 1);
    root->addLayout(columns, 1);
    loadData();
    playerReady = player_init() == 0 && player_set_playlist(songs, songCount) == 0;
    playbackTimer = new QTimer(this);
    playbackTimer->setInterval(200);
    QObject::connect(playbackTimer, &QTimer::timeout, this, [this]() {
        if (player_update() != 0) {
            // 查询失败时暂停轮询，避免每 200ms 重复报错；下次播放成功再启动。
            playbackTimer->stop();
            updatePlaybackUi();
            status->setText(QStringLiteral("播放中断，请重新选择歌曲后播放"));
            return;
        }
        updatePlaybackUi();
    });
    if (playerReady) playbackTimer->start();
    else {
        playButton->setEnabled(false);
        status->setText(QStringLiteral("播放器初始化失败"));
    }
}

// 窗口退出时停止轮询并关闭音频设备，无参数、无返回值。
MusicWindow::~MusicWindow()
{
    playbackTimer->stop();
    player_close();
}

// 读取歌单和收藏；加载失败显示空列表并提示，避免使用假数据覆盖旧收藏。
void MusicWindow::loadData()
{
    if (data_load("data/songs.txt", "data/favorites.txt") == 0)
        songCount = data_get_songs(songs, MAX_SONGS);
    else
        songCount = -1;
    if (songCount < 0) {
        songCount = 0;
        status->setText(QStringLiteral("歌单或收藏文件读取失败"));
        QTimer::singleShot(0, this, [this]() {
            QMessageBox::warning(this, QStringLiteral("读取失败"),
                QStringLiteral("无法读取歌单或收藏文件，请检查内容和权限。\n歌单：%1\n收藏：%2")
                    .arg(QDir::current().filePath("data/songs.txt"),
                         QDir::current().filePath("data/favorites.txt")));
        });
    }
    refreshLists();
}

// ② 歌单和收藏夹共用同一种面板、同一种歌曲行
QWidget *MusicWindow::createListPanel(const QString &title, bool favoritePanel)
{
    auto *panel = new QWidget(this);
    panel->setObjectName("panel");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 20, 10, 12);
    layout->setSpacing(12);
    auto *heading = new QLabel(title, panel);
    heading->setObjectName("heading");
    auto *count = new QLabel(panel);
    count->setObjectName("muted");
    auto *list = new QListWidget(panel);
    list->setObjectName(favoritePanel ? "favoriteList" : "songList");
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setResizeMode(QListView::Adjust); // 窗口缩小时，歌曲行跟着变窄。
    layout->addWidget(heading);
    layout->addWidget(count);
    layout->addWidget(list, 1);
    if (favoritePanel) {
        favoriteList = list;
        favoriteCount = count;
    } else {
        songList = list;
        songCountLabel = count;
    }
    QObject::connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        selectSong(item->data(Qt::UserRole).toInt());
    });
    return panel;
}

void MusicWindow::addSongRow(QListWidget *list, int index, bool isFavorite)
{
    auto *item = new QListWidgetItem(list);
    item->setData(Qt::UserRole, index); // 每行记录歌曲下标。
    item->setSizeHint(QSize(0, 68));
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 8, 2, 8);
    layout->setSpacing(2);
    auto *text = new QVBoxLayout;
    text->setSpacing(5);
    auto *title = new QLabel(QString::fromUtf8(songs[index].title), row);
    auto *artist = new QLabel(QString::fromUtf8(songs[index].artist), row);
    artist->setObjectName("muted");
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    artist->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title->setToolTip(title->text());
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    artist->setAttribute(Qt::WA_TransparentForMouseEvents);
    text->addWidget(title);
    text->addWidget(artist);
    layout->addLayout(text, 1);
    // 收藏状态由数据模块提供；界面只负责显示和发送操作。
    auto *heart = new QPushButton(isFavorite ? QStringLiteral("♥") : QStringLiteral("♡"), row);
    heart->setObjectName("heart");
    heart->setFixedSize(32, 36);
    heart->setProperty("favorite", isFavorite);
    heart->setToolTip(isFavorite ? QStringLiteral("取消收藏") : QStringLiteral("收藏"));
    heart->setAccessibleName(QStringLiteral("%1 %2")
        .arg(isFavorite ? QStringLiteral("取消收藏") : QStringLiteral("收藏"),
             QString::fromUtf8(songs[index].title)));
    auto *more = new QPushButton(QStringLiteral("⋯"), row);
    more->setObjectName("more");
    more->setFixedSize(26, 36);
    more->setToolTip(QStringLiteral("歌曲信息"));
    layout->addWidget(heart);
    layout->addWidget(more);
    list->setItemWidget(item, row);
    // 排队处理，等按钮自己的点击事件结束后，再刷新并销毁旧的歌曲行。
    QObject::connect(heart, &QPushButton::clicked, this,
        [this, index]() { onFavoriteClicked(index); }, Qt::QueuedConnection);
    QObject::connect(more, &QPushButton::clicked, this, [this, index]() { showSongInfo(index); });
    if (index == selected)
        list->setCurrentItem(item);
}

void MusicWindow::refreshLists()
{
    // 刷新时保留滚动位置，点击收藏不会把歌单跳回顶部。
    int songScroll = songList->verticalScrollBar()->value(); // 刷新前歌单滚动位置。
    int favoriteScroll = favoriteList->verticalScrollBar()->value(); // 收藏列表滚动位置。
    Song favoriteSongs[MAX_SONGS]; // 数据模块返回的收藏副本，不另行维护收藏状态。
    int count = songCount ? data_get_favorites(favoriteSongs, MAX_SONGS) : 0; // 收藏数量。
    songList->clear();
    favoriteList->clear();
    for (int i = 0; i < songCount; ++i) { // 当前显示的歌单下标。
        addSongRow(songList, i, data_is_favorite(songs[i].id) == 1);
    }
    for (int i = 0; i < count; ++i) { // 当前显示的收藏歌曲下标。
        for (int j = 0; j < songCount; ++j) { // 将歌曲编号映射为界面数组下标。
            if (songs[j].id == favoriteSongs[i].id) {
                addSongRow(favoriteList, j, true);
                break;
            }
        }
    }
    favoriteCount->setText(count < 0 ? QStringLiteral("收藏读取失败")
                                   : QStringLiteral("%1 首收藏").arg(count));
    songCountLabel->setText(QStringLiteral("%1 首歌曲").arg(songCount));
    songList->doItemsLayout();
    favoriteList->doItemsLayout();
    songList->verticalScrollBar()->setValue(songScroll);
    favoriteList->verticalScrollBar()->setValue(favoriteScroll);
}

// ③ 歌曲选择、收藏和信息按钮
void MusicWindow::selectSong(int index)
{
    if (index < 0 || index >= songCount) return;
    if (index == selected) return; // 点击正在播放的同一首，不中断音频。
    if (playerReady && player_get_current_id() >= 0 && player_stop() != 0) {
        showPlaybackError(QStringLiteral("无法停止当前歌曲，请重试。"));
        return;
    }
    showSelectedSong(index);
    updatePlaybackUi();
    status->setText(QStringLiteral("已选择歌曲，点击播放"));
}

// index 为歌曲下标；仅同步文字和两侧选中行，无返回值。
void MusicWindow::showSelectedSong(int index)
{
    selected = index;
    currentSong->setText(QStringLiteral("%1 · %2")
        .arg(QString::fromUtf8(songs[index].title)).arg(QString::fromUtf8(songs[index].artist)));
    // 同步两个列表的选中行，不重新创建控件。
    QListWidget *lists[] = {songList, favoriteList};
    for (QListWidget *list : lists) {
        list->setCurrentRow(-1);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toInt() == index)
                list->setCurrentRow(i);
    }
}

// index 为界面歌曲下标；调用数据接口增删并保存，失败则恢复原状态，无返回值。
void MusicWindow::onFavoriteClicked(int index)
{
    if (index < 0 || index >= songCount) return;
    int id = songs[index].id; // 数据接口使用的歌曲编号。
    int wasFavorite = data_is_favorite(id); // 用于切换收藏及保存失败后的回滚。
    if (wasFavorite < 0 ||
        (wasFavorite ? data_remove_favorite(id) : data_add_favorite(id)) != 0) {
        QMessageBox::warning(this, QStringLiteral("收藏失败"), QStringLiteral("歌曲不存在，无法修改收藏。"));
        return;
    }
    if (data_save_favorites("data/favorites.txt") != 0) {
        // 保存未成功时，内存也恢复到操作之前，保证红心和文件内容一致。
        if (wasFavorite) data_add_favorite(id);
        else data_remove_favorite(id);
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("收藏未保存，本次操作已撤销。请检查 data 目录的写入权限，"
                           "以及是否存在未处理的 favorites.txt.tmp 文件。"));
        return;
    }
    refreshLists();
    status->setText(wasFavorite ? QStringLiteral("已取消收藏并保存") : QStringLiteral("已收藏并保存"));
}

void MusicWindow::showSongInfo(int index)
{
    if (index < 0 || index >= songCount) return;
    Song song; // 数据模块查询得到的歌曲信息副本。
    if (data_find_song(songs[index].id, &song) != 0) return;
    QMessageBox::information(this, QStringLiteral("歌曲信息"),
        QStringLiteral("编号：%1\n歌名：%2\n歌手：%3\n专辑：%4\n文件：%5")
            .arg(song.id).arg(QString::fromUtf8(song.title)).arg(QString::fromUtf8(song.artist))
            .arg(QString::fromUtf8(song.album)).arg(QString::fromUtf8(song.path)));
}

// ④ 播放区：进度条、上一首 / 播放 / 下一首、循环模式、音量
QWidget *MusicWindow::createControls()
{
    auto *panel = new QWidget(this);
    panel->setObjectName("panel");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(16);
    currentSong = new QLabel(QStringLiteral("从歌单选择一首歌曲"), panel);
    currentSong->setAlignment(Qt::AlignCenter);
    layout->addWidget(currentSong);
    auto *timeline = new QHBoxLayout;
    elapsed = new QLabel(QStringLiteral("00:00"), panel);
    duration = new QLabel(QStringLiteral("--:--"), panel);
    elapsed->setObjectName("muted");
    duration->setObjectName("muted");
    elapsed->setAccessibleName(QStringLiteral("当前播放时间"));
    duration->setAccessibleName(QStringLiteral("歌曲总时长"));
    progress = new QSlider(Qt::Horizontal, panel);
    progress->setObjectName("progress");
    progress->setRange(0, 0);
    progress->setEnabled(false); // 打开歌曲并取得时长后启用，数值单位为毫秒。
    timeline->addWidget(elapsed);
    timeline->addWidget(progress, 1);
    timeline->addWidget(duration);
    layout->addLayout(timeline);

    auto *buttons = new QHBoxLayout;
    auto *previous = new QPushButton(panel);
    auto *next = new QPushButton(panel);
    playButton = new QPushButton(panel);
    playButton->setObjectName("play");
    previous->setObjectName("previous");
    next->setObjectName("next");
    previous->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    next->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    previous->setFixedSize(40, 40);
    next->setFixedSize(40, 40);
    playButton->setFixedSize(56, 56);
    playButton->setIconSize(QSize(26, 26));
    previous->setToolTip(QStringLiteral("上一首"));
    next->setToolTip(QStringLiteral("下一首"));
    playButton->setToolTip(QStringLiteral("播放 / 暂停"));
    buttons->addStretch();
    buttons->addWidget(previous);
    buttons->addSpacing(16);
    buttons->addWidget(playButton);
    buttons->addSpacing(16);
    buttons->addWidget(next);
    buttons->addStretch();
    layout->addLayout(buttons);

    auto *options = new QHBoxLayout;
    modeButton = new QPushButton(QStringLiteral("顺序播放"), panel);
    modeButton->setObjectName("mode");
    modeButton->setFixedSize(90, 32);
    volume = new QSlider(Qt::Horizontal, panel);
    volume->setObjectName("volume");
    volume->setRange(0, 100);
    volume->setValue(70);
    volume->setFixedWidth(90);
    auto *volumeLabel = new QLabel(QStringLiteral("音量 70"), panel);
    volumeLabel->setObjectName("muted");
    options->addWidget(modeButton);
    options->addStretch();
    options->addWidget(volumeLabel);
    options->addWidget(volume);
    layout->addLayout(options);
    QObject::connect(playButton, &QPushButton::clicked, this, [this]() { togglePlay(); });
    QObject::connect(previous, &QPushButton::clicked, this, [this]() { changeSong(-1); });
    QObject::connect(next, &QPushButton::clicked, this, [this]() { changeSong(1); });
    QObject::connect(modeButton, &QPushButton::clicked, this, [this]() {
        if (!playerReady) return;
        int nextMode = (mode + 1) % 3; // 将要设置的播放模式。
        if (player_set_mode(static_cast<PlayerMode>(nextMode)) != 0) {
            showPlaybackError(QStringLiteral("无法设置播放模式。"));
            return;
        }
        mode = nextMode;
        const QString names[] = {QStringLiteral("顺序播放"), QStringLiteral("列表循环"), QStringLiteral("单曲循环")};
        modeButton->setText(names[mode]);
    });
    QObject::connect(volume, &QSlider::valueChanged, this, [this, volumeLabel](int value) {
        if (!playerReady || player_set_volume(value) != 0) {
            QSignalBlocker blocker(volume); // 恢复旧数值时不再次触发 valueChanged。
            volume->setValue(volumeValue);
            showPlaybackError(QStringLiteral("无法设置音量。"));
            return;
        }
        volumeValue = value;
        volumeLabel->setText(QStringLiteral("音量 %1").arg(value));
    });
    QObject::connect(progress, &QSlider::sliderMoved, this, [this](int value) {
        elapsed->setText(formatTime(value)); // 拖动时先预览目标时间。
    });
    QObject::connect(progress, &QSlider::sliderReleased, this, [this]() {
        if (!playerReady) return;
        int result = player_seek(progress->value()); // 松开滑块后跳转真实音频进度。
        updatePlaybackUi();
        if (result != 0) showPlaybackError(QStringLiteral("无法跳转到该播放位置。"));
    });
    return panel;
}

void MusicWindow::togglePlay()
{
    if (!playerReady || songCount == 0) return;
    if (selected < 0) selectSong(0);
    int result; // 本次播放、暂停或继续操作的返回值。
    bool sameSong = player_get_current_id() == songs[selected].id; // 设备是否打开了选中的歌曲。
    if (sameSong && player_get_state() == PLAYER_PLAYING)
        result = player_pause();
    else if (sameSong && player_get_state() == PLAYER_PAUSED)
        result = player_resume();
    else
        result = player_play(songs[selected].id);
    updatePlaybackUi();
    if (result != 0) {
        showPlaybackError(QStringLiteral("无法播放或暂停歌曲，请检查音频文件和系统音频设备。\n文件：%1")
            .arg(QFileInfo(QString::fromUtf8(songs[selected].path)).absoluteFilePath()));
        return;
    }
    playbackTimer->start();
}

void MusicWindow::changeSong(int direction)
{
    if (!playerReady || songCount == 0) return;
    int id; // B 返回的新歌曲编号，不是数组下标。
    if (selected < 0) {
        id = player_play(songs[0].id) == 0 ? songs[0].id : -1;
    } else {
        if (mode == PLAYER_ORDER && ((direction < 0 && selected == 0) ||
                                     (direction > 0 && selected == songCount - 1))) {
            status->setText(direction < 0 ? QStringLiteral("已经是第一首") : QStringLiteral("已经是最后一首"));
            return;
        }
        if (player_get_current_id() == songs[selected].id)
            id = direction < 0 ? player_previous() : player_next();
        else {
            // 手动选中了尚未播放的新歌，B 仍记着旧歌；此时按界面选择定位。
            int index = (selected + direction + songCount) % songCount; // 目标歌曲下标。
            id = player_play(songs[index].id) == 0 ? songs[index].id : -1;
        }
    }
    updatePlaybackUi(); // 只同步显示，不调用会停止音频的 selectSong()。
    if (id < 0) {
        showPlaybackError(QStringLiteral("切歌失败，请检查对应的音频文件。"));
        return;
    }
    playbackTimer->start();
}

// 从 B 查询状态和时间；自动切歌只同步显示，拖动滑块时不覆盖用户的位置。
void MusicWindow::updatePlaybackUi()
{
    if (!playerReady) return;
    PlayerState state = player_get_state(); // 当前真实播放状态。
    int id = player_get_current_id(); // 当前设备的歌曲编号。
    bool songChanged = false; // 是否发生了手动或自动切歌。
    if (state != PLAYER_STOPPED) {
        for (int i = 0; i < songCount; ++i) { // 查找歌曲编号对应的界面下标。
            if (songs[i].id == id && selected != i) {
                showSelectedSong(i);
                songChanged = true;
                break;
            }
        }
    }
    bool sameSong = selected >= 0 && songs[selected].id == id; // 避免显示上一首已停止音频的进度。
    int total = sameSong ? player_get_duration() : 0; // 总时长，毫秒。
    int position = sameSong ? player_get_position() : 0; // 当前进度，毫秒。
    total = qMax(0, total);
    position = qBound(0, position, total);
    progress->setEnabled(total > 0);
    if (!progress->isSliderDown()) {
        progress->setRange(0, total);
        progress->setValue(position);
        elapsed->setText(formatTime(position));
    }
    duration->setText(total > 0 ? formatTime(total) : QStringLiteral("--:--"));
    playButton->setIcon(style()->standardIcon(state == PLAYER_PLAYING ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    playButton->setToolTip(state == PLAYER_PLAYING ? QStringLiteral("暂停") : QStringLiteral("播放"));
    if (state != displayedState || songChanged) {
        status->setText(state == PLAYER_PLAYING ? QStringLiteral("正在播放")
            : state == PLAYER_PAUSED ? QStringLiteral("已暂停") : QStringLiteral("播放已停止"));
    }
    displayedState = state;
}

// message 是错误原因；统一显示提示，无返回值。
void MusicWindow::showPlaybackError(const QString &message)
{
    status->setText(QStringLiteral("播放操作失败"));
    QMessageBox::warning(this, QStringLiteral("播放失败"), message);
}

// ⑤ 程序入口，与笔记中的四步相同。
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    // Qt Creator 与双击启动使用相同的数据目录；分发时使用程序旁的 data 目录。
    QDir root(QApplication::applicationDirPath()); // 可执行文件所在目录。
    if (QFileInfo::exists(root.filePath("../../data/songs.txt"))) root.cd("../..");
    if (!QDir::setCurrent(root.absolutePath())) {
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"), QStringLiteral("无法进入数据所在目录。"));
        return 1;
    }
    MusicWindow window;
    window.show();
    return app.exec();
}
