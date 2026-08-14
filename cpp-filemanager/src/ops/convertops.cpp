#include "ops/convertops.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

// swordconv lives next to the file manager in the project tree during
// development, and on PATH once installed.
QString helperPath() {
    const QString local =
        QFileInfo(QCoreApplication::applicationDirPath() + "/../../swordconv")
            .absoluteFilePath();
    if (QFileInfo::exists(local))
        return local;
    return QStandardPaths::findExecutable("swordconv");
}

const QList<ConvFormat> &allFormats() {
    static const QList<ConvFormat> f = {
        {"pdf",  "PDF  (.pdf)",       ".pdf"},
        {"docx", "Word  (.docx)",     ".docx"},
        {"md",   "Markdown  (.md)",   ".md"},
        {"txt",  "Plain text  (.txt)", ".txt"},
        {"html", "HTML  (.html)",     ".html"},
    };
    return f;
}

// Extension -> the format id it already is, so we never offer a no-op.
QString formatOf(const QString &path) {
    const QString s = QFileInfo(path).suffix().toLower();
    if (s == "pdf")  return "pdf";
    if (s == "docx") return "docx";
    if (s == "md" || s == "markdown") return "md";
    if (s == "txt" || s == "text")    return "txt";
    if (s == "html" || s == "htm")    return "html";
    return {};
}

} // namespace

bool isConvertible(const QString &path) {
    return QFileInfo(path).isFile() && !formatOf(path).isEmpty();
}

QList<ConvFormat> conversionTargetsFor(const QString &path) {
    const QString self = formatOf(path);
    if (self.isEmpty())
        return {};
    QList<ConvFormat> out;
    for (const ConvFormat &f : allFormats()) {
        if (f.id != self)
            out.append(f);
    }
    return out;
}

bool convertDocument(const QString &inPath, const QString &outPath,
                     const QString &formatId, QString *error) {
    const QString helper = helperPath();
    if (helper.isEmpty()) {
        if (error)
            *error = "The swordconv helper was not found.\n"
                     "Re-run install-cpp.sh to install it.";
        return false;
    }

    // Argument list, not a shell string: a file named `a; rm -rf ~.pdf` must
    // reach the helper as one literal argument.
    QProcess proc;
    proc.start(helper, QStringList{formatId, inPath, outPath});

    if (!proc.waitForStarted(10000)) {
        if (error)
            *error = "Could not start the converter.";
        return false;
    }
    if (!proc.waitForFinished(5 * 60 * 1000)) {
        proc.kill();
        proc.waitForFinished(2000);
        if (error)
            *error = "Conversion timed out after 5 minutes.";
        return false;
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QString msg = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (msg.isEmpty())
            msg = QString("Converter exited with code %1").arg(proc.exitCode());
        if (error)
            *error = msg;
        return false;
    }
    return true;
}
