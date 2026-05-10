// ─────────────────────────────────────────────────────────────────────────────
// MainWindowCanvas.cpp
//
// Canvas operation dialog slots for MainWindow:
//   onNewCanvas(), onCanvasResize(), onCanvasExtend(), onCanvasCrop()
//
// Each slot opens a modal dialog, validates input, then calls the appropriate
// LayerStack method followed by CanvasWidget::reinitCanvas().
// ─────────────────────────────────────────────────────────────────────────────
#include "MainWindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QRadioButton>
#include <QButtonGroup>

#include "LayerStack.h"
#include "CanvasWidget.h"

// ── New canvas ────────────────────────────────────────────────────────────────
void MainWindow::onNewCanvas()
{
    if (!maybeSave()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("New Canvas"));
    dlg.setFixedWidth(300);

    auto *form = new QGridLayout(&dlg);
    form->setColumnStretch(1, 1);

    auto *wSpin = new QSpinBox(&dlg);
    wSpin->setRange(1, 16384); wSpin->setValue(2048); wSpin->setSuffix(" px");

    auto *hSpin = new QSpinBox(&dlg);
    hSpin->setRange(1, 16384); hSpin->setValue(2048); hSpin->setSuffix(" px");

    auto *dpiSpin = new QSpinBox(&dlg);
    dpiSpin->setRange(72, 1200); dpiSpin->setValue(96); dpiSpin->setSuffix(" dpi");

    form->addWidget(new QLabel(tr("Width:"),      &dlg), 0, 0); form->addWidget(wSpin,   0, 1);
    form->addWidget(new QLabel(tr("Height:"),     &dlg), 1, 0); form->addWidget(hSpin,   1, 1);
    form->addWidget(new QLabel(tr("Resolution:"), &dlg), 2, 0); form->addWidget(dpiSpin, 2, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons, 3, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const QSize newSize(wSpin->value(), hSpin->value());
    m_layerStack->init(newSize);
    m_canvas->reinitCanvas();
    m_currentFile.clear();
    setWindowTitle(tr("PixelCanvas — New Canvas %1×%2")
                       .arg(newSize.width()).arg(newSize.height()));
}

// ── Resize canvas ─────────────────────────────────────────────────────────────
void MainWindow::onCanvasResize()
{
    const QSize cur = m_layerStack->canvasSize();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Resize Canvas"));
    dlg.setFixedWidth(320);

    auto *form = new QGridLayout(&dlg);
    form->setColumnStretch(1, 1);

    auto *wSpin = new QSpinBox(&dlg);
    wSpin->setRange(1, 16384); wSpin->setValue(cur.width());  wSpin->setSuffix(" px");

    auto *hSpin = new QSpinBox(&dlg);
    hSpin->setRange(1, 16384); hSpin->setValue(cur.height()); hSpin->setSuffix(" px");

    form->addWidget(new QLabel(tr("Width:"),  &dlg), 0, 0); form->addWidget(wSpin, 0, 1);
    form->addWidget(new QLabel(tr("Height:"), &dlg), 1, 0); form->addWidget(hSpin, 1, 1);

    // 3×3 anchor grid
    auto *anchorGroup = new QGroupBox(tr("Anchor (content origin)"), &dlg);
    auto *ag          = new QGridLayout(anchorGroup);
    auto *bg          = new QButtonGroup(&dlg);

    const char *labels[3][3] = {
        {"↖", "↑", "↗"},
        {"←", "·", "→"},
        {"↙", "↓", "↘"}
    };
    QRadioButton *btns[3][3];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            btns[r][c] = new QRadioButton(tr(labels[r][c]), anchorGroup);
            btns[r][c]->setFixedSize(36, 28);
            ag->addWidget(btns[r][c], r, c);
            bg->addButton(btns[r][c], r * 3 + c);
        }
    btns[0][0]->setChecked(true);
    form->addWidget(anchorGroup, 2, 0, 1, 2);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons, 3, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const QSize newSize(wSpin->value(), hSpin->value());
    if (newSize == cur) return;

    const int id  = bg->checkedId();
    const int col = id % 3;
    const int row = id / 3;
    const int dx  = (newSize.width()  - cur.width())  * col / 2;
    const int dy  = (newSize.height() - cur.height()) * row / 2;

    m_layerStack->resizeCanvas(newSize, QPoint(dx, dy));
    m_canvas->reinitCanvas();
}

