# SwordFM — Architecture

A Qt6 Widgets file manager. Single process, no threads, no plugins. Roughly
3,800 lines of C++ across five subgroups under `src/`.

Build: `cmake -B build -S . && cmake --build build -j4`
Install: `install -Dm755 build/swordfm ~/.local/bin/swordfm`

> Always check `which -a swordfm` after building. A stale copy in `~/.local/bin`
> shadows a fresh build and makes changes look like they did nothing.

---

## Subgroup map

```
src/
├── app/      entry point, window shell, theme       ← start reading here
├── model/    data layer (what rows exist)
├── view/     the file listing widget
├── panel/    chrome around the listing
└── ops/      actions performed on files
```

Dependency direction is one-way: `app` knows everything; `panel`, `view` and
`ops` know only `model` and `app/theme.h`. Nothing in `model` includes a widget.

Includes are subgroup-qualified (`#include "model/filefilter.h"`), so any file
tells you at a glance which layers it reaches into.

---

## app/ — entry point and shell

### `main.cpp`
Creates the `QApplication`, forces the Fusion style, applies the One Dark
palette, and hands off to `MainWindow`.

`applyIconTheme()` exists because Qt only inherits the desktop icon theme when a
platform theme plugin is loaded. Under Fusion on a bare i3 session
`QIcon::themeName()` returns empty or `"hicolor"`, every `QIcon::fromTheme()`
lookup yields a null icon, and the file list renders with no icons at all. The
function reads `~/.config/gtk-3.0/settings.ini` and calls `QIcon::setThemeName()`
directly. **If icons ever disappear again, look here first.**

### `mainwindow.cpp` (~680 lines — the largest file)
The coordinator. Owns every widget, the `QFileSystemModel`, navigation history,
the clipboard, and the type/date filter state. Every user action lands here as a
public slot (`copySelection`, `navigateUp`, `graphSelected`, …) so the menu bar,
toolbar, context menu and keyboard shortcuts can all invoke the same code.

Key internals:

- **`applyDirectory(path, pushHistory)`** — the single funnel for changing
  folders. `QFileSystemModel` loads asynchronously, so this stashes `m_pendingRoot`
  and finishes the job in the `directoryLoaded` handler.
- **`applyTypeDateFilter()`** — decides between two very different display modes.
  With no filter active, the normal directory listing shows. With a type or date
  filter, it triggers a **recursive** search instead (see `model/searchmodel`),
  because "show me all images" means "under this whole tree", not "in this one
  folder".
- **`actionPaths()`** — resolves what copy/move/delete should operate on: marked
  paths if any exist, otherwise the current selection.
- **`onSelectionChanged()`** — selecting 2+ items automatically marks them, since
  a multi-selection *is* the intent to act on a group.

### `theme.h`
One Dark color constants and the global stylesheet.

```
BG #282c34   BG2 #21252b   DIM #3e4451    BORDER / HOVER / SELECT
FG #abb2bf   FG_DIM #5c6370
CYAN #61afef  GREEN #98c379  AMBER #e5c07b  RED #e06c75  PURPLE #c678dd
```

There is **no `YELLOW`** — use `AMBER`. Referencing `Theme::YELLOW` is a
recurring compile error.

---

## model/ — the data layer

### `filemodel.cpp`
Thin `QFileSystemModel` subclass. Only overrides `data()` for the font and
`headerData()` for column titles.

### `filefilter.cpp` — `FileFilterProxy`
The `QSortFilterProxyModel` sitting between the model and the views. Three jobs:

1. **Junk filtering** (`isJunkName`) — hides systemd units, libvirt clutter and
   purely numeric names.
2. **Type / date filtering** (`filterAcceptsRow`) — *within the current folder
   only*. Directories are never filtered out; hiding them would strand you in a
   folder with no way to navigate out.
3. **Marks** — a sticky multi-selection that survives navigating between folders,
   so one copy/delete can gather items from several places. Marks are rendered as
   checkboxes via `Qt::CheckStateRole` and tinted `AMBER` via `Qt::ForegroundRole`.

