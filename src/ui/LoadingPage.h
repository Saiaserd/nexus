#pragma once

#include <QWidget>

class QLabel;

namespace nexus::ui {

class BusyBar;

// Shown while a vault is being opened on a worker thread: a message and a small moving bar.
class LoadingPage : public QWidget {
    Q_OBJECT

public:
    explicit LoadingPage(QWidget* parent = nullptr);

    void setVaultName(const QString& name);
    [[nodiscard]] QString message() const;

private:
    QLabel* m_message;
    BusyBar* m_bar;
};

} // namespace nexus::ui
