#include "ui/DeleteConfirmDialog.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace nexus::ui {

namespace {
constexpr int kDialogWidth = 420;
} // namespace

DeleteConfirmDialog::DeleteConfirmDialog(const QString& displayName, bool folder, int noteCount,
                                         bool hasUnsaved, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("deleteConfirmDialog"));
    setWindowTitle(tr("刪除"));
    setFixedWidth(kDialogWidth);
    setModal(true);

    m_body =
        folder
            ? tr("資料夾和裡面的 %1 篇筆記會移到系統的資源回收筒，可以從那裡還原。").arg(noteCount)
            : tr("筆記會移到系統的資源回收筒，可以從那裡還原。");
    if (hasUnsaved) {
        m_body += folder ? tr("有些筆記有未儲存的變更，也會一起捨棄。")
                         : tr("這篇筆記有未儲存的變更，也會一起捨棄。");
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 18);
    layout->setSpacing(0);

    auto* title = new QLabel(tr("刪除「%1」？").arg(displayName), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    title->setWordWrap(true);
    layout->addWidget(title);

    auto* body = new QLabel(m_body, this);
    body->setObjectName(QStringLiteral("dialogBody"));
    body->setWordWrap(true);
    layout->addSpacing(10);
    layout->addWidget(body);

    m_dontAsk = new QCheckBox(tr("不要再詢問"), this);
    layout->addSpacing(14);
    layout->addWidget(m_dontAsk);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(10);
    buttons->addStretch(1);
    auto* cancel = new QPushButton(tr("取消"), this);
    auto* confirm = new QPushButton(tr("移到資源回收筒"), this);
    confirm->setProperty("variant", QStringLiteral("danger"));
    cancel->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(confirm);
    layout->addSpacing(18);
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, this, &QDialog::accept);
}

bool DeleteConfirmDialog::dontAskAgain() const
{
    return m_dontAsk->isChecked();
}

} // namespace nexus::ui
