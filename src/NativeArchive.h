#pragma once
#include <QString>
#include <QStringList>
namespace NativeArchive {
// Supports normal stored/deflated JAR archives. Rejects encryption, ZIP64 and traversal.
void extract(const QString &archive, const QString &destination, const QStringList &excludes = {});
}