// ── Extend canvas ─────────────────────────────────────────────────────────────
void MainWindow::onCanvasExtend()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Extend Canvas"));
    dlg.setFixedWidth(280);

    auto *form = new QGridLayout(&dlg);
    form->setColumnStretch(1, 1);

    auto makeSpin = [&](QWidget *parent) {
        auto *s = new QSpinBox(parent);
        s->setRange(0, 8192); s->setValue(0); s->setSuffix(" px");
        return s;
    };

    auto *topSpin    = makeSpin(&dlg);
    auto *bottomSpin = makeSpin(&dlg);
    auto *leftSpin   = makeSpin(&dlg);
    auto *rightSpin  = makeSpin(&dlg);

    form->addWidget(new QLabel(tr("Top:"),    &dlg), 0, 0); form->addWidget(topSpin,    0, 1);
    form->addWidget(new QLabel(tr("Bottom:"), &dlg), 1, 0); form->addWidget(bottomSpin, 1, 1);
    form->addWidget(new QLabel(tr("Left:"),   &dlg), 2, 0); form->addWidget(leftSpin,   2, 1);
    form->addWidget(new QLabel(tr("Right:"),  &dlg), 3, 0); form->addWidget(rightSpin,  3, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons, 4, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const int l = leftSpin->value(),  t = topSpin->value(),
              r = rightSpin->value(), b = bottomSpin->value();
    if (l + t + r + b == 0) return;

    m_layerStack->extendCanvas(l, t, r, b);
    m_canvas->reinitCanvas();
}

// ── Crop canvas ───────────────────────────────────────────────────────────────
void MainWindow::onCanvasCrop()
{
    const QSize cur = m_layerStack->canvasSize();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Crop Canvas"));
    dlg.setFixedWidth(280);

    auto *form = new QGridLayout(&dlg);
    form->setColumnStretch(1, 1);

    auto *xSpin = new QSpinBox(&dlg);
    xSpin->setRange(0, cur.width() - 1);  xSpin->setValue(0); xSpin->setSuffix(" px");

    auto *ySpin = new QSpinBox(&dlg);
    ySpin->setRange(0, cur.height() - 1); ySpin->setValue(0); ySpin->setSuffix(" px");

    auto *wSpin = new QSpinBox(&dlg);
    wSpin->setRange(1, cur.width());  wSpin->setValue(cur.width());  wSpin->setSuffix(" px");

    auto *hSpin = new QSpinBox(&dlg);
    hSpin->setRange(1, cur.height()); hSpin->setValue(cur.height()); hSpin->setSuffix(" px");

    connect(xSpin, QOverload<int>::of(&QSpinBox::valueChanged), [&](int x){
        wSpin->setMaximum(cur.width() - x);
    });
    connect(ySpin, QOverload<int>::of(&QSpinBox::valueChanged), [&](int y){
        hSpin->setMaximum(cur.height() - y);
    });

    form->addWidget(new QLabel(tr("X offset:"), &dlg), 0, 0); form->addWidget(xSpin, 0, 1);
    form->addWidget(new QLabel(tr("Y offset:"), &dlg), 1, 0); form->addWidget(ySpin, 1, 1);
    form->addWidget(new QLabel(tr("Width:"),    &dlg), 2, 0); form->addWidget(wSpin, 2, 1);
    form->addWidget(new QLabel(tr("Height:"),   &dlg), 3, 0); form->addWidget(hSpin, 3, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons, 4, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const QSize  newSize(wSpin->value(), hSpin->value());
    const QPoint origin(-xSpin->value(), -ySpin->value());

    if (newSize == cur && origin.isNull()) return;

    m_layerStack->resizeCanvas(newSize, origin);
    m_canvas->reinitCanvas();
}
