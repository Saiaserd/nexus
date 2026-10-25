#include "ui/WelcomePage.h"

#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include "ui/ClickableFrame.h"
#include "ui/ElidedLabel.h"
#include "ui/Fonts.h"
#include "ui/Icons.h"
#include "ui/ThemeManager.h"

#ifndef NEXUS_VERSION
#define NEXUS_VERSION "0.0.0"
#endif

namespace nexus::ui {

namespace {

constexpr int kColumnMaxWidth = 640;
constexpr int kEdgeMarginX = 16;
constexpr int kEdgeMarginY = 32;

QLabel* makeLabel(const QString& text, const char* objectName, QWidget* parent = nullptr)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QLatin1StringView(objectName));
    return label;
}

QLabel* makeIconLabel(int size, QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setFixedSize(size, size);
    label->setScaledContents(false);
    return label;
}

void setIcon(QLabel* label, const QString& name, const QColor& color, int size)
{
    label->setPixmap(icons::pixmap(name, color, size, label->devicePixelRatioF()));
}

// Moving the first and last row's hover background inside the list's rounded corners.
QString positionOf(int index, int count)
{
    if (count == 1) {
        return QStringLiteral("only");
    }
    if (index == 0) {
        return QStringLiteral("first");
    }
    return index == count - 1 ? QStringLiteral("last") : QStringLiteral("middle");
}

} // namespace

WelcomePage::WelcomePage(ThemeManager& theme, QWidget* parent)
    : QWidget(parent), m_theme(theme),
      m_exists([](const QString& path) { return QFileInfo(path).isDir(); })
{
    build();
    connect(&m_theme, &ThemeManager::themeChanged, this, [this] {
        applyIcons();
        refreshFooter();
        rebuildRows();
    });
    applyIcons();
}

WelcomePage::~WelcomePage() = default;

