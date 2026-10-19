// 用真实窗口和播放模块测试接线；静音 WAV 与歌单都在临时目录。
#define main music_player_entry
#include "../src/main.cpp"
#undef main

#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <functional>

// condition 为检查条件，message 为失败原因；失败时终止测试，无返回值。
static void check(bool condition, const char *message)
{
    if (!condition) qFatal("%s", message);
}

// 等待 condition 成立，最多四秒；期间处理 Qt 事件，成功返回 true。
static bool waitFor(const std::function<bool()> &condition)
{
    QElapsedTimer timer; // 限制测试等待时间，避免窗口卡住时无限循环。
    timer.start();
    while (!condition() && timer.elapsed() < 4000) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return condition();
}

// 等待 milliseconds 毫秒且持续处理界面事件，无返回值。
static void processFor(int milliseconds)
{
    QElapsedTimer timer; // 让界面定时器有机会触发。
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
}

// path 为临时 WAV 路径；生成 15 秒单声道静音 PCM，用真实音频设备测试但不发声。
static void writeWave(const QString &path)
{
    QFile file(path); // 自动关闭的临时音频文件。
    check(file.open(QIODevice::WriteOnly), "open WAV fixture");
    QByteArray pcm(8000 * 15 * 2, '\0'); // 8kHz、16 位、15 秒静音采样。
    QDataStream output(&file); // 用小端格式写 WAV 头。
    output.setByteOrder(QDataStream::LittleEndian);
    output.writeRawData("RIFF", 4);
    output << quint32(36 + pcm.size());
    output.writeRawData("WAVEfmt ", 8);
    output << quint32(16) << quint16(1) << quint16(1) << quint32(8000)
           << quint32(16000) << quint16(2) << quint16(16);
    output.writeRawData("data", 4);
    output << quint32(pcm.size());
    check(output.writeRawData(pcm.constData(), pcm.size()) == pcm.size(), "write WAV samples");
    check(output.status() == QDataStream::Ok, "write WAV header");
}

// text 为歌单内容；仅修改测试临时目录中的文件，无返回值。
static void writeSongs(const QByteArray &text)
{
    QFile file("data/songs.txt"); // 测试歌单。
    check(file.open(QIODevice::WriteOnly), "open test playlist");
    check(file.write(text) == text.size(), "write test playlist");
}

