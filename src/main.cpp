//hyf的

// 项目接口。
#include "main.h"
#include "data.h"
#include "player.h"

// Qt 应用和提示窗口。
#include <QApplication>
#include <QMessageBox>

// Qt 布局。
#include <QHBoxLayout>
#include <QVBoxLayout>

// Qt 文件和辅助工具。
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>
#include <QStyle>


// 将毫秒换成分:秒；t_ms 为毫秒数，返回时间文字。
static QString formatTime(int t_ms)
{
    if (t_ms < 0) t_ms = 0; // 时间不能小于零。
    int totalSeconds = t_ms / 1000;
    int minutes = totalSeconds / 60;        // 完整的分钟数。
    int seconds = totalSeconds % 60;        // 不足一分钟的剩余秒数。
    return QString::asprintf("%02d:%02d", minutes, seconds); // 不足两位时补 0。
}

// ① 创建三栏布局，再读取数据、初始化播放器和定时器。
MusicWindow::MusicWindow()
{
    setWindowTitle("Simple Music Player");
    setObjectName("window");
    resize(1200, 760);
    setMinimumSize(960, 600);

    setStyleSheet(QStringLiteral(R"(
        QWidget { color: #293448; font-family: 'Microsoft YaHei'; font-size: 13px; }
        QWidget#window { background: #f5f6fa; }
        QWidget#panel { background: white; border-radius: 14px; }
        QLabel#heading { font-size: 19px; font-weight: bold; }
        QLabel#muted { color: #8790a0; font-size: 11px; } /* 弱化文本 */
        /* 弹窗使用浅色背景和深色文字 */
        QMessageBox { background: white; }
        QMessageBox QLabel { color: #293448; background: transparent; }
        QMessageBox QPushButton { color: #293448; background: #edf0ff; min-width: 64px; padding: 6px 16px; }
        QMessageBox QPushButton:hover { background: #dfe5ff; }
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

    QVBoxLayout *root = new QVBoxLayout(this); //根布局
    //使用new运算符可以创建一个新的对象，并返回指向该对象的指针
    root->setContentsMargins(24, 20, 24, 24);
    root->setSpacing(18);
    QLabel *name = new QLabel("Simple Music Player", this);//文字，主窗口指针
    name->setObjectName("heading");
    root->addWidget(name);

    // 1 : 2 : 1，左右各约四分之一；中间上方留空，下方放播放控制。
    QHBoxLayout *columns = new QHBoxLayout;//无参构造函数。这里是嵌套布局。要么直接装到主窗口，要么后面嵌套进去
    columns->setSpacing(18);
    QWidget *left = createListPanel("歌单", false);//后面创建的，歌单与收藏夹我们用同一个函数，只是判断一下是不是
    left->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    QWidget *right = createListPanel("收藏夹", true);
    right->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    QWidget *center = new QWidget(this);
    QVBoxLayout *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->addStretch();//插入会伸缩的空白
    centerLayout->addWidget(createControls());
    
    center->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
   
    columns->addWidget(left, 1);
    columns->addWidget(center, 2);
    columns->addWidget(right, 1);//拉伸因子

    root->addLayout(columns, 1);//name是0，columns占大部分

    loadData();//加载歌曲数据和刷新列表
    playerReady = player_init() == 0 && player_set_playlist(songs, songCount) == 0;
    playbackTimer = new QTimer(this);
    playbackTimer->setInterval(200);
    //建立信号与回调的连接（发出信号的对象，定时器的超时信号，接收上下文，也就是当前窗口，收到信号后执行的 Lambda 函数）
    QObject::connect(playbackTimer, &QTimer::timeout, this, [this]() {
        if (player_update() != 0) {
            // 查询失败时暂停轮询，避免每 200ms 重复弹报错窗口
            playbackTimer->stop();
            updatePlaybackUi();
            showPlaybackError("播放中断，请重试。");
            return;
        }
        updatePlaybackUi();
    });
    if (playerReady) playbackTimer->start();
    else {
        playButton->setEnabled(false);
        QTimer::singleShot(0, this, [this]() {//只触发一次
            showPlaybackError("播放器初始化失败。");
        });
    }
}

// 退出窗口时释放播放资源，无参数、无返回值。
MusicWindow::~MusicWindow()
{
    playbackTimer->stop();
    player_close();
    //公开继承qwidegt的虚析构函数，我们重写一下
}

// 读取歌单和收藏，失败时提示。songcount是实际加载的歌曲数
//把底层的函数再包装一下弄到界面里
void MusicWindow::loadData()
{
    if (data_load("data/songs.txt", "data/favorites.txt") == 0)//成功返回0
        songCount = data_get_songs(songs, MAX_SONGS);//songs是交互准备的songs数组
    else
        songCount = -1;
    if (songCount < 0) {
        songCount = 0;
        QTimer::singleShot(0, this, [this]() {
            QMessageBox::warning(this, "读取失败",//对话框标题
                "歌单或收藏读取失败。");//提示内容
        });
    }
    refreshLists();
}

// title 为标题，favoritePanel 指定是否为收藏列表；返回创建的面板。
QWidget *MusicWindow::createListPanel(QString title, bool favoritePanel)
/*
    创建一个完整的列表面板，包含标题、歌曲数量和歌曲列表，最后把这个面板返回。
    */
{//先创建布局，两个统一的
    QWidget *panel = new QWidget(this);
    panel->setObjectName("panel");
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 20, 10, 12);
    layout->setSpacing(12);
    QLabel *heading = new QLabel(title, panel);
    heading->setObjectName("heading");
    QLabel *count = new QLabel(panel);
    count->setObjectName("muted");

    QListWidget *list = new QListWidget(panel);//创建列表控件。临时保存本次创建的列表对象，之后要转给具体两侧的列表控件
    list->setObjectName(favoritePanel ? "favoriteList" : "songList");
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);//按像素滚动，而不是按整行滚动
    list->setResizeMode(QListView::Adjust); // 列表尺寸变化时，重新调整列表项的布局

    layout->addWidget(heading);
    layout->addWidget(count);
    layout->addWidget(list, 1);
    if (favoritePanel) {
        //判断当前创建的是哪个面板，把对应的控件地址保存到正确的成员变量里。
        //之后还要调用呢
        favoriteList = list;
        favoriteCount = count;
    } else {
        songList = list;
        songCountLabel = count;
    }
    //发出信号的列表控件，用户点击列表项时发出的信号，当前 MusicWindow，作为回调上下文，收到信号后执行的 Lambda
    QObject::connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        selectSong(item->data(Qt::UserRole).toInt()); //取下标
    });
    return panel;
}

// 在 list 中显示 songs[index]；isFavorite 决定爱心外观
// list 是要添加歌曲的列表，index 是 songs 数组下标，isFavorite 表示是否已收藏；无返回值。
//两个列表分别有index，每首歌都有userrole
void MusicWindow::addSongRow(QListWidget *list, int index, bool isFavorite) // 创建一首歌曲对应的列表行。
{//水平layout，竖直text
    QListWidgetItem *item = new QListWidgetItem(list); // 创建列表项，并把它加入 list。
    //Qt::UserRole 是 Qt 预定义的枚举值
    item->setData(Qt::UserRole, index); // 存下标
    item->setSizeHint(QSize(0, 68)); // 行高建议
    QWidget *row = new QWidget; // 创建容器，放这一行的歌名、歌手和按钮。
    QHBoxLayout *layout = new QHBoxLayout(row); // 给 row 设置水平布局，让文字和按钮左右排列。
    layout->setContentsMargins(8, 8, 2, 8); 
    layout->setSpacing(2); 

    QVBoxLayout *text = new QVBoxLayout; // 歌名、歌手的垂直布局
    text->setSpacing(5); 

    QLabel *title = new QLabel(QString::fromUtf8(songs[index].title), row); // 把 UTF-8 歌名转成 Qt 字符串，创建歌名标签。
    QLabel *artist = new QLabel(QString::fromUtf8(songs[index].artist), row); 
    artist->setObjectName("muted"); 
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // 宽度忽略控件建议的尺寸，交给布局分配空间；优先采用控件建议的尺寸，必要时允许调整
    artist->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); 
    //setAttribute(属性, 是否启用);默认启用。这里鼠标事件忽略它们
    title->setAttribute(Qt::WA_TransparentForMouseEvents); 
    artist->setAttribute(Qt::WA_TransparentForMouseEvents); 
    text->addWidget(title); 
    text->addWidget(artist); 
    layout->addLayout(text, 1);
  
    QPushButton *heart = new QPushButton(isFavorite ? "♥" : "♡", row); 
    heart->setObjectName("heart"); 
    heart->setFixedSize(32, 36); 
    heart->setProperty("favorite", isFavorite); // 保存收藏属性，QSS 根据它决定是否显示红色。
    heart->setToolTip(isFavorite ? "取消收藏" : "收藏"); 

    QPushButton *more = new QPushButton("⋯", row); 
    more->setObjectName("more"); 
    more->setFixedSize(26, 36);
    more->setToolTip("歌曲信息"); 
    layout->addWidget(heart); 
    layout->addWidget(more); 
    list->setItemWidget(item, row); // 用做好的 row 作为这个列表项的显示内容，交给列表管理。
    QObject::connect(heart, &QPushButton::clicked, this, 
        [this, index]() { onFavoriteClicked(index); }); // 记住当前窗口和歌曲下标，点击后调用收藏处理函数。
    QObject::connect(more, &QPushButton::clicked, this, [this, index]() { showSongInfo(index); }); // 点击三个点时，把这首歌的下标交给信息显示函数。
    if (index == selected) 
        list->setCurrentItem(item); 
} 

// 从 A 查询收藏状态，刷新两侧列表并保留选择和滚动位置，无参数、无返回值。
void MusicWindow::refreshLists()
{
    int songScroll = songList->verticalScrollBar()->value(); // 先保存滚动条当前的位置数值。
    int favoriteScroll = favoriteList->verticalScrollBar()->value(); 
    int count = 0; // 当前收藏歌曲数。
    //刷新的时候，全部清空再重新创建
    songList->clear();
    favoriteList->clear();
    for (int i = 0; i < songCount; ++i) { // 当前歌曲下标。
        bool favorite = data_is_favorite(songs[i].id) == 1; // 由 A 查询收藏状态。
        addSongRow(songList, i, favorite);
        if (favorite) {
            addSongRow(favoriteList, i, true);
            ++count;
        }
    }
    songCountLabel->setText(QString::asprintf("%d 首歌曲", songCount));
    favoriteCount->setText(QString::asprintf("%d 首收藏", count));
    songList->doItemsLayout();//立即更新界面
    favoriteList->doItemsLayout();
    songList->verticalScrollBar()->setValue(songScroll);
    favoriteList->verticalScrollBar()->setValue(favoriteScroll);
}

// index 为歌曲下标；只更新当前歌曲和两侧选择，无返回值。
void MusicWindow::showSelectedSong(int index)
//更新中间播放面板显示的歌曲名称和歌手。同步左右两个列表的选中状态。
{
    selected = index;
    currentSong->setText(QString::fromUtf8(songs[index].title) + " · " +
                         QString::fromUtf8(songs[index].artist));
    QListWidget *lists[] = {songList, favoriteList}; // 定义了一个指针数组，里面放了两个 QListWidget 指针。
    for (int k = 0; k < 2; ++k) { // 分别更新歌单和收藏夹。
        QListWidget *list = lists[k]; // 本次处理的列表。
        list->setCurrentRow(-1);
        for (int i = 0; i < list->count(); ++i) // 查找对应的列表行。
            if (list->item(i)->data(Qt::UserRole).toInt() == index)
                list->setCurrentRow(i);
    }
}

// 给我一个歌曲 id，我帮你找到这首歌在数组里的 index
int MusicWindow::findSongIndex(int id)
{
    for (int i = 0; i < songCount; ++i) { // 当前检查的歌单下标。
        if (songs[i].id == id) return i;
    }
    return -1;
}

// 选中 index 对应的歌曲，停止旧歌曲
void MusicWindow::selectSong(int index)
{
    if (index < 0 || index >= songCount) return;
    if (index == selected) return; // 点击正在播放的同一首，不中断音频。
    if (playerReady && player_get_current_id() >= 0 && player_stop() != 0) {
        showPlaybackError("停止失败，请重试。");
        return;
    }
    showSelectedSong(index);
    updatePlaybackUi();
}

// ③ index 为歌曲下标；调用 A 增删并保存收藏，失败回滚
void MusicWindow::onFavoriteClicked(int index)
{
    if (index < 0 || index >= songCount) return;
    int id = songs[index].id; // 数据接口使用的歌曲编号。
    int wasFavorite = data_is_favorite(id); // 用于切换收藏及保存失败后的回滚。
    int result = -1; // 增删操作的结果，-1 表示失败。
    if (wasFavorite == 1) result = data_remove_favorite(id);
    else if (wasFavorite == 0) result = data_add_favorite(id);
    if (result != 0) {
        QMessageBox::warning(this, "收藏失败", "无法修改收藏。");
        return;
    }
    if (data_save_favorites("data/favorites.txt") != 0) {
        // 保存未成功时，内存也恢复到操作之前，保证红心和文件内容一致。
        if (wasFavorite) data_add_favorite(id);
        else data_remove_favorite(id);
        QMessageBox::warning(this, "保存失败", "收藏未保存，本次操作已撤销。");
        return;
    }
    // 点击事件结束后再刷新，避免在按钮的回调中销毁按钮自身。
    QTimer::singleShot(0, this, &MusicWindow::refreshLists);
}

// 显示 index 对应的完整歌曲信息
void MusicWindow::showSongInfo(int index)
{
    if (index < 0 || index >= songCount) return;
    Song song; // 数据模块查询得到的歌曲信息副本。
    if (data_find_song(songs[index].id, &song) != 0) return;
    QMessageBox::information(this, "歌曲信息",
        QString::asprintf("编号：%d\n歌名：%s\n歌手：%s\n专辑：%s\n文件：%s",
                          song.id, song.title, song.artist, song.album, song.path));
}

// ④ 创建原来的播放控制区：进度、播放图标、循环模式和音量，返回面板。
QWidget *MusicWindow::createControls()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName("panel");
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(16);
    currentSong = new QLabel("从歌单选择一首歌曲", panel);
    currentSong->setAlignment(Qt::AlignCenter);//中对齐
    layout->addWidget(currentSong);
    QHBoxLayout *timeline = new QHBoxLayout;
    elapsed = new QLabel("00:00", panel);
    duration = new QLabel("--:--", panel);
    elapsed->setObjectName("muted");
    progress = new QSlider(Qt::Horizontal, panel);
    progress->setObjectName("progress");
    progress->setRange(0, 0);
    progress->setEnabled(false); // 打开歌曲并取得时长后启用，数值单位为毫秒。
    timeline->addWidget(elapsed);
    timeline->addWidget(progress, 1);
    timeline->addWidget(duration);
    layout->addLayout(timeline);

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *previous = new QPushButton(panel);
    QPushButton *next = new QPushButton(panel);
    playButton = new QPushButton(panel);
    playButton->setObjectName("play");
    previous->setObjectName("previous");
    next->setObjectName("next");
    //从 Qt 的标准图标中取得播放图标，然后设置到 playButton 按钮上
    previous->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    next->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    previous->setFixedSize(40, 40);
    next->setFixedSize(40, 40);
    playButton->setFixedSize(56, 56);
    playButton->setIconSize(QSize(26, 26));
    previous->setToolTip("上一首");
    next->setToolTip("下一首");
    playButton->setToolTip("播放 / 暂停");
    buttons->addStretch();
    buttons->addWidget(previous);
    buttons->addSpacing(16);
    buttons->addWidget(playButton);
    buttons->addSpacing(16);
    buttons->addWidget(next);
    buttons->addStretch();
    layout->addLayout(buttons);

    QHBoxLayout *options = new QHBoxLayout;
    modeButton = new QPushButton("顺序播放", panel);
    modeButton->setObjectName("mode");
    modeButton->setFixedSize(90, 32);
    volume = new QSlider(Qt::Horizontal, panel);
    volume->setObjectName("volume");
    volume->setRange(0, 100);
    volume->setValue(70);
    volume->setFixedWidth(90);
    QLabel *volumeLabel = new QLabel("音量 70", panel);
    volumeLabel->setObjectName("muted");
    options->addWidget(modeButton);
    options->addStretch();
    options->addWidget(volumeLabel);
    options->addWidget(volume);
    layout->addLayout(options);
    QObject::connect(playButton, &QPushButton::clicked, this, &MusicWindow::togglePlay);
    QObject::connect(previous, &QPushButton::clicked, this, [this]() { changeSong(-1); });
    QObject::connect(next, &QPushButton::clicked, this, [this]() { changeSong(1); });
    QObject::connect(modeButton, &QPushButton::clicked, this, [this]() {
        if (!playerReady) return;
        int nextMode = (mode + 1) % 3; // 将要设置的播放模式。
        if (player_set_mode((PlayerMode)nextMode) != 0) {
            showPlaybackError("模式设置失败。");
            return;
        }
        mode = nextMode;
        const QString names[] = {"顺序播放", "列表循环", "单曲循环"};
        modeButton->setText(names[mode]);
    });
    QObject::connect(volume, &QSlider::valueChanged, this, [this, volumeLabel](int value) {
        if (!playerReady || player_set_volume(value) != 0) {
            volume->blockSignals(true); // 恢复旧值时不再次触发本回调。
            volume->setValue(volumeValue);
            volume->blockSignals(false);
            showPlaybackError("音量设置失败。");
            return;
        }
        volumeValue = value;
        volumeLabel->setText(QString::asprintf("音量 %d", value));
    });
    QObject::connect(progress, &QSlider::sliderMoved, this, [this](int value) {
        elapsed->setText(formatTime(value)); // 拖动时先预览目标时间。
    });
    QObject::connect(progress, &QSlider::sliderReleased, this, [this]() {
        if (!playerReady) return;
        int result = player_seek(progress->value()); // 松开滑块后跳转真实音频进度。
        updatePlaybackUi();
        if (result != 0) showPlaybackError("进度跳转失败。");
    });
    return panel;
}

