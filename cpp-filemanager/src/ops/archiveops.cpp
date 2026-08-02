#include "ops/archiveops.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>

namespace {

// Longest suffixes first: ".tar.gz" must win over ".gz", otherwise a tarball
// gets treated as a plain gzip stream and extracts to a single blob.
const QStringList &knownSuffixes() {
    static const QStringList s = {
        "tar.gz", "tar.bz2", "tar.xz", "tar.zst", "tgz", "tbz2", "txz",
        "zip", "7z", "rar", "tar", "gz", "bz2", "xz", "zst",
    };
    return s;
}

QString matchedSuffix(const QString &fileName) {
    const QString lower = fileName.toLower();
    for (const QString &s : knownSuffixes()) {
        if (lower.endsWith("." + s))
            return s;
    }
    return {};
}

bool haveTool(const QString &name) {
    return !QStandardPaths::findExecutable(name).isEmpty();
}

// Run `program args` in workDir. Captures stderr so failures can be reported
// with the tool's own message instead of a bare exit code.
bool run(const QString &program, const QStringList &args, const QString &workDir,
         QString *error) {
    const QString exe = QStandardPaths::findExecutable(program);
    if (exe.isEmpty()) {
        if (error) *error = QString("%1 is not installed").arg(program);
        return false;
    }

    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.start(exe, args);
    if (!proc.waitForStarted(5000)) {
        if (error) *error = QString("Could not start %1").arg(program);
        return false;
    }
    // Archiving a large tree can legitimately take a while; the caller shows a
    // wait cursor rather than a progress dialog, so allow several minutes.
    if (!proc.waitForFinished(10 * 60 * 1000)) {
        proc.kill();
        if (error) *error = QString("%1 timed out").arg(program);
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QString msg = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (msg.isEmpty())
            msg = QString("%1 failed with code %2").arg(program).arg(proc.exitCode());
        if (error) *error = msg;
        return false;
    }
    return true;
}

} // namespace

QList<ArchiveFormat> availableFormats() {
    QList<ArchiveFormat> out;
    const bool tar = haveTool("tar");

    if (haveTool("zip"))
        out.append({"zip", "ZIP  (.zip)", ".zip"});
    else if (haveTool("7z"))
        // 7z writes zip too, so the most-expected format stays offered even
        // without Info-ZIP installed.
        out.append({"zip", "ZIP  (.zip)", ".zip"});

    if (tar && haveTool("gzip"))  out.append({"tar.gz",  "Tar + gzip  (.tar.gz)",   ".tar.gz"});
    if (tar && haveTool("xz"))    out.append({"tar.xz",  "Tar + xz  (.tar.xz)",     ".tar.xz"});
    if (tar && haveTool("bzip2")) out.append({"tar.bz2", "Tar + bzip2  (.tar.bz2)", ".tar.bz2"});
    if (tar && haveTool("zstd"))  out.append({"tar.zst", "Tar + zstd  (.tar.zst)",  ".tar.zst"});
    if (tar)                      out.append({"tar",     "Tar, no compression  (.tar)", ".tar"});
    if (haveTool("7z"))           out.append({"7z",      "7-Zip  (.7z)",            ".7z"});

    return out;
}

bool isArchive(const QString &path) {
    QFileInfo fi(path);
    return fi.isFile() && !matchedSuffix(fi.fileName()).isEmpty();
}

QString suggestedArchiveName(const QStringList &paths) {
    if (paths.isEmpty())
        return QStringLiteral("archive");
    if (paths.size() == 1) {
        QFileInfo fi(paths.first());
        return fi.isDir() ? fi.fileName() : fi.completeBaseName();
    }
    const QString parent = QFileInfo(paths.first()).absolutePath();
    const QString name = QDir(parent).dirName();
    return name.isEmpty() ? QStringLiteral("archive") : name;
}