`suffixesFor(TypeFilter)` is the single extension table (images, videos, audio,
documents, archives, discs) — shared with `SearchModel`. **Add new extensions
here and both the in-folder filter and the recursive search pick them up.**

### `searchmodel.cpp` — `SearchModel`
A flat `QStandardItemModel` for recursive results. This exists because
`QFileSystemModel` only ever exposes one directory at a time, so "every image
under here" cannot be expressed as a filter over it.

`startSearch()` is **asynchronous**: it clears the model, hands the walk to a
`FileScanner` on a `QThread`, and returns immediately. Rows arrive in batches
via `progress(found)`, and `completed(found, truncated)` fires at the end.
Starting a new search cancels any scan still running. Columns:

| Name | Location | Size | Type | Date Modified |
|------|----------|------|------|---------------|

Name is the bare filename and Location is the folder relative to the search root.
They are separate columns deliberately — folding the path into Name pushed the
filename off the right edge and made every row look nameless.

The absolute path lives in `Qt::UserRole + 1`; retrieve it with `pathAt(index)`.

Icons and type names are cached **per extension**. Asking the MIME database and
icon theme once per result made populating twenty thousand rows slower than the
disk walk that produced them; every `.png` resolves identically, so one lookup
per suffix is enough.

> `SearchModel` uses `QFileIconProvider`, which requires a `QApplication`, not a
> bare `QGuiApplication`. A test harness using the latter segfaults.

### `filescanner.cpp` — `FileScanner`
The recursive walk itself, run off the GUI thread. This replaced a synchronous
`QDirIterator` loop that froze the window; a search of `$HOME` went from
**over 165 seconds (never observed finishing) to 1.6 s**, and 426 ms → 41 ms on
a purely local subtree. First results appear in ~40 ms.

Three things make the difference, in descending order of impact:

1. **It does not cross filesystem boundaries** (one `fstat` per directory, the
   same rule as `find -xdev`). A home directory here has several rclone/FUSE
   cloud mounts under it, and descending into one turns every `readdir` into a
   network round trip. This was the dominant cost by far. Searching *inside* a
   mount still works — the root's device becomes the reference.
2. **POSIX `readdir` instead of `QDirIterator`.** `dirent.d_type` distinguishes
   file from directory with no `stat()` syscall, so the extension test — pure
   string work — rejects the vast majority of entries before any metadata is
   read. Only entries that already matched get `stat()`ed. `DT_UNKNOWN` (older
   XFS, some network mounts) falls back to `lstat`.
3. **A work-stealing thread pool** over a shared directory queue, sized to
   `min(8, cores)`. The queue tracks in-flight directories as well as pending
   ones, so the scan ends only when nothing is queued *and* nobody is walking.

Results are emitted in batches (200 hits or 100 ms, whichever first). One signal
per file would flood the event loop and end up slower than the synchronous
version. Cancellation is a `std::atomic<bool>` checked per directory —
verified race-free under ThreadSanitizer across 20 mid-walk cancellations.

Hidden entries are skipped unless the window's show-hidden toggle is on, which
keeps results identical to what the old walk returned.


---

## view/ — the file listing

### `fileview.cpp` — `FileView`
A `QStackedWidget` with **three** pages:

| Page | Widget | Model | When |
|------|--------|-------|------|
| Details | `QTreeView` | `FileFilterProxy` | default |
| Icons | `QListView` | `FileFilterProxy` | view toggle |
| Search | `QTreeView` | `SearchModel` | type/date filter active |

The search page is the awkward one: it is **not** backed by the filesystem model,
so anything reading indices needs an `m_searchMode` branch — `selectedPaths()`,
`currentIndex()`, `currentView()`, `setDetailsMode()`.

Double-click emits two different signals by design:
- normal pages → `fileActivated(sourceIndex)`
- search page → `pathActivated(path)`, since there is no source index to give

Navigating anywhere clears search mode.

---

## panel/ — chrome around the listing

### `toolbar.cpp`
Path bar, nav buttons, search field, and in the top-right: the **type combo**
(All types / Images / Videos / Audio / Documents / Archives / Discs) and the
**date-range button**. Emits `typeFilterChanged(int)` and
`dateRangeChanged(from, to)`; `MainWindow` decides what to do with them.

