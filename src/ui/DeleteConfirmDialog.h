#pragma once

#include <QDialog>

class QCheckBox;

namespace nexus::ui {

// Confirmation before a note or folder is moved to the recycle bin (States.dc.html 3).
class DeleteConfirmDialog : public QDialog {
    Q_OBJECT

public:
    // displayName: shown in the title. noteCount: notes inside a folder (ignored for a note).
    // hasUnsaved: some of the affected notes have unsaved changes.
    DeleteConfirmDialog(const QString& displayName, bool folder, int noteCount, bool hasUnsaved,
                        QWidget* parent = nullptr);

    [[nodiscard]] bool dontAskAgain() const;
    [[nodiscard]] QString bodyText() const { return m_body; }

private:
    QCheckBox* m_dontAsk = nullptr;
    QString m_body;
};

} // namespace nexus::ui