bool compressTo(const QStringList &paths, const QString &archivePath,
                const QString &formatId, QString *error) {
    if (paths.isEmpty()) {
        if (error) *error = "Nothing selected";
        return false;
    }

    // Run from the common parent and pass bare names, so the archive contains
    // "photos/a.png" rather than "home/sword/Pictures/photos/a.png".
    const QString workDir = QFileInfo(paths.first()).absolutePath();
    QStringList names;
    for (const QString &p : paths) {
        const QFileInfo fi(p);
        if (fi.absolutePath() != workDir) {
            if (error) *error = "All items must be in the same folder";
            return false;
        }
        names << fi.fileName();
    }

    const QString out = QFileInfo(archivePath).absoluteFilePath();

    if (formatId == "zip") {
        if (haveTool("zip"))
            return run("zip", QStringList{"-r", "-q", out} + names, workDir, error);
        return run("7z", QStringList{"a", "-tzip", "-bso0", "-bsp0", out} + names,
                   workDir, error);
    }
    if (formatId == "7z")
        return run("7z", QStringList{"a", "-bso0", "-bsp0", out} + names, workDir, error);

    QString flag;
    if (formatId == "tar.gz")       flag = "-z";
    else if (formatId == "tar.xz")  flag = "-J";
    else if (formatId == "tar.bz2") flag = "-j";
    else if (formatId == "tar.zst") flag = "--zstd";
    else if (formatId == "tar")     flag = QString();
    else {
        if (error) *error = QString("Unknown format: %1").arg(formatId);
        return false;
    }

    QStringList args{"-c"};
    if (!flag.isEmpty())
        args << flag;
    // "--" stops tar reading a leading-dash filename as an option.
    args << "-f" << out << "--" << names;
    return run("tar", args, workDir, error);
}

bool extractTo(const QString &archivePath, const QString &destDir, QString *error) {
    const QFileInfo fi(archivePath);
    const QString suffix = matchedSuffix(fi.fileName());
    if (suffix.isEmpty()) {
        if (error) *error = "Not a recognised archive";
        return false;
    }
    if (!QDir().mkpath(destDir)) {
        if (error) *error = "Could not create destination folder";
        return false;
    }

    const QString src = fi.absoluteFilePath();
    const QString dest = QDir(destDir).absolutePath();

    if (suffix == "zip") {
        if (haveTool("unzip"))
            return run("unzip", {"-q", "-o", src, "-d", dest}, dest, error);
        return run("7z", {"x", "-y", "-bso0", "-bsp0", "-o" + dest, src}, dest, error);
    }
    if (suffix == "7z")
        return run("7z", {"x", "-y", "-bso0", "-bsp0", "-o" + dest, src}, dest, error);
    if (suffix == "rar") {
        if (haveTool("unrar"))
            return run("unrar", {"x", "-y", src, dest + "/"}, dest, error);
        return run("7z", {"x", "-y", "-bso0", "-bsp0", "-o" + dest, src}, dest, error);
    }

    if (suffix.startsWith("tar") || suffix == "tgz" || suffix == "tbz2" || suffix == "txz") {
        // Let tar sniff the compression rather than mapping every suffix.
        return run("tar", {"-x", "-a", "-f", src, "-C", dest}, dest, error);
    }

    // Bare gz/bz2/xz/zst: a single compressed file, not a container.
    QString tool;
    if (suffix == "gz")       tool = "gzip";
    else if (suffix == "bz2") tool = "bzip2";
    else if (suffix == "xz")  tool = "xz";
    else if (suffix == "zst") tool = "zstd";
    if (tool.isEmpty()) {
        if (error) *error = "Unsupported archive type";
        return false;
    }

    const QString exe = QStandardPaths::findExecutable(tool);
    if (exe.isEmpty()) {
        if (error) *error = QString("%1 is not installed").arg(tool);
        return false;
    }
    QString outName = fi.fileName();
    outName.chop(suffix.length() + 1); // drop ".gz" etc.
    if (outName.isEmpty())
        outName = fi.fileName() + ".out";

    QProcess proc;
    proc.setWorkingDirectory(dest);
    proc.setStandardOutputFile(QDir(dest).filePath(outName));
    proc.start(exe, {"-d", "-c", src});
    if (!proc.waitForFinished(10 * 60 * 1000) || proc.exitCode() != 0) {
        proc.kill();
        if (error) *error = QString("%1 failed").arg(tool);
        return false;
    }
    return true;
}