### `sidebar.cpp`
Places, Devices, Bookmarks.

- **Places** are XDG dirs. `addXdgPlace()` skips any that resolve to `$HOME` —
  an XDG dir that was never created (e.g. `~/Music`) returns `$HOME`, which would
  otherwise show a second "Home" entry under a misleading name. **This is why
  Music is absent: the folder does not exist. Create `~/Music` and it appears.**
- **Devices** come from `QStorageInfo::mountedVolumes()`, filtered down to
  removable paths and rclone/FUSE cloud mounts. `deviceLabel()` resolves the real
  disk label through `/dev/disk/by-label` symlinks, because `QStorageInfo::name()`
  returns empty here and volumes would otherwise display as raw UUIDs.
- **Bookmarks** persist to `~/.config/swordfm/bookmarks.json`.

List heights are computed from `sizeHintForRow()` rather than hard-coded;
scrollbars are disabled, so an under-estimate silently swallows the last entries.

### `previewpanel.cpp`
A `QStackedWidget` over four pages: empty, text, image (in a `QScrollArea`), and
a stub for unpreviewable files. Handles:

- **Text / code** — up to 2 MB, rejected if NUL bytes appear in the first 8 KB.
- **Images** — the full-resolution pixmap is kept in `m_sourcePixmap` so zooming
  in re-samples the original instead of magnifying a downscaled copy.
- **PDF** — shelled out to `pdftoppm -png -r 150 -f 1 -l 1` (poppler), which
  writes `<prefix>-1.png`. Async; the completion handler checks the selection
  hasn't moved on before displaying.
- **Folder graphs** — runs `swordgraph` and displays the PNG.

Zoom (`−` / `+` / `⤢` in the header) applies to whichever pixmap is showing, so
images, PDFs and graphs all zoom with the same code. The buttons hide themselves
on text and stub pages.

### `statusbar.cpp`
Item count · search info (purple) · mark count (amber) · selection · clipboard.

---

## ops/ — actions on files

### `fileops.cpp`
Free functions: `copyFiles`, `moveFiles`, `deleteFileOrDir`, `selectedTotalSize`,
`uniqueDestPath`. No UI, no dialogs — callers handle confirmation.

### `contextmenu.cpp`
Builds the right-click menu and wires entries straight to `MainWindow` slots.
"Open in SwordGraph" sits directly above "Open in Terminal". Archive entries
("Extract Here", "Extract to Subfolder", and the "Compress" submenu) appear
between the mark actions and cut/copy/paste. Extract only shows when *every*
selected item is an archive, so it is never a no-op.

### `archiveops.cpp`
Create and extract archives by shelling out to the standard CLI tools.

`availableFormats()` probes the system and only offers formats whose tool is
actually installed, so the Compress submenu never lists something that will
fail. Supported: zip, tar.gz, tar.xz, tar.bz2, tar.zst, tar, 7z. Extraction
additionally handles rar and bare gz/bz2/xz/zst. If Info-ZIP `zip` is missing
but `7z` is present, 7z transparently covers the zip case.

Two details worth preserving:

- **Arguments are always passed as a `QStringList`, never a shell string.** A
  file named `weird; name.txt` must be one literal argument; building a command
  line by concatenation would execute it. There is no shell in this path.
- **Compression runs from the items' common parent** and passes bare filenames,
  so the archive stores `photos/a.png` instead of `home/sword/Pictures/photos/a.png`.

Suffix matching tries longest-first, so `.tar.gz` wins over `.gz` — otherwise a
tarball would be treated as a plain gzip stream and unpack to a single blob.

### `convertops.cpp`
Document conversion between PDF, DOCX, Markdown, TXT and HTML, delegated to the
`swordconv` Python helper (see below). `conversionTargetsFor()` omits the file's
own format so the menu never offers a no-op, and the "Convert To" submenu only
appears when every selected file is the same kind of document.

Output goes beside the original via `uniqueDestPath()`, so converting never
overwrites anything.

