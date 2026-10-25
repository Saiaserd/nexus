#pragma once

#include <QLabel>

namespace nexus::ui {

// A single-line label that cuts long text with "..." at the end of the available width
// (design: white-space nowrap, text-overflow ellipsis). It can shrink to nothing, so it never
// forces its parent wider; the full text is the tool tip.
class ElidedLabel : public QLabel {
    Q_OBJECT

public:
    explicit ElidedLabel(QWidget* parent = nullptr);
    explicit ElidedLabel(const QString& text, QWidget* parent = nullptr);

    void setFullText(const QString& text);
    [[nodiscard]] const QString& fullText() const { return m_text; }
    // What is currently painted (for tests).
    [[nodiscard]] QString elidedText() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_text;
};

} // namespace nexus::ui
