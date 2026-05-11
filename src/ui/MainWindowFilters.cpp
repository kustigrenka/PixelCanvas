// ─────────────────────────────────────────────────────────────────────────────
// MainWindowFilters.cpp
//
// Filter menu dialog slots for MainWindow:
//   onFilterBlur(), onFilterBrightnessContrast(),
//   onFilterInvert(), onFilterSharpen()
//
// Each slot builds a small modal dialog, constructs the matching Filter with
// the user-chosen parameters, then calls CanvasWidget::applyFilter(). That
// method handles: push undo snapshot → apply → recomposite → update().
// ─────────────────────────────────────────────────────────────────────────────
#include "MainWindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include "Filter.h"
#include "CanvasWidget.h"

// ─────────────────────────────────────────────────────────────────────────────
// Gaussian Blur
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onFilterBlur()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Gaussian Blur"));
    dlg.setFixedWidth(280);

    auto *form = new QGridLayout(&dlg);
    form->setColumnStretch(1, 1);

    auto *radiusSpin = new QSpinBox(&dlg);
    radiusSpin->setRange(1, 100);
    radiusSpin->setValue(3);
    radiusSpin->setSuffix(tr(" px"));

    form->addWidget(new QLabel(tr("Radius:"), &dlg), 0, 0);
    form->addWidget(radiusSpin,                      0, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons, 1, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    BlurFilter filter(radiusSpin->value());
    m_canvas->applyFilter(&filter);
}

// ─────────────────────────────────────────────────────────────────────────────
// Brightness / Contrast
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onFilterBrightnessContrast()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Brightness / Contrast"));
    dlg.setFixedWidth(300);

    auto *form = new QGridLayout(&dlg);
    form->setColumnStretch(1, 1);

    auto *brightSpin = new QSpinBox(&dlg);
    brightSpin->setRange(-255, 255);
    brightSpin->setValue(0);

    auto *contrastSpin = new QDoubleSpinBox(&dlg);
    contrastSpin->setRange(0.0, 3.0);
    contrastSpin->setSingleStep(0.05);
    contrastSpin->setDecimals(2);
    contrastSpin->setValue(1.0);

    form->addWidget(new QLabel(tr("Brightness (-255 … +255):"), &dlg), 0, 0);
    form->addWidget(brightSpin,                                        0, 1);
    form->addWidget(new QLabel(tr("Contrast (0.0 … 3.0):"),    &dlg), 1, 0);
    form->addWidget(contrastSpin,                                      1, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons, 2, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    BrightnessContrastFilter filter(brightSpin->value(), contrastSpin->value());
    m_canvas->applyFilter(&filter);
}

// ─────────────────────────────────────────────────────────────────────────────
// Invert
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onFilterInvert()
{
    InvertFilter filter;
    m_canvas->applyFilter(&filter);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sharpen
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onFilterSharpen()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Sharpen"));
    dlg.setFixedWidth(280);

    auto *form = new QGridLayout(&dlg);
    form->setColumnStretch(1, 1);

    auto *strengthSpin = new QDoubleSpinBox(&dlg);
    strengthSpin->setRange(0.1, 5.0);
    strengthSpin->setSingleStep(0.1);
    strengthSpin->setDecimals(1);
    strengthSpin->setValue(1.0);

    form->addWidget(new QLabel(tr("Strength (0.1 … 5.0):"), &dlg), 0, 0);
    form->addWidget(strengthSpin,                                   0, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons, 1, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    SharpenFilter filter(strengthSpin->value());
    m_canvas->applyFilter(&filter);
}