### `shareops.cpp`
`ShareDialog` — serve a file or folder to your phone over the LAN. Unlike every
other helper, `swordshare` is **long-lived**: it keeps listening until stopped.
The dialog therefore owns the process and kills it in its destructor, so closing
the window always stops the server. There is no way to leave one running
invisibly.

The dialog shows a QR code, the URL and a six-digit code, and copies the URL to
the clipboard. When a device enters the code, an **Allow / Deny** row appears
naming its address; only after Allow does that device get in, and only one
device may hold the share at a time. **Disconnect Device** frees the slot for
another. The code is hidden once a device is paired — it is spent, and leaving it
on screen only invites shoulder-surfing.

Communication is line-delimited JSON: events (`ready`, `claim`, `paired`,
`rejected`, `released`) arrive on the helper's stdout, and answers (`allow`,
`deny`, `release`, `quit`) go back on its stdin.

It appears in the context menu on a single selection ("Share over Network") and
on empty space ("Share This Folder").

### `openwith.cpp`
Freedesktop `.desktop` handling — parses MIME associations, reads
`mimeapps.list` for defaults, launches via the `Exec=` line. Includes a
preferred-video-player fallback list for when the registered default is wrong.

### `termutil.cpp`
`openTerminalAt()`, `openInYazi()`, and `isPreviewableFile()`.

---

## Companion tool: `swordconv`

Python script in the parent project directory, installed to `~/.local/bin`.
Converts between PDF, DOCX, Markdown, TXT and HTML.

```
swordconv <pdf|docx|md|txt|html> <input> <output>
```

**No LibreOffice or pandoc dependency**, deliberately — those are hundreds of
megabytes and slow to start. Instead:

| Direction | Engine |
|-----------|--------|
| anything → PDF | PyMuPDF `Story` (lays HTML out across pages, keeps formatting) |
| PDF → DOCX | `pdf2docx` (preserves layout, tables, images) |
| DOCX → html/md/txt | `mammoth` |
| PDF → md/txt/html | PyMuPDF text extraction |
| Markdown → html | `markdown` if installed, else a built-in subset |

Two details in the PDF reader worth keeping:

- A PDF has no headings, only text at different sizes. The reader takes the
  **most common font size as body text** and promotes anything meaningfully
  larger back to a heading — without this, converted output is one flat wall of
  paragraphs.
- Extraction uses `TEXT_DEHYPHENATE`, which also **expands ligatures**.
  Otherwise "first" comes back as the single glyph `ﬁrst` and every later
  search for it fails.

Bold detection requires the *whole* line to be bold. Testing "contains a bold
span" turned every paragraph with an emphasised phrase into a heading.

Errors go to stderr in plain English and SwordFM shows them verbatim, so a
password-protected or scanned PDF produces an explanation rather than a
mysterious failure.

## Companion tool: `swordshare`

Python script in the parent project directory, installed to `~/.local/bin`.
Serves one file or folder over HTTP to devices on the same network.

```
swordshare <path> [--port N] [--no-upload]
```

It prints **one JSON line per event** on stdout — `ready` (carrying `url`, `ip`,
`port`, `password`, `qr`, `upload`, `name`), then `claim` / `paired` / `rejected`
/ `released` as devices come and go — and reads commands (`allow`, `deny`,
`release`, `quit`) on stdin. `ShareDialog` drives both ends.

> The stdin reader uses `readline()`, not `for line in sys.stdin`. The latter's
> read-ahead buffering holds a command back until enough further input arrives,
> which here is never — the first `allow` would simply never be seen.

**HTTP, not FTP** — every current phone browser removed `ftp://` support, so a
QR code containing an FTP link simply fails to open. HTTP opens in the browser
that just scanned the code.

Design points worth keeping:

- **Exactly one device may be connected, and the desktop approves it.** The
  correct password only earns a *claim*; `Share.claim()` parks the address as
  pending and emits a `claim` event, and nothing is served until SwordFM answers
  `allow`. Without this, anyone who glimpsed the QR code over your shoulder is as
  authorised as your own phone and you would never know they were there. Every
  request re-checks the client address, so a stolen cookie is useless from an
  unapproved device. `release` clears the pairing *and* rotates the signing key,
  invalidating the old session.
