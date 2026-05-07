#pragma once

#include <QImage>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// Filter  (Toma – Ph. 10)
//
// All filters take const QImage& and return a NEW QImage — never modify in place.
// OpenCV conversions: QImage(Format_ARGB32) ↔ cv::Mat(CV_8UC4) via bits().
// ─────────────────────────────────────────────────────────────────────────────
class Filter
{
public:
    virtual ~Filter() = default;

    virtual QImage  apply(const QImage &src) const = 0;
    virtual QString name() const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Concrete filters (implementations in FilterImpls.cpp – Ph. 10)
// ─────────────────────────────────────────────────────────────────────────────

class BlurFilter : public Filter
{
public:
    explicit BlurFilter(int radius = 3) : m_radius(radius) {}
    QImage  apply(const QImage &src) const override;
    QString name() const override { return "Gaussian Blur"; }
private:
    int m_radius;
};

class BrightnessContrastFilter : public Filter
{
public:
    explicit BrightnessContrastFilter(int brightness = 0, qreal contrast = 1.0)
        : m_brightness(brightness), m_contrast(contrast) {}
    QImage  apply(const QImage &src) const override;
    QString name() const override { return "Brightness / Contrast"; }
private:
    int   m_brightness; // -255 .. +255
    qreal m_contrast;   // 0.0 .. 3.0
};

class InvertFilter : public Filter
{
public:
    QImage  apply(const QImage &src) const override;
    QString name() const override { return "Invert"; }
};

class SharpenFilter : public Filter
{
public:
    explicit SharpenFilter(qreal strength = 1.0) : m_strength(strength) {}
    QImage  apply(const QImage &src) const override;
    QString name() const override { return "Sharpen"; }
private:
    qreal m_strength;
};