void WelcomePage::build()
{
    setObjectName(QStringLiteral("welcomePage"));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("welcomeScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("welcomeContent"));
    scroll->setWidget(content);

    // [stretch][ column (max 640 wide) ][stretch], centred both ways like the design.
    auto* vertical = new QVBoxLayout(content);
    vertical->setContentsMargins(kEdgeMarginX, kEdgeMarginY, kEdgeMarginX, kEdgeMarginY);
    auto* horizontal = new QHBoxLayout();
    vertical->addStretch(1);
    vertical->addLayout(horizontal);
    vertical->addStretch(1);

    auto* column = new QWidget(content);
    column->setMaximumWidth(kColumnMaxWidth);
    horizontal->addStretch(1);
    horizontal->addWidget(column, 1000);
    horizontal->addStretch(1);

    auto* stack = new QVBoxLayout(column);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(32);

    // --- brand ---------------------------------------------------------------------------
    auto* brand = new QHBoxLayout();
    brand->setSpacing(16);
    m_logo = makeIconLabel(44, column);
    brand->addWidget(m_logo, 0, Qt::AlignVCenter);
    auto* titles = new QVBoxLayout();
    titles->setSpacing(4);
    QLabel* title = makeLabel(QStringLiteral("Nexus"), "welcomeTitle", column);
    QFont titleFont;
    titleFont.setWeight(QFont::Bold);
    titleFont.setPixelSize(28);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.3);
    title->setFont(titleFont);
    titles->addWidget(title);
    titles->addWidget(
        makeLabel(tr("用連結的筆記描述意圖，讓 AI 去寫程式。"), "welcomeTagline", column));
    brand->addLayout(titles, 1);
    stack->addLayout(brand);

    // --- the two actions -----------------------------------------------------------------
    auto* cards = new QGridLayout();
    cards->setHorizontalSpacing(14);
    cards->setVerticalSpacing(14);
    cards->setColumnStretch(0, 1);
    cards->setColumnStretch(1, 1);

    const auto makeCard = [column](const QString& variant, QLabel*& icon, const QString& title,
                                   const QString& sub) {
        auto* card = new ClickableFrame(column);
        card->setObjectName(QStringLiteral("card"));
        card->setProperty("variant", variant);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 18, 18, 16);
        layout->setSpacing(10);
        icon = makeIconLabel(22, card);
        layout->addWidget(icon, 0, Qt::AlignLeft);
        layout->addWidget(makeLabel(title, "cardTitle", card));
        auto* subLabel = makeLabel(sub, "cardSub", card);
        subLabel->setWordWrap(true);
        layout->addWidget(subLabel);
        layout->addStretch(1);
        for (auto* child : card->findChildren<QLabel*>()) {
            child->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        return card;
    };
    m_openCard = makeCard(QStringLiteral("primary"), m_openIcon, tr("開啟資料夾作為 vault"),
                          tr("選擇已有 Markdown 筆記的資料夾　Ctrl+O"));
    m_createCard = makeCard(QStringLiteral("secondary"), m_createIcon, tr("建立新的 vault"),
                            tr("在空資料夾建立，附一篇說明筆記"));
    cards->addWidget(m_openCard, 0, 0);
    cards->addWidget(m_createCard, 0, 1);
    stack->addLayout(cards);
    connect(m_openCard, &ClickableFrame::clicked, this, &WelcomePage::openFolderRequested);
    connect(m_createCard, &ClickableFrame::clicked, this, &WelcomePage::createVaultRequested);

    // --- recent vaults -------------------------------------------------------------------
    m_recentSection = new QWidget(column);
    m_recentSection->setObjectName(QStringLiteral("recentSection"));
    auto* section = new QVBoxLayout(m_recentSection);
    section->setContentsMargins(0, 0, 0, 0);
    section->setSpacing(0);
    auto* header = new QHBoxLayout();
    header->setContentsMargins(4, 0, 4, 8);
    QLabel* sectionTitle = makeLabel(tr("最近開啟"), "sectionTitle", m_recentSection);
    QFont sectionFont;
    sectionFont.setWeight(QFont::Bold);
    sectionFont.setPixelSize(12);
    sectionFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    sectionTitle->setFont(sectionFont);
    header->addWidget(sectionTitle);
    header->addStretch(1);
    auto* clear = new QPushButton(tr("清除清單"), m_recentSection);
    clear->setProperty("variant", QStringLiteral("link"));
    clear->setCursor(Qt::PointingHandCursor);
    clear->setFlat(true);
    header->addWidget(clear);
    connect(clear, &QPushButton::clicked, this, &WelcomePage::clearRecentRequested);
    section->addLayout(header);

    m_list = new QFrame(m_recentSection);
    m_list->setObjectName(QStringLiteral("recentList"));
    m_listLayout = new QVBoxLayout(m_list);
    m_listLayout->setContentsMargins(1, 1, 1, 1); // inside the 1px border
    m_listLayout->setSpacing(0);
    section->addWidget(m_list);
    stack->addWidget(m_recentSection);
    m_recentSection->hide();

    // --- footer --------------------------------------------------------------------------
    m_footer = new QLabel(column);
    QLabel* footer = m_footer;
    footer->setObjectName(QStringLiteral("welcomeFooter"));
    footer->setAlignment(Qt::AlignCenter);
    footer->setTextFormat(Qt::RichText);
    connect(footer, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link == QLatin1String("quickstart")) {
            emit quickStartRequested();
        } else {
            emit appearanceRequested();
        }
    });
    stack->addWidget(footer);
    refreshFooter();
}

void WelcomePage::refreshFooter()
{
    // Re-setting the text makes the label pick up the current palette for its link colour.
    QPalette palette = m_footer->palette();
    palette.setColor(QPalette::Link, m_theme.palette().accent);
    m_footer->setPalette(palette);
    m_footer->clear(); // QLabel ignores setText() with unchanged text, which would keep old colours
    m_footer->setText(
        QStringLiteral(
            "Nexus %1　·　<a href=\"quickstart\">%2</a>　·　<a href=\"appearance\">%3</a>")
            .arg(QStringLiteral(NEXUS_VERSION), tr("快速入門"), tr("外觀設定")));
}

void WelcomePage::applyIcons()
{
    const Palette& p = m_theme.palette();
    setIcon(m_logo, QStringLiteral("logo-large"), p.accent, 44);
    setIcon(m_openIcon, QStringLiteral("folder"), p.onAccent, 22);
    setIcon(m_createIcon, QStringLiteral("folder-plus"), p.accent, 22);
}

void WelcomePage::setBusy(bool busy)
{
    m_busy = busy;
    m_openCard->setEnabled(!busy);
    m_createCard->setEnabled(!busy);
    m_list->setEnabled(!busy);
}

