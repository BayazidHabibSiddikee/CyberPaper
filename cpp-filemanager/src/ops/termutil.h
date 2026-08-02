#pragma once
#include <QString>

// Launch a terminal in dir (or parent of file).
void openTerminalAt(const QString &path);

// Open path in yazi inside a terminal (dir or file entry).
bool openInYazi(const QString &path);

// True for text / image types suitable for in-app preview.
bool isPreviewableFile(const QString &path);
