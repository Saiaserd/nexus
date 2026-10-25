#include "ui/NoteSession.h"

#include <utility>

#include <QPlainTextDocumentLayout>
#include <QTextDocument>
#include <QtConcurrent>

namespace nexus::ui {

NoteSession::NoteSession(std::shared_ptr<const core::NoteFiles> files, const QString& relPath,
                         QObject* parent)
    : QObject(parent), m_files(std::move(files)), m_relPath(relPath),
      m_document(new QTextDocument(this))
{
    m_document->setDocumentLayout(new QPlainTextDocumentLayout(m_document));

    m_autosave.setSingleShot(true);
    m_autosave.setInterval(kAutosaveDelayMs);
    connect(&m_autosave, &QTimer::timeout, this, &NoteSession::save);
    connect(m_document, &QTextDocument::contentsChanged, this, &NoteSession::onContentsChanged);
    connect(&m_read, &QFutureWatcher<ReadResult>::finished, this, &NoteSession::onReadFinished);
    connect(&m_write, &QFutureWatcher<WriteResult>::finished, this, &NoteSession::onWriteFinished);
}

NoteSession::~NoteSession()
{
    // The tasks only hold copies and the shared NoteFiles; waiting guarantees that a started
    // write is complete before the tab is gone.
    m_read.waitForFinished();
    m_write.waitForFinished();
}

NoteSession::SaveState NoteSession::saveState() const
{
    if (m_writePending) {
        return SaveState::Saving;
    }
    return isDirty() ? SaveState::Unsaved : SaveState::Saved;
}

void NoteSession::load()
{
    m_state = State::Loading;
    m_read.setFuture(QtConcurrent::run(
        [files = m_files, rel = m_relPath]() -> ReadResult { return files->read(rel); }));
}

void NoteSession::onReadFinished()
{
    const ReadResult result = m_read.result();
    if (!result) {
        m_state = State::Failed;
        m_lastError = result.error();
        emit loadFailed(m_lastError);
        return;
    }
    m_lineEnding = result->lineEnding;
    m_hasBom = result->hasBom;
    m_settingText = true;
    m_document->setUndoRedoEnabled(false); // the loaded text is not an undo step
    m_document->setPlainText(result->text);
    m_document->setUndoRedoEnabled(true);
    m_settingText = false;
    m_state = State::Ready;
    emit loaded();
    emit saveStateChanged();
}

void NoteSession::onContentsChanged()
{
    if (m_settingText || m_state != State::Ready) {
        return;
    }
    const bool wasDirty = isDirty();
    ++m_editSerial;
    m_autosave.start();
    if (!wasDirty) {
        emit saveStateChanged();
    }
}

core::NoteDocument NoteSession::snapshot() const
{
    return {m_document->toPlainText(), m_lineEnding, m_hasBom};
}

void NoteSession::save()
{
    m_autosave.stop();
    if (m_state != State::Ready || !isDirty() || m_writePending) {
        return; // a running write saves again when it finishes
    }
    startWrite();
}

void NoteSession::startWrite()
{
    m_writingSerial = m_editSerial;
    m_writePending = true;
    m_write.setFuture(
        QtConcurrent::run([files = m_files, rel = m_relPath, doc = snapshot()]() -> WriteResult {
            return files->write(rel, doc);
        }));
    emit saveStateChanged();
}

bool NoteSession::collectWrite()
{
    if (!m_writePending) {
        return true;
    }
    m_writePending = false;
    const WriteResult result = m_write.result();
    if (result) {
        m_savedSerial = m_writingSerial;
    } else {
        m_lastError = result.error();
        emit saveFailed(m_lastError);
    }
    emit saveStateChanged();
    return result.has_value();
}

void NoteSession::onWriteFinished()
{
    if (collectWrite() && isDirty() && !m_autosave.isActive()) {
        save(); // edits made while writing whose idle timer already fired
    }
}

bool NoteSession::saveAndWait()
{
    m_write.waitForFinished();
    collectWrite();
    m_autosave.stop();
    if (m_state != State::Ready || !isDirty()) {
        return true;
    }
    m_writingSerial = m_editSerial;
    const WriteResult result = m_files->write(m_relPath, snapshot());
    if (!result) {
        m_lastError = result.error();
        emit saveFailed(m_lastError);
        return false;
    }
    m_savedSerial = m_writingSerial;
    emit saveStateChanged();
    return true;
}

void NoteSession::setRelPath(const QString& relPath)
{
    if (relPath == m_relPath) {
        return;
    }
    m_write.waitForFinished(); // a running write still targets the old path
    m_relPath = relPath;
    emit relPathChanged(m_relPath);
}

} // namespace nexus::ui
