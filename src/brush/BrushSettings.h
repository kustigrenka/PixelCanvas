#pragma once

#include <QString>

enum class TipType
{
    Pixel, Airbrush, Brush, WaterColor, Marker,
    Eraser, SelPen, SelEraser, Bucket, Gradient,
    Blur, Smudge, Chalk,
};

enum class BrushBlendMode
{
    Normal, Multiply, Screen, Overlay,
    Luminosity, Shade, Vivid, Deep, Erase,
};

struct BrushSettings
{
    float size            = 20.0f;
    float sizeMultiplier  = 1.0f;
    float minSizeFraction = 0.01f;
    float opacity         = 1.0f;
    float minOpacity      = 0.0f;
    float hardness        = 0.90f;
    float spacing         = 0.08f;
    float smoothing       = 0.0f;   // 0=no stabilisation, 1=maximum lag

    BrushBlendMode blendMode  = BrushBlendMode::Normal;
    bool           keepOpacity = false;

    float blending        = 0.0f;
    float dilution        = 0.0f;
    float persistence     = 0.50f;
    float blurPressure    = 0.50f;
    float coloring        = 0.20f;
    float uncolorPressure = 0.90f;
    float blurWidth       = 0.50f;

    int   brushShape = 0;   // 0=Circle  1=Diamond  2=Square

    TipType tipType = TipType::Pixel;

    float effectiveDiameter() const { return size * sizeMultiplier; }

    float opacityAtPressure(float pressure) const
    {
        const float minO = opacity * minOpacity;
        return minO + (opacity - minO) * pressure;
    }

    float sizeAtPressure(float pressure) const
    {
        const float minD = effectiveDiameter() * minSizeFraction;
        return minD + (effectiveDiameter() - minD) * pressure;
    }

    bool isWetMedia()    const { return tipType==TipType::Brush || tipType==TipType::WaterColor || tipType==TipType::Marker; }
    bool hasBlurPressure() const { return tipType==TipType::WaterColor || tipType==TipType::Marker; }
    bool hasHardness()   const { return tipType!=TipType::Blur && tipType!=TipType::Bucket && tipType!=TipType::Gradient; }

    static constexpr const char* kKeySize            = "size";
    static constexpr const char* kKeySizeMul         = "sizeMultiplier";
    static constexpr const char* kKeyMinSize         = "minSizeFraction";
    static constexpr const char* kKeyOpacity         = "opacity";
    static constexpr const char* kKeyMinOpacity      = "minOpacity";
    static constexpr const char* kKeyHardness        = "hardness";
    static constexpr const char* kKeySpacing         = "spacing";
    static constexpr const char* kKeySmoothing       = "smoothing";
    static constexpr const char* kKeyBlendMode       = "blendMode";
    static constexpr const char* kKeyKeepOpacity     = "keepOpacity";
    static constexpr const char* kKeyBlending        = "blending";
    static constexpr const char* kKeyDilution        = "dilution";
    static constexpr const char* kKeyPersistence     = "persistence";
    static constexpr const char* kKeyBlurPressure    = "blurPressure";
    static constexpr const char* kKeyColoring        = "coloring";
    static constexpr const char* kKeyUncolorPressure = "uncolorPressure";
    static constexpr const char* kKeyBlurWidth       = "blurWidth";
    static constexpr const char* kKeyBrushShape      = "brushShape";
    static constexpr const char* kKeyTipType         = "tipType";
};

struct BrushPreset
{
    QString       name;
    BrushSettings settings;

    // Fixed: sizeMultiplier was 0.1f — caused sub-pixel dabs, no drawing on first click
    static BrushPreset makePixelPencil()
    {
        BrushPreset p; p.name="Pencil";
        p.settings.tipType=TipType::Pixel;
        p.settings.size=10.0f; p.settings.sizeMultiplier=1.0f;
        p.settings.hardness=0.95f; p.settings.spacing=0.05f;
        p.settings.smoothing=0.0f;
        return p;
    }
    static BrushPreset makeAirbrush()
    {
        BrushPreset p; p.name="Airbrush";
        p.settings.tipType=TipType::Airbrush; p.settings.size=50.0f;
        p.settings.hardness=0.0f; p.settings.spacing=0.05f; return p;
    }
    static BrushPreset makeBrush()
    {
        BrushPreset p; p.name="Brush";
        p.settings.tipType=TipType::Brush; p.settings.size=50.0f;
        p.settings.hardness=0.80f; p.settings.blending=0.50f;
        p.settings.persistence=0.50f; return p;
    }
    static BrushPreset makeWaterColor()
    {
        BrushPreset p; p.name="Water Color";
        p.settings.tipType=TipType::WaterColor; p.settings.size=50.0f;
        p.settings.hardness=0.85f; p.settings.blending=0.30f;
        p.settings.dilution=0.30f; p.settings.persistence=0.50f; return p;
    }
    static BrushPreset makeEraser()
    {
        BrushPreset p; p.name="Eraser";
        p.settings.tipType=TipType::Eraser; p.settings.size=50.0f;
        p.settings.hardness=0.85f; p.settings.blendMode=BrushBlendMode::Erase; return p;
    }
    static BrushPreset makeSmudge()
    {
        BrushPreset p; p.name="Smudge";
        p.settings.tipType=TipType::Smudge; p.settings.size=50.0f;
        p.settings.hardness=0.80f; p.settings.coloring=0.20f; return p;
    }
    static BrushPreset makeBlur()
    {
        BrushPreset p; p.name="Blur";
        p.settings.tipType=TipType::Blur; p.settings.size=50.0f;
        p.settings.blurWidth=0.50f; return p;
    }
};
