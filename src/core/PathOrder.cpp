#include "core/PathOrder.h"

#include <QChar>

namespace nexus::core {

namespace {

qsizetype digitRunEnd(QStringView s, qsizetype from)
{
    while (from < s.size() && s[from].isDigit()) {
        ++from;
    }
    return from;
}

// Compares the digit runs starting at i / j by numeric value; advances both past the runs.
int compareDigitRuns(QStringView a, qsizetype& i, QStringView b, qsizetype& j)
{
    const qsizetype endA = digitRunEnd(a, i);
    const qsizetype endB = digitRunEnd(b, j);
    qsizetype startA = i;
    qsizetype startB = j;
    while (startA < endA && a[startA] == u'0') {
        ++startA;
    }
    while (startB < endB && b[startB] == u'0') {
        ++startB;
    }
    i = endA;
    j = endB;
    const qsizetype lenA = endA - startA;
    const qsizetype lenB = endB - startB;
    if (lenA != lenB) {
        return lenA < lenB ? -1 : 1;
    }
    const int cmp = a.mid(startA, lenA).compare(b.mid(startB, lenB));
    return cmp < 0 ? -1 : (cmp > 0 ? 1 : 0);
}

int naturalCompare(QStringView a, QStringView b)
{
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].isDigit() && b[j].isDigit()) {
            const int cmp = compareDigitRuns(a, i, b, j);
            if (cmp != 0) {
                return cmp;
            }
            continue;
        }
        const char16_t ca = a[i].toCaseFolded().unicode();
        const char16_t cb = b[j].toCaseFolded().unicode();
        if (ca != cb) {
            return ca < cb ? -1 : 1;
        }
        ++i;
        ++j;
    }
    if (i < a.size()) {
        return 1;
    }
    if (j < b.size()) {
        return -1;
    }
    const int tie = a.compare(b, Qt::CaseSensitive);
    return tie < 0 ? -1 : (tie > 0 ? 1 : 0);
}

} // namespace

bool naturalLess(QStringView a, QStringView b)
{
    return naturalCompare(a, b) < 0;
}

bool pathLess(QStringView a, bool aIsDir, QStringView b, bool bIsDir)
{
    qsizetype posA = 0;
    qsizetype posB = 0;
    while (true) {
        const bool endA = posA >= a.size();
        const bool endB = posB >= b.size();
        if (endA || endB) {
            return endA && !endB; // the parent sorts first; identical paths are not less
        }
        const qsizetype slashA = a.indexOf(u'/', posA);
        const qsizetype slashB = b.indexOf(u'/', posB);
        const bool lastA = slashA < 0;
        const bool lastB = slashB < 0;
        const QStringView partA = a.mid(posA, lastA ? -1 : slashA - posA);
        const QStringView partB = b.mid(posB, lastB ? -1 : slashB - posB);
        if (partA != partB) {
            const bool dirA = !lastA || aIsDir;
            const bool dirB = !lastB || bIsDir;
            if (dirA != dirB) {
                return dirA;
            }
            return naturalLess(partA, partB);
        }
        posA = lastA ? a.size() : slashA + 1;
        posB = lastB ? b.size() : slashB + 1;
    }
}

bool isSafeRelPath(QStringView rel)
{
    if (rel.isEmpty() || rel.contains(u'\\') || rel.contains(u':')) {
        return false;
    }
    qsizetype pos = 0;
    while (pos <= rel.size()) {
        qsizetype next = rel.indexOf(u'/', pos);
        if (next < 0) {
            next = rel.size();
        }
        const QStringView part = rel.mid(pos, next - pos);
        if (part.isEmpty() || part == u"." || part == u"..") {
            return false;
        }
        pos = next + 1;
    }
    return true;
}

} // namespace nexus::core
