#pragma once
#include <QString>

// Sworddeck / One Dark palette
namespace Theme {
inline constexpr const char *BG      = "#282c34";
inline constexpr const char *BG2     = "#21252b";
inline constexpr const char *DIM     = "#3e4451";
inline constexpr const char *FG      = "#abb2bf";
inline constexpr const char *FG_DIM  = "#5c6370";
inline constexpr const char *CYAN    = "#61afef";
inline constexpr const char *GREEN   = "#98c379";
inline constexpr const char *AMBER   = "#e5c07b";
inline constexpr const char *RED     = "#e06c75";
inline constexpr const char *PURPLE  = "#c678dd";
inline constexpr const char *BORDER  = "#3e4451";
inline constexpr const char *HOVER   = "#2c313c";
inline constexpr const char *SELECT  = "#3e4451";
inline constexpr const char *SELECT_FG = "#61afef";

inline QString appStylesheet() {
    return QString(R"(
QWidget {
  background-color: %1;
  color: %2;
  font-size: 13px;
}
QMainWindow, QMenuBar {
  background-color: %1;
  color: %2;
}
QMenuBar::item {
  background: transparent;
  padding: 4px 10px;
}
QMenuBar::item:selected {
  background: %3;
  color: %4;
}
QMenu {
  background-color: %5;
  color: %2;
  border: 1px solid %6;
}
QMenu::item {
  padding: 6px 24px 6px 16px;
}
QMenu::item:selected {
  background: %3;
  color: %4;
}
QMenu::separator {
  height: 1px;
  background: %6;
  margin: 4px 8px;
}
QSplitter::handle {
  background: %6;
  width: 1px;
}
QScrollBar:vertical {
  background: %1;
  width: 10px;
  margin: 0;
}
QScrollBar::handle:vertical {
  background: %3;
  border-radius: 4px;
  min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: %4; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
  background: %1;
  height: 10px;
}
QScrollBar::handle:horizontal {
  background: %3;
  border-radius: 4px;
  min-width: 24px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QToolTip {
  background: %5;
  color: %2;
  border: 1px solid %4;
}
QMessageBox, QInputDialog, QDialog {
  background-color: %1;
  color: %2;
}
QLineEdit {
  background: %5;
  color: %2;
  border: 1px solid %6;
  border-radius: 4px;
  padding: 5px 8px;
  selection-background-color: %3;
  selection-color: %4;
}
QLineEdit:focus { border-color: %4; }
)").arg(BG, FG, SELECT, CYAN, BG2, BORDER);
}
} // namespace Theme