void WelcomePage::setRecent(const QList<RecentVault>& entries)
{
    m_entries = entries;
    m_missing.clear();
    rebuildRows();

    // New generation: an answer for an older list is ignored.
    ++m_generation;
    if (m_watcher != nullptr) {
        m_watcher->disconnect(this);
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }
    if (m_entries.isEmpty()) {
        m_checking = false;
        return;
    }

    QStringList paths;
    for (const RecentVault& entry : m_entries) {
        paths << entry.path;
    }
    m_checking = true;
    m_watcher = new QFutureWatcher<QList<bool>>(this);
    const quint64 generation = m_generation;
    connect(m_watcher, &QFutureWatcher<QList<bool>>::finished, this, [this, generation] {
        if (generation == m_generation) {
            onCheckFinished();
        }
    });
    m_watcher->setFuture(QtConcurrent::run([paths, check = m_exists] {
        QList<bool> result;
        result.reserve(paths.size());
        for (const QString& path : paths) {
            result.push_back(check(path)); // may block on a dead network drive: worker thread
        }
        return result;
    }));
}

void WelcomePage::onCheckFinished()
{
    const QList<bool> result = m_watcher->result();
    m_missing.clear();
    for (qsizetype i = 0; i < m_entries.size() && i < result.size(); ++i) {
        if (!result.at(i)) {
            m_missing.insert(m_entries.at(i).path);
        }
    }
    m_checking = false;
    rebuildRows();
    emit checkCompleted();
}

void WelcomePage::rebuildRows()
{
    while (QLayoutItem* item = m_listLayout->takeAt(0)) {
        delete item->widget(); // NOLINT(cppcoreguidelines-owning-memory): QLayoutItem hands over
                               // the widget
        delete item;
    }
    m_recentSection->setVisible(!m_entries.isEmpty());

    const Palette& p = m_theme.palette();
    const QDateTime now = QDateTime::currentDateTime();
    const int count = static_cast<int>(m_entries.size());

    for (int i = 0; i < count; ++i) {
        const RecentVault& entry = m_entries.at(i);
        const bool missing = m_missing.contains(entry.path);
        const QString name = RecentVaults::displayName(entry.path);
        const QString nativePath = QDir::toNativeSeparators(entry.path);

        // Rows with a live folder can be clicked; a missing one only offers "remove".
        QFrame* row = nullptr;
        if (missing) {
            row = new QFrame(m_list);
        } else {
            auto* clickable = new ClickableFrame(m_list);
            connect(clickable, &ClickableFrame::clicked, this,
                    [this, path = entry.path] { emit recentActivated(path); });
            row = clickable;
        }
        row->setObjectName(QStringLiteral("recentRow"));
        row->setProperty("pos", positionOf(i, count));
        row->setProperty("separated", i > 0);
        row->setProperty("missing", missing);

        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(14);

        auto* icon = makeIconLabel(16, row);
        setIcon(icon, missing ? QStringLiteral("alert-circle") : QStringLiteral("folder"),
                missing ? p.dangerLine : p.accent, 16);
        layout->addWidget(icon);

        auto* texts = new QVBoxLayout();
        texts->setSpacing(2);
        texts->addWidget(makeLabel(name, "recentName", row));
        if (missing) {
            auto* message = new ElidedLabel(tr("找不到資料夾：%1").arg(nativePath), row);
            message->setObjectName(QStringLiteral("recentMissing"));
            texts->addWidget(message);
        } else {
            auto* path = new ElidedLabel(nativePath, row);
            path->setObjectName(QStringLiteral("recentPath"));
            path->setFont(fonts::monoFont(12));
            texts->addWidget(path);
        }
        layout->addLayout(texts, 1);

        if (missing) {
            auto* remove = new QPushButton(tr("從清單移除"), row);
            remove->setProperty("variant", QStringLiteral("small"));
            remove->setCursor(Qt::PointingHandCursor);
            connect(remove, &QPushButton::clicked, this,
                    [this, path = entry.path] { emit removeRecentRequested(path); });
            layout->addWidget(remove);
        } else {
            layout->addWidget(makeLabel(formatRelativeTime(entry.opened, now), "recentTime", row));
            for (auto* child : row->findChildren<QLabel*>()) {
                child->setAttribute(Qt::WA_TransparentForMouseEvents);
            }
        }
        m_listLayout->addWidget(row);
    }
}

} // namespace nexus::ui
