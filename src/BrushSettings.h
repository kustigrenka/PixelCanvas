#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// BrushSettings  –  all user-controllable parameters for the active brush.
//
// Extracted into its own header so ToolbarPanel, BrushPreviewWidget, and
// other UI code can include only this file without pulling in BrushEngine.h
// (which depends on Stroke.h and BrushTip.h).
// ─────────────────────────────────────────────────────────────────────────────
struct BrushSettings
{
    float size        = 20.0f;  // diameter in canvas pixels
    float opacity     = 1.0f;   // 0.0–1.0
    float hardness    = 0.90f;  // 0.0 (soft) – 1.0 (hard)
    float spacing     = 0.08f;  // dab spacing as fraction of diameter (0.05–2.0)
    float smoothing   = 0.7f;   // input smoothing weight (0 = none, 1 = max)

    // Pressure dynamics
    float sizeDynamics    = 1.0f;    // always 1 — min/max range controlled by minSizeFraction
    float minSizeFraction = 0.01f;   // pressure 0.0 gives this fraction of full size (0.0–1.0)
    float opacityDynamics = 1.0f;

    // Brush type — used by ToolbarPanel to switch tips
    enum class TipType { Pixel, Eraser, Airbrush, Smudge, Chalk } tipType = TipType::Pixel;
};
