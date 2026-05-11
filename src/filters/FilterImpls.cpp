#include "Filter.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Conversion helpers
// ─────────────────────────────────────────────────────────────────────────────

// Convert a QImage (Format_ARGB32_Premultiplied) to a cv::Mat (CV_8UC4, BGRA).
//
// Qt stores pixels as ARGB in memory on little-endian → byte order is B G R A,
// which is exactly what OpenCV expects for CV_8UC4 / BGRA.
// We copy row-by-row so the cv::Mat owns its buffer and can be freely modified.
static cv::Mat qImageToMat(const QImage &img)
{
    const QImage src = (img.format() == QImage::Format_ARGB32_Premultiplied)
                       ? img
                       : img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    cv::Mat mat(src.height(), src.width(), CV_8UC4);
    for (int y = 0; y < src.height(); ++y)
        std::memcpy(mat.ptr(y), src.constScanLine(y),
                    static_cast<std::size_t>(src.width()) * 4);
    return mat;
}

// Wrap a CV_8UC4 cv::Mat back into a QImage without an extra copy.
// The returned QImage takes ownership via copy().
static QImage matToQImage(const cv::Mat &mat)
{
    Q_ASSERT(mat.type() == CV_8UC4);
    Q_ASSERT(mat.isContinuous());
    return QImage(mat.data,
                  mat.cols, mat.rows,
                  static_cast<qsizetype>(mat.step),
                  QImage::Format_ARGB32_Premultiplied).copy();
}

// ─────────────────────────────────────────────────────────────────────────────
// BlurFilter  –  Gaussian blur via OpenCV
// ─────────────────────────────────────────────────────────────────────────────

QImage BlurFilter::apply(const QImage &src) const
{
    if (m_radius <= 0) return src.copy();

    cv::Mat mat = qImageToMat(src);

    // Kernel size must be odd and ≥ 1.
    const int k = m_radius * 2 + 1;

    // Blur all 4 channels (matches Krita behaviour).
    cv::Mat blurred;
    cv::GaussianBlur(mat, blurred, cv::Size(k, k), 0, 0, cv::BORDER_REFLECT_101);

    return matToQImage(blurred);
}

// ─────────────────────────────────────────────────────────────────────────────
// BrightnessContrastFilter  –  LUT-based adjustment
// ─────────────────────────────────────────────────────────────────────────────

QImage BrightnessContrastFilter::apply(const QImage &src) const
{
    // Build a 256-entry LUT for the RGB channels.
    // Formula: out = clamp(in * contrast + brightness, 0, 255)
    std::uint8_t lut[256];
    for (int i = 0; i < 256; ++i)
    {
        const double v = static_cast<double>(i) * m_contrast + m_brightness;
        lut[i] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(v), 0, 255));
    }
    const cv::Mat lutMat(1, 256, CV_8UC1, lut);

    cv::Mat mat = qImageToMat(src);

    // Split into BGRA planes and apply the LUT to colour channels only.
    std::vector<cv::Mat> planes;
    cv::split(mat, planes);   // planes[0]=B, [1]=G, [2]=R, [3]=A

    cv::LUT(planes[0], lutMat, planes[0]);
    cv::LUT(planes[1], lutMat, planes[1]);
    cv::LUT(planes[2], lutMat, planes[2]);
    // planes[3] (alpha) is left untouched.

    cv::Mat result;
    cv::merge(planes, result);
    return matToQImage(result);
}

// ─────────────────────────────────────────────────────────────────────────────
// InvertFilter  –  invert RGB, keep alpha
// ─────────────────────────────────────────────────────────────────────────────

QImage InvertFilter::apply(const QImage &src) const
{
    // QImage::invertPixels(InvertRgb) inverts only RGB and leaves alpha intact,
    // avoiding a round-trip through OpenCV for this trivial operation.
    QImage result = src.copy();
    result.invertPixels(QImage::InvertRgb);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// SharpenFilter  –  unsharp mask via GaussianBlur + addWeighted
// ─────────────────────────────────────────────────────────────────────────────

QImage SharpenFilter::apply(const QImage &src) const
{
    cv::Mat mat = qImageToMat(src);

    // Split off alpha — only sharpen colour channels.
    std::vector<cv::Mat> planes;
    cv::split(mat, planes);   // [B, G, R, A]

    cv::Mat colour;
    cv::merge(std::vector<cv::Mat>{planes[0], planes[1], planes[2]}, colour);

    // Unsharp mask: sharpened = original × (1 + s) − blurred × s
    cv::Mat blurred;
    cv::GaussianBlur(colour, blurred, cv::Size(0, 0), 3.0, 0, cv::BORDER_REFLECT_101);

    cv::Mat sharpened;
    cv::addWeighted(colour,  1.0 + m_strength,
                    blurred, -m_strength,
                    0.0,     sharpened);

    // Reassemble with the original alpha channel.
    std::vector<cv::Mat> resultPlanes;
    cv::split(sharpened, resultPlanes);   // B, G, R
    resultPlanes.push_back(planes[3]);    // A

    cv::Mat result;
    cv::merge(resultPlanes, result);
    return matToQImage(result);
}
