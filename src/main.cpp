#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "data.h"
#include "song.h"

// 小类只用来把控件、界面状态和回调放在一起，全部 Qt 代码都在本文件。
// 使用已有信号连接 lambda，不需要自定义信号、Q_OBJECT 或 .ui 文件。
class MusicWindow : public QWidget {
public:
    MusicWindow();

private:
    Song songs[MAX_SONGS] = {}; // 数据模块返回的歌曲副本，用于显示。
    int songCount = 0; // 实际从歌单文件加载的歌曲数。
    int selected = -1; // 数组下标；传给 A/B 时使用 songs[selected].id。
    bool playing = false;
    int mode = 0;

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

    QWidget *createListPanel(const QString &title, bool favoritePanel);
    QWidget *createControls();
    void addSongRow(QListWidget *list, int index, bool isFavorite = false); // 只负责显示
    void refreshLists();
    void loadData(); // 读取文件并刷新界面；失败时显示错误，无参数、无返回值。
    void selectSong(int index);
    void onFavoriteClicked(int index);
    void showSongInfo(int index);
    void togglePlay();
    void changeSong(int direction);
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
    status = new QLabel(QStringLiteral("收藏自动保存 · 音频尚未接入"), this);
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
    // TODO B：player_init(); player_set_playlist(songs, songCount);
    //         用 QTimer 每 200ms 调 player_update()，读取真实状态、进度、歌曲 ID。
    //         程序关闭时调用 player_close()。
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
    selected = index;
    playing = false;
    currentSong->setText(QStringLiteral("%1 · %2")
        .arg(QString::fromUtf8(songs[index].title)).arg(QString::fromUtf8(songs[index].artist)));
    playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    status->setText(QStringLiteral("收藏自动保存 · 音频尚未接入"));
    // 同步两个列表的选中行，不重新创建控件。
    QListWidget *lists[] = {songList, favoriteList};
    for (QListWidget *list : lists) {
        list->setCurrentRow(-1);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toInt() == index)
                list->setCurrentRow(i);
    }
    // TODO B：如旧歌正在播放，先 player_stop()；选中不等于立即播放。
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
    status->setText(wasFavorite ? QStringLiteral("已取消收藏并保存 · 音频尚未接入")
                                : QStringLiteral("已收藏并保存 · 音频尚未接入"));
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
    progress = new QSlider(Qt::Horizontal, panel);
    progress->setObjectName("progress");
    progress->setRange(0, 100);
    progress->setEnabled(false); // B 提供真实时长后启用，范围改为毫秒。
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
    playButton->setToolTip(QStringLiteral("播放 / 暂停（界面预览）"));
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
    auto *volume = new QSlider(Qt::Horizontal, panel);
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
        mode = (mode + 1) % 3;
        const QString names[] = {QStringLiteral("顺序播放"), QStringLiteral("列表循环"), QStringLiteral("单曲循环")};
        modeButton->setText(names[mode]);
        // TODO B：player_set_mode(static_cast<PlayerMode>(mode));
    });
    QObject::connect(volume, &QSlider::valueChanged, this, [volumeLabel](int value) {
        volumeLabel->setText(QStringLiteral("音量 %1").arg(value));
        // TODO B：player_set_volume(value);
    });
    QObject::connect(progress, &QSlider::sliderReleased, this, [this]() {
        // TODO B：player_seek(progress->value());
        // 更新进度时，用户正在拖动（isSliderDown）则暂不覆盖滑块位置。
    });
    return panel;
}

void MusicWindow::togglePlay()
{
    if (songCount == 0) return;
    if (selected < 0) selectSong(0);
    // TODO B：未开始时 player_play(songs[selected].id)；
    //         播放中 player_pause()；暂停后 player_resume()。
    //         用 player_get_state() 判断真实状态，成功后再更新图标。
    playing = !playing;
    playButton->setIcon(style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    status->setText(playing ? QStringLiteral("播放按钮预览 · 当前没有音频输出")
                            : QStringLiteral("收藏自动保存 · 音频尚未接入"));
}

void MusicWindow::changeSong(int direction)
{
    if (songCount == 0) return;
    // 预览只改变选择；真正切歌和边界规则由 B 决定。
    int index = selected < 0 ? 0 : selected + direction;
    if (index < 0) index = songCount - 1;
    if (index >= songCount) index = 0;
    selectSong(index);
    // TODO B：player_previous / player_next 返回歌曲 ID，转换为下标再显示。
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
