#pragma once

#include <QPointF>
#include <QVector>

// ─────────────────────────────────────────────────────────────────────────────
// StrokeSample  –  one input event captured during a stroke
// ─────────────────────────────────────────────────────────────────────────────
struct StrokeSample
{
    QPointF pos;           // canvas-space position
    float   pressure = 1.0f;  // 0.0–1.0  (1.0 when using mouse)
    float   tiltX    = 0.0f;  // degrees, +X tilts pen toward user right
    float   tiltY    = 0.0f;  // degrees, +Y tilts pen toward user
    float   rotation = 0.0f;  // barrel rotation in degrees (art pens)
};

// A stroke is just an ordered list of samples.
using Stroke = QVector<StrokeSample>;
