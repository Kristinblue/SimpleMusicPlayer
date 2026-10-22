// 复用真实窗口，通过按钮和列表验证界面与数据模块的连接，不需要 Qt Test 模块。
#define main music_player_entry
#include "../src/main.cpp"
#undef main

#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>

// condition 为检查结果，message 为失败说明；失败时终止测试，无返回值。
static void check(bool condition, const char *message)
{
    if (!condition) qFatal("%s", message);
}

// 将内容 text 写入 path，任何写入错误终止测试，无返回值。
static void writeText(const char *path, const QByteArray &text)
{
    QFile file(QString::fromUtf8(path)); // 隔离测试目录中的输入文件。
    check(file.open(QIODevice::WriteOnly), "open input");
    check(file.write(text) == text.size(), "write input");
}

// 返回 path 文件的原始内容，无法读取时终止测试。
static QByteArray readText(const char *path)
{
    QFile file(QString::fromUtf8(path)); // 待验证的收藏文件。
    check(file.open(QIODevice::ReadOnly), "read output");
    return file.readAll().replace("\r\n", "\n");
}

// 点击 list 中 row 行的爱心并处理排队事件；参数为列表及行号，无返回值。
static void clickHeart(QListWidget *list, int row)
{
    QWidget *widget = list->itemWidget(list->item(row)); // 当前歌曲行。
    check(widget != nullptr, "song row exists");
    QPushButton *heart = widget->findChild<QPushButton *>("heart"); // 真实收藏按钮。
    check(heart != nullptr, "heart exists");
    heart->click();
    QApplication::processEvents();
    QApplication::processEvents();
}

// 验证窗口显示的红心状态；list/row 为列表及行号，expected 为期望状态，无返回值。
static void checkHeart(QListWidget *list, int row, bool expected)
{
    QPushButton *heart = list->itemWidget(list->item(row))->findChild<QPushButton *>("heart");
    // heart 为此歌曲行的真实按钮，属性和值必须一起更新。
    check(heart->property("favorite").toBool() == expected, "favorite property");
    check(heart->text() == (expected ? QStringLiteral("♥") : QStringLiteral("♡")), "heart text");
}

// 在下一次模态弹窗出现时关闭它，并验证 title；无返回值。
static void closeWarning(const QString &title)
{
    QTimer::singleShot(20, [title]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); // 当前提示框。
        check(box && box->windowTitle() == title, "expected error dialog");
        box->accept();
    });
}

// 独立入口，argc/argv 交给 Qt；成功返回 0。所有数据均在临时目录，避免修改用户收藏。
int main(int argc, char *argv[])
{
    QApplication app(argc, argv); // 测试事件循环和窗口所需的 Qt 环境。
    QTemporaryDir temporary; // 自动清理的测试目录。
    QString previous = QDir::currentPath(); // 退出测试前恢复原工作目录。
    check(temporary.isValid() && QDir::setCurrent(temporary.path()), "temporary directory");
    check(QDir().mkpath("data"), "create data directory");
    writeText("data/songs.txt", "20|晴天|歌手甲|夏日|music/20.wav\n3|夜空|歌手乙|夜航|music/03.wav\n");
    {
        MusicWindow window; // 真实界面从测试歌单启动。
        QListWidget *songs = window.findChild<QListWidget *>("songList"); // 歌单列表。
        QListWidget *favorites = window.findChild<QListWidget *>("favoriteList"); // 收藏列表。
        check(songs && favorites && songs->count() == 2 && favorites->count() == 0, "initial lists");
        clickHeart(songs, 1);
        check(favorites->count() == 1 && readText("data/favorites.txt") == "3\n", "add and save");
        checkHeart(songs, 1, true);
        checkHeart(favorites, 0, true);
        clickHeart(songs, 0);
        check(favorites->count() == 2 && readText("data/favorites.txt") == "20\n3\n", "playlist order");
        favorites->itemClicked(favorites->item(1));
        check(songs->currentRow() == 1, "favorite selection uses song ID mapping");
        closeWarning(QStringLiteral("歌曲信息"));
        songs->itemWidget(songs->item(1))->findChild<QPushButton *>("more")->click();
        clickHeart(favorites, 0);
        check(favorites->count() == 1 && readText("data/favorites.txt") == "3\n", "remove from favorites");
        check(songs->currentRow() == 1, "selection survives refresh");
        checkHeart(songs, 0, false);

        writeText("data/favorites.txt.tmp", "keep");
        closeWarning(QStringLiteral("保存失败"));
        clickHeart(songs, 0);
        check(data_is_favorite(20) == 0 && favorites->count() == 1, "failed save rolls back memory");
        checkHeart(songs, 0, false);
        check(readText("data/favorites.txt") == "3\n", "failed save preserves file");
        check(QFile::remove("data/favorites.txt.tmp"), "remove test obstruction");
    }
    {
        MusicWindow restarted; // 模拟重启窗口和重新读取数据模块。
        QListWidget *songs = restarted.findChild<QListWidget *>("songList"); // 重启后的歌单。
        QListWidget *favorites = restarted.findChild<QListWidget *>("favoriteList"); // 重启后的收藏。
        check(favorites->count() == 1, "favorites restored after restart");
        checkHeart(songs, 1, true);
        clickHeart(songs, 1);
        check(favorites->count() == 0 && readText("data/favorites.txt").isEmpty(), "remove from playlist");
    }
    writeText("data/songs.txt", "");
    {
        MusicWindow empty; // 空歌单不能访问无效下标。
        check(empty.findChild<QListWidget *>("songList")->count() == 0, "empty playlist");
        empty.findChild<QPushButton *>("play")->click();
        empty.findChild<QPushButton *>("next")->click();
    }
    writeText("data/songs.txt", "broken\n");
    {
        MusicWindow invalid; // 损坏输入应提示并阻止收藏操作。
        closeWarning(QStringLiteral("读取失败"));
        QApplication::processEvents();
        check(invalid.findChild<QListWidget *>("songList")->count() == 0, "invalid input has no fake songs");
        check(readText("data/favorites.txt").isEmpty(), "invalid input preserves favorites");
    }
    check(QDir::setCurrent(previous), "restore working directory");
    qInfo("favorites UI tests passed");
    return EXIT_SUCCESS;
}
