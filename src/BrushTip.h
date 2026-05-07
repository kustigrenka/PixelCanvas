#pragma once

#include <QImage>
#include <QPointF>
#include <QColor>
#include <QPainter>
#include <cmath>

struct DabParams
{
    QPointF  center;
    float    diameter;
    float    opacity;
    float    hardness;
    float    pressure;
    QColor   color;
};

class BrushTip
{
public:
    virtual ~BrushTip() = default;
    virtual void stamp(QPainter &p, const DabParams &dab) = 0;
    virtual QImage preview() { return {}; }
};

class PixelTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

class EraserTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

class AirbrushTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};

class SmudgeTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;

private:
    QImage m_sample;   // pixel sample picked up at stroke start / each dab
    bool   m_hasSample = false;
};

class ChalkTip : public BrushTip
{
public:
    void stamp(QPainter &p, const DabParams &dab) override;
};