// ④ 根据 B 的真实状态播放、暂停或继续
void MusicWindow::togglePlay()
{
    if (!playerReady || songCount == 0) return;
    if (selected < 0) selectSong(0);
    int result; // 本次播放、暂停或继续操作的返回值。
    PlayerState state = player_get_state(); // 查询一次实际状态，决定调用哪个接口。
    bool sameSong = player_get_current_id() == songs[selected].id; // 设备是否打开了选中的歌曲。
    if (sameSong && state == PLAYER_PLAYING)
        result = player_pause();
    else if (sameSong && state == PLAYER_PAUSED)
        result = player_resume();
    else
        result = player_play(songs[selected].id);
    updatePlaybackUi();
    if (result != 0) {
        showPlaybackError("播放或暂停失败，请检查音频文件。");
        return;
    }
    playbackTimer->start();
}

// direction 为 -1 上一首、1 下一首，无返回值。
void MusicWindow::changeSong(int direction)
{
    if (!playerReady || songCount == 0) return;
    // 计算目标歌曲下标
    int index = selected < 0 ? 0 : selected + direction;
    // 第一首还往上，或者最后一首还往下的情况
    if (index < 0 || index >= songCount) {
        if (mode == PLAYER_ORDER)
            return;
        index = index < 0 ? songCount - 1 : 0;
        //左边是index<0,右边是index>=songcount的情况。一往前，最后往后。
    }
    //播放正确处理切歌后的目标歌曲
    int result = player_play(songs[index].id);
    updatePlaybackUi();

    if (result != 0) {
        showPlaybackError("切歌失败，请检查音频文件。");
        return;
    }
    playbackTimer->start();
}