- **Binds to the LAN address, never `0.0.0.0`.** `lan_ip()` gets it by
  connecting a UDP socket (no packet is sent); resolving the hostname instead
  usually just returns `127.0.0.1`.
- **Every path goes through `Share.resolve()`**, which calls `realpath()` *before*
  the prefix check — so neither `../` nor a symlink pointing outside the shared
  folder can reach the rest of the disk. Sharing a single file additionally
  404s everything else in its directory.
- **Sessions are HMAC-signed cookies** over a per-run random key, so a forged
  cookie cannot be constructed and every restart invalidates old ones.
- **Uploads are streamed to disk**, not buffered. `cgi` was removed in Python
  3.13 and the stdlib alternatives hold the whole body in memory, which a phone
  uploading a video would not survive. `BodyReader` exists because mixing
  `readline()` with `read()` on the raw socket loses bytes already pulled past
  a part boundary — that dropped every file after the first. The CRLF ending a
  boundary line must also be stripped from the pushed-back tail, or the next
  part reads as unnamed.
- **`SIGTERM` is turned into a normal exit** so the temporary QR image is
  cleaned up. Without it, terminating the server leaves `/tmp` littered.

The password is six digits shown as two groups; the login comparison strips
spaces, so it can be typed either way.

## Companion tool: `swordgraph`

Python script in the parent project directory (`../swordgraph`), also installed
to `~/.local/bin`. Renders a directory tree as a graphviz/neato diagram.

```
swordgraph --out /tmp/graph.png ~/some/project
```

**On resolution:** `size` is an upper bound in *inches* and `dpi` converts it to
pixels. Raising dpi while proportionally shrinking size cancels out and produces
no supersampling — this was the original blur bug. The fix holds `size` at
`width/96` and raises `dpi` to `96 * scale` (scale=4), supersampling nodes, edges
and text alike for a crisp downscale.

---

## Where to make common changes

| Goal | File |
|------|------|
| Add a searchable file type | `model/filefilter.cpp` → `suffixesFor()` |
| Change what search results show | `model/searchmodel.cpp` → `appendBatch()` |
| Tune search speed / traversal rules | `model/filescanner.cpp` → `run()` |
| Add a toolbar control | `panel/toolbar.cpp` + a `MainWindow` connect |
| Add a context-menu entry | `ops/contextmenu.cpp` + a `MainWindow` slot |
| Add an archive format | `ops/archiveops.cpp` → `availableFormats()` + `compressTo()` |
| Add a conversion format | `swordconv` → `READERS` + a writer |
| Change what the share server serves | `swordshare` → `Handler` / `Share.resolve()` |
| Add a preview format | `panel/previewpanel.cpp` → `previewFile()` |
| Change colors | `app/theme.h` |
| Hide more filesystem junk | `model/filefilter.cpp` → `isJunkName()` |
| Add a sidebar section | `panel/sidebar.cpp` → `rebuildPlaces()` |
| Add a keyboard shortcut | `app/mainwindow.cpp` → `setupMenus()` |

## Gotchas worth knowing

- `QFileSystemModel` populates **asynchronously**. Anything touching a directory
  right after `setRootPath()` must wait for `directoryLoaded`.
- **Never give a key both a `QShortcut` and a menu action.** Two objects claiming
  one sequence makes it *ambiguous*, and Qt responds by firing neither — silently.
  This had broken Ctrl+A, Space, Ctrl+C/X/V, Delete and F2 all at once. Menu
  actions own their keys; the constructor's `QShortcut` list is only for keys with
  no menu entry.
- Marks are keyed by absolute path, not index, so they survive navigation and
  model resets.
- The search page breaks the "everything is a filesystem index" assumption. New
  code reading the current selection needs an `m_searchMode` branch.
- Filters never hide directories, on purpose.
- Recursive search will not cross a mount point. If results from a cloud mount
  are missing, that is deliberate — navigate into the mount and search there.
- The share server is reachable only by the one device you approved, and only
  while the dialog is open. Closing it stops the server; there is no background
  mode.
