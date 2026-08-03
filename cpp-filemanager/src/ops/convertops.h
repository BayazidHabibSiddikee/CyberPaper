#pragma once
#include <QString>
#include <QStringList>

// Document conversion, delegated to the `swordconv` helper script.
//
// The heavy lifting is Python (PyMuPDF, python-docx, mammoth, pdf2docx) because
// the alternative is a LibreOffice dependency an order of magnitude larger.
// Arguments are passed to QProcess as a list, never as a shell string.

struct ConvFormat {
    QString id;      // "pdf", "docx", "md", "txt", "html"
    QString label;   // menu text
    QString suffix;  // ".pdf"
};

// Formats `path` can be turned into; empty when the file is not convertible.
QList<ConvFormat> conversionTargetsFor(const QString &path);

bool isConvertible(const QString &path);

// Blocking. Writes to outPath; returns false with a human-readable reason.
bool convertDocument(const QString &inPath, const QString &outPath,
                     const QString &formatId, QString *error);