// argc/argv 交给 Qt；成功返回 0，失败立即报告，不修改用户歌单或收藏。
int main(int argc, char **argv)
{
    QApplication app(argc, argv); // 测试真实窗口所需的应用。
    QTemporaryDir temporary; // 自动清理的音频、歌单和收藏目录。
    QString previous = QDir::currentPath(); // 测试后恢复的工作目录。
    check(temporary.isValid() && QDir::setCurrent(temporary.path()), "temporary directory");
    check(QDir().mkpath("data") && QDir().mkpath(QStringLiteral("音频")), "fixture directories");
    writeWave(QStringLiteral("音频/第一首.wav"));
    writeWave(QStringLiteral("音频/第二首.wav"));
    writeSongs("20|第一首|测试|测试|音频/第一首.wav\n3|第二首|测试|测试|音频/第二首.wav\n");
    {
        MusicWindow window; // 非连续编号与中文路径的真实窗口。
        auto *songs = window.findChild<QListWidget *>("songList"); // 歌单列表。
        auto *play = window.findChild<QPushButton *>("play"); // 播放暂停按钮。
        auto *next = window.findChild<QPushButton *>("next"); // 下一首按钮。
        auto *previousButton = window.findChild<QPushButton *>("previous"); // 上一首按钮。
        auto *mode = window.findChild<QPushButton *>("mode"); // 循环模式按钮。
        auto *progress = window.findChild<QSlider *>("progress"); // 真实进度滑块。
        auto *volume = window.findChild<QSlider *>("volume"); // 音量滑块。
        check(songs->count() == 2 && !progress->isEnabled(), "initial controls");
        songs->itemClicked(songs->item(0));
        play->click();
        check(player_get_state() == PLAYER_PLAYING && player_get_current_id() == 20, "play button starts audio");
        check(progress->isEnabled() && progress->maximum() >= 14000, "real duration enables progress");
        check(waitFor([progress]() { return progress->value() >= 200; }), "timer advances visible progress");
        play->click();
        int paused = player_get_position(); // 暂停时的真实音频位置。
        check(player_get_state() == PLAYER_PAUSED, "pause button");
        processFor(350);
        check(qAbs(player_get_position() - paused) < 80, "pause freezes audio position");
        play->click();
        check(waitFor([paused]() { return player_get_position() >= paused + 150; }), "resume continues audio");

        progress->setSliderDown(true);
        progress->setValue(5000);
        processFor(350);
        check(progress->value() == 5000, "timer does not overwrite dragging");
        progress->setSliderDown(false); // 自动发出 sliderReleased，触发真实 seek。
        check(player_get_position() >= 4900, "slider release seeks audio");
        volume->setValue(35);
        check(volume->value() == 35, "volume command succeeds");

        previousButton->click();
        check(player_get_current_id() == 20 && player_get_state() == PLAYER_PLAYING, "order mode keeps first song at boundary");
        next->click();
        check(player_get_current_id() == 3 && songs->currentRow() == 1, "next maps song ID to UI row");
        check(player_get_state() == PLAYER_PLAYING, "UI refresh does not stop next song");
        next->click();
        check(player_get_current_id() == 3, "order mode keeps last song at boundary");
        previousButton->click();
        check(player_get_current_id() == 20 && songs->currentRow() == 0, "previous button");
        songs->itemClicked(songs->item(1));
        check(player_get_state() == PLAYER_STOPPED && songs->currentRow() == 1, "selecting another song stops old audio");
        processFor(300);
        check(songs->currentRow() == 1 && !progress->isEnabled(), "timer preserves an unplayed selection");
        previousButton->click();
        check(player_get_current_id() == 20 && songs->currentRow() == 0, "previous uses unplayed UI selection");
        songs->itemClicked(songs->item(1));
        play->click();
        check(player_get_current_id() == 3 && player_get_state() == PLAYER_PLAYING, "play uses newly selected song");

        check(player_seek(player_get_duration() - 100) == 0, "seek near end in order mode");
        check(waitFor([]() { return player_get_state() == PLAYER_STOPPED; }), "order mode stops after last song");
        mode->click(); // 列表循环：从最后一首自动回到第一首。
        play->click();
        check(player_seek(player_get_duration() - 100) == 0, "seek near end in list loop");
        check(waitFor([songs]() { return player_get_current_id() == 20 && songs->currentRow() == 0; }), "automatic list loop updates selected row");
        check(player_get_state() == PLAYER_PLAYING, "automatic next remains playing");
        mode->click(); // 单曲循环：编号不变，但真实进度回到开头。
        check(player_seek(player_get_duration() - 100) == 0, "seek near end in single loop");
        check(waitFor([]() { return player_get_current_id() == 20 && player_get_position() < 1000; }), "single loop restarts audio");
        processFor(250);
        check(progress->value() < 1500, "loop resets visible progress");
    }
    check(player_get_current_id() == -1 && player_get_state() == PLAYER_STOPPED, "window destructor releases audio");
    writeSongs("42|不存在|测试|测试|missing.wav\n");
    {
        MusicWindow missing; // 缺失音频必须提示失败，不能假装正在播放。
        QTimer::singleShot(20, []() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); // 真实失败弹窗。
            check(box && box->windowTitle() == QStringLiteral("播放失败"), "missing audio error dialog");
            box->accept();
        });
        missing.findChild<QPushButton *>("play")->click();
        check(player_get_state() == PLAYER_STOPPED, "failed playback remains stopped");
        check(!missing.findChild<QSlider *>("progress")->isEnabled(), "failed playback has no fake progress");
    }
    writeSongs("");
    {
        MusicWindow empty; // 空歌单的按钮操作不访问无效下标。
        empty.findChild<QPushButton *>("play")->click();
        empty.findChild<QPushButton *>("next")->click();
        check(player_get_current_id() == -1, "empty playlist stays stopped");
    }
    check(QDir::setCurrent(previous), "restore working directory");
    qInfo("playback UI tests passed");
    return 0;
}
