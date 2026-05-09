// ─────────────────────────────────────────────────────────────────────────────
// AppStyleSheet.h  –  SAI2-inspired dark theme for PixelCanvas
//
// Apply once in MainWindow constructor:  qApp->setStyleSheet(kAppStyleSheet);
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include <QString>

inline const QString kAppStyleSheet = QStringLiteral(R"(

/* ── Base ─────────────────────────────────────────────────────────────────── */
QWidget {
    background-color: #2b2b2b;
    color: #d0d0d0;
    font-family: "Segoe UI", "Noto Sans", sans-serif;
    font-size: 12px;
}

QMainWindow, QDialog {
    background-color: #252525;
}

/* ── Menu bar ─────────────────────────────────────────────────────────────── */
QMenuBar {
    background-color: #1e1e1e;
    color: #c8c8c8;
    border-bottom: 1px solid #111;
    padding: 1px 0;
}
QMenuBar::item:selected {
    background: #3d3d3d;
}
QMenu {
    background-color: #2b2b2b;
    border: 1px solid #111;
    padding: 2px 0;
}
QMenu::item {
    padding: 4px 24px 4px 16px;
    color: #d0d0d0;
}
QMenu::item:selected {
    background: #4a6fa5;
    color: #fff;
}
QMenu::separator {
    height: 1px;
    background: #3a3a3a;
    margin: 2px 6px;
}

/* ── Toolbar / Quick bar ──────────────────────────────────────────────────── */
QToolBar {
    background-color: #232323;
    border-bottom: 1px solid #111;
    spacing: 2px;
    padding: 2px 4px;
}
QToolBar QToolButton {
    background: transparent;
    border: none;
    color: #c8c8c8;
    padding: 2px 6px;
    border-radius: 3px;
}
QToolBar QToolButton:hover {
    background: #3a3a3a;
}
QToolBar QToolButton:pressed {
    background: #4a4a4a;
}

/* ── Dock widgets ─────────────────────────────────────────────────────────── */
QDockWidget {
    color: #c8c8c8;
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
}
QDockWidget::title {
    background: #1e1e1e;
    padding: 4px 8px;
    border-bottom: 1px solid #111;
    font-size: 11px;
    color: #aaa;
}

/* ── Scroll bars ──────────────────────────────────────────────────────────── */
QScrollBar:vertical {
    background: #222;
    width: 8px;
    margin: 0;
    border-radius: 4px;
}
QScrollBar::handle:vertical {
    background: #555;
    min-height: 20px;
    border-radius: 4px;
}
QScrollBar::handle:vertical:hover { background: #777; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }

QScrollBar:horizontal {
    background: #222;
    height: 8px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal {
    background: #555;
    min-width: 20px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal:hover { background: #777; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }

/* ── Combo boxes ──────────────────────────────────────────────────────────── */
QComboBox {
    background: #333;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 2px 6px;
    color: #d0d0d0;
    min-height: 20px;
}
QComboBox:hover { border-color: #777; }
QComboBox::drop-down { border: none; width: 16px; }
QComboBox QAbstractItemView {
    background: #2b2b2b;
    border: 1px solid #555;
    selection-background-color: #4a6fa5;
    color: #d0d0d0;
}

/* ── Spin boxes ───────────────────────────────────────────────────────────── */
QSpinBox, QDoubleSpinBox {
    background: #333;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 1px 4px;
    color: #d0d0d0;
    min-height: 20px;
}
QSpinBox:hover, QDoubleSpinBox:hover { border-color: #777; }
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    background: #3a3a3a;
    border: none;
    width: 14px;
}

/* ── Sliders ──────────────────────────────────────────────────────────────── */
QSlider::groove:horizontal {
    background: #1a1a1a;
    height: 4px;
    border-radius: 2px;
    border: 1px solid #444;
}
QSlider::handle:horizontal {
    background: #7a9fd4;
    width: 10px;
    height: 10px;
    margin: -4px 0;
    border-radius: 5px;
}
QSlider::handle:horizontal:hover { background: #9ab8e8; }
QSlider::sub-page:horizontal {
    background: #4a6fa5;
    border-radius: 2px;
}

/* ── Push / Tool buttons ──────────────────────────────────────────────────── */
QPushButton {
    background: #383838;
    border: 1px solid #555;
    border-radius: 3px;
    color: #d0d0d0;
    padding: 3px 8px;
    min-height: 20px;
}
QPushButton:hover { background: #424242; border-color: #777; }
QPushButton:pressed { background: #2a2a2a; }
QPushButton:disabled { color: #666; }

QToolButton {
    background: #383838;
    border: 1px solid #555;
    border-radius: 3px;
    color: #d0d0d0;
    padding: 2px;
}
QToolButton:hover { background: #424242; }
QToolButton:checked { background: #4a6fa5; border-color: #5b83c0; color: #fff; }
QToolButton:pressed { background: #2a2a2a; }

/* ── List widgets ─────────────────────────────────────────────────────────── */
QListWidget {
    background: #1e1e1e;
    border: 1px solid #444;
    color: #d0d0d0;
    outline: none;
}
QListWidget::item:selected {
    background: #4a6fa5;
    color: #fff;
}
QListWidget::item:hover {
    background: #333;
}

/* ── Labels ───────────────────────────────────────────────────────────────── */
QLabel {
    color: #c0c0c0;
    background: transparent;
}

/* ── Status bar ───────────────────────────────────────────────────────────── */
QStatusBar {
    background: #1a1a1a;
    color: #999;
    border-top: 1px solid #111;
    font-size: 11px;
}

/* ── Splitter ─────────────────────────────────────────────────────────────── */
QSplitter::handle {
    background: #3a3a3a;
}
QSplitter::handle:hover {
    background: #4a6fa5;
}

/* ── Frames / separators ──────────────────────────────────────────────────── */
QFrame[frameShape="4"],  /* HLine */
QFrame[frameShape="5"] { /* VLine */
    color: #3a3a3a;
}

/* ── Scroll area ──────────────────────────────────────────────────────────── */
QScrollArea {
    background: transparent;
    border: none;
}

/* ── Dialog ───────────────────────────────────────────────────────────────── */
QDialogButtonBox QPushButton {
    min-width: 64px;
}

)");
