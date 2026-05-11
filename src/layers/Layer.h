#pragma once

#include <QImage>
#include <QString>
#include <QPainter>

// ─────────────────────────────────────────────────────────────────────────────
// Layer  (Toma – Ph. 6)
//
// Plain data struct — no logic.  LayerStack owns all Layer instances.
// Always use QImage::Format_ARGB32_Premultiplied to avoid compositing halos.
// ─────────────────────────────────────────────────────────────────────────────

struct Layer
{
    QImage  pixels;                                                         // Format_ARGB32_Premultiplied
    QString name      = "Layer";
    qreal   opacity   = 1.0;
    QPainter::CompositionMode blendMode = QPainter::CompositionMode_SourceOver;
    bool    visible   = true;
};
