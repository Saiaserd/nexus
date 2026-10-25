#pragma once

#include <cstdint>
#include <optional>

#include <QString>

// File-system error types (ADR 0004). The core layer never produces user-facing text;
// the UI maps these enums to messages.
namespace nexus::core {

enum class NameError : std::uint8_t {
    Empty,
    InvalidCharacter, // \ / : * ? " < > |
    ControlCharacter,
    TrailingSpaceOrDot,
    ReservedName, // CON, PRN, AUX, NUL, COM1-9, LPT1-9 (also with an extension)
    TooLong,
};

enum class FileErrorKind : std::uint8_t {
    NotFound,
    NotADirectory,
    NotAFile,
    AccessDenied,
    AlreadyExists,
    InvalidPath, // empty, absolute, or escaping the vault root
    InvalidName, // see FileError::nameError
    InvalidEncoding,
    TrashFailed,
    IoError,
};

struct FileError {
    FileErrorKind kind = FileErrorKind::IoError;
    QString path; // vault-relative path when known, otherwise the path that was given
    std::optional<NameError> nameError;

    friend bool operator==(const FileError& a, const FileError& b)
    {
        return a.kind == b.kind && a.path == b.path && a.nameError == b.nameError;
    }
};

} // namespace nexus::core