// 定时读取 B 的歌曲、状态和进度并显示，拖动时不覆盖滑块；无参数、无返回值。
void MusicWindow::updatePlaybackUi()
{
    if (!playerReady) return;
    PlayerState state = player_get_state(); // 当前真实播放状态。
    int index = findSongIndex(player_get_current_id()); // 当前播放歌曲的界面下标。
    if (state != PLAYER_STOPPED && index >= 0 && selected != index)
        showSelectedSong(index);// 播放器切换了歌曲且未停止时，同步界面的当前歌曲。
    bool sameSong = selected >= 0 && selected == index; // 界面选中的歌曲，和播放器实际打开的歌曲是否相同。
    int total = sameSong ? player_get_duration() : 0; 
    int position = sameSong ? player_get_position() : 0; 
    if (total < 0) total = 0;
    if (position < 0) position = 0;
    if (position > total) position = total;
    progress->setEnabled(total > 0);// 总时长大于零（是有效时长）时才启用滑块。
    if (!progress->isSliderDown()) {//只有用户没有正在拖动滑块时，才允许程序自动更新进度条。
        progress->setRange(0, total);//设置滑块允许的数值范围
        progress->setValue(position);
        elapsed->setText(formatTime(position));
    }
    if (total > 0) duration->setText(formatTime(total));
    else duration->setText("--:--");
    if (state == PLAYER_PLAYING) {
        playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        playButton->setToolTip("暂停");
    } else {
        playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        playButton->setToolTip("播放");
    }
}

// message 为错误原因，显示播放错误弹窗，无返回值。
void MusicWindow::showPlaybackError(QString message)
{
    QMessageBox::warning(this, "播放失败", message);
}

// ⑤ 程序入口：创建应用、窗口并进入事件循环；返回退出状态码。
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);//要接收参数
    app.setStyle("Fusion");//Fusion 是 Qt 内置的一套界面风格，会影响按钮、滑块、滚动条等控件的默认外观。
    MusicWindow window;//创建对象，构造函数前面写过了
    window.show();//调用从 QWidget 继承来的 show() 成员函数。
    return app.exec();
}
