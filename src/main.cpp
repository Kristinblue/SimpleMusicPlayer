#include <QApplication>
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
#include "player.h"

// 小类只用来把控件、界面状态和回调放在一起，全部 Qt 代码都在本文件。
// 使用已有信号连接 lambda，不需要自定义信号、Q_OBJECT 或 .ui 文件。
class MusicWindow : public QWidget {
public:
    MusicWindow();

private:
    // 临时示例数据：A 完成后，用 data_get_songs() 替换。没有对应音频。
    Song songs[MAX_SONGS] = {
        {1, "晴天", "示例歌手 A", "夏日", "music/01.wav"},
        {2, "夜空", "示例歌手 B", "夜航", "music/02.wav"},
        {3, "山间的风", "示例歌手 C", "远方", "music/03.wav"},
        {4, "慢慢走", "示例歌手 A", "夏日", "music/04.wav"},
        {5, "雨后", "示例歌手 D", "四季", "music/05.wav"},
        {6, "海岸线", "示例歌手 B", "夜航", "music/06.wav"},
        {7, "晚风", "示例歌手 C", "远方", "music/07.wav"},
        {8, "午后", "示例歌手 D", "四季", "music/08.wav"},
        {9, "星光", "示例歌手 B", "夜航", "music/09.wav"},
        {10, "旅途", "示例歌手 C", "远方", "music/10.wav"},
        {11, "小城", "示例歌手 A", "夏日", "music/11.wav"},
        {12, "初雪", "示例歌手 D", "四季", "music/12.wav"},
        {13, "回声", "示例歌手 C", "远方", "music/13.wav"},
        {14, "晨光", "示例歌手 A", "夏日", "music/14.wav"}
    };
    int songCount = 14;
    bool favorites[MAX_SONGS] = {false};
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
    void addSongRow(QListWidget *list, int index); // 两个列表共用
    void refreshLists();
    void selectSong(int index);
    void toggleFavorite(int index);
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
    favorites[1] = favorites[4] = favorites[8] = true;

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
        QPushButton#heart:checked { color: #e55370; }
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
    status = new QLabel(QStringLiteral("界面预览 · 尚未接入音频和文件保存"), this);
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
    refreshLists();

    // TODO A：data_load("data/songs.txt", "data/favorites.txt");
    //         songCount = data_get_songs(songs, MAX_SONGS);
    //         从 data_is_favorite() 取得收藏状态，再 refreshLists()。
    // TODO B：player_init(); player_set_playlist(songs, songCount);
    //         用 QTimer 每 200ms 调 player_update()，读取真实状态、进度、歌曲 ID。
    //         程序关闭时调用 player_close()。
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

void MusicWindow::addSongRow(QListWidget *list, int index)
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
    auto *heart = new QPushButton(favorites[index] ? QStringLiteral("♥") : QStringLiteral("♡"), row);
    heart->setObjectName("heart");
    heart->setFixedSize(32, 36);
    heart->setCheckable(true);
    heart->setChecked(favorites[index]);
    heart->setToolTip(favorites[index] ? QStringLiteral("取消收藏") : QStringLiteral("收藏"));
    auto *more = new QPushButton(QStringLiteral("⋯"), row);
    more->setObjectName("more");
    more->setFixedSize(26, 36);
    more->setToolTip(QStringLiteral("歌曲信息"));
    layout->addWidget(heart);
    layout->addWidget(more);
    list->setItemWidget(item, row);
    QObject::connect(heart, &QPushButton::clicked, this, [this, index]() { toggleFavorite(index); });
    QObject::connect(more, &QPushButton::clicked, this, [this, index]() { showSongInfo(index); });
    if (index == selected)
        list->setCurrentItem(item);
}

void MusicWindow::refreshLists()
{
    // 刷新时保留滚动位置，点击收藏不会把歌单跳回顶部。
    int songScroll = songList->verticalScrollBar()->value();
    int favoriteScroll = favoriteList->verticalScrollBar()->value();
    songList->clear();
    favoriteList->clear();
    int count = 0;
    for (int i = 0; i < songCount; ++i) {
        addSongRow(songList, i);
        if (favorites[i]) {
            addSongRow(favoriteList, i);
            ++count;
        }
    }
    favoriteCount->setText(QStringLiteral("%1 首收藏%2").arg(count)
        .arg(count == 0 ? QStringLiteral(" · 点击 ♡ 添加") : QString()));
    songCountLabel->setText(QStringLiteral("%1 首歌曲").arg(songCount));
    songList->doItemsLayout();
    favoriteList->doItemsLayout();
    songList->verticalScrollBar()->setValue(songScroll);
    favoriteList->verticalScrollBar()->setValue(favoriteScroll);
}

// ③ 歌曲选择、收藏和信息按钮
void MusicWindow::selectSong(int index)
{
    selected = index;
    playing = false;
    currentSong->setText(QStringLiteral("%1 · %2")
        .arg(QString::fromUtf8(songs[index].title)).arg(QString::fromUtf8(songs[index].artist)));
    playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    status->setText(QStringLiteral("界面预览 · 尚未接入音频和文件保存"));
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

void MusicWindow::toggleFavorite(int index)
{
    // TODO A：data_add_favorite / data_remove_favorite 成功后再改界面；
    //         data_save_favorites 保存。现在只是预览，退出不保存。
    favorites[index] = !favorites[index];
    // 下一轮事件循环再刷新，避免在按钮的点击回调里销毁按钮自身。
    QTimer::singleShot(0, this, [this]() { refreshLists(); });
}

void MusicWindow::showSongInfo(int index)
{
    const Song &song = songs[index];
    // TODO A：需要重新查询时，用 data_find_song(song.id, &result)。
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
                            : QStringLiteral("界面预览 · 尚未接入音频和文件保存"));
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
    MusicWindow window;
    window.show();
    return app.exec();
}
