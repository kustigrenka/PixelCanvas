#pragma once

#include <QObject>
#include <QString>

class LayerStack;

// ─────────────────────────────────────────────────────────────────────────────
// ProjectIO  (Toma – Ph. 12)
//
// .paint format: per-layer PNGs + project.json, zipped into a single file.
//
// JSON schema:
// {
//   "version": 1,
//   "canvasWidth": 2048,
//   "canvasHeight": 2048,
//   "layers": [
//     { "name": "Background", "opacity": 1.0,
//       "blendMode": "SourceOver", "visible": true, "file": "layer_0.png" }
//   ]
// }
//
// CMake requirement — expose Qt6 private headers for QZipWriter/QZipReader:
//   target_include_directories(PixelCanvas PRIVATE
//       ${Qt6Core_PRIVATE_INCLUDE_DIRS})
// ─────────────────────────────────────────────────────────────────────────────
class ProjectIO : public QObject
{
    Q_OBJECT

public:
    explicit ProjectIO(QObject *parent = nullptr);

    void setLayerStack(LayerStack *ls) { m_layerStack = ls; }

    // Returns true on success. Uses QSaveFile for atomic writes.
    bool saveProject(const QString &path);

    // Returns true on success. Reinitialises LayerStack from the .paint bundle.
    bool loadProject(const QString &path);

    // Composites all visible layers and writes PNG/JPEG (format from extension).
    bool exportFlat(const QString &path);

signals:
    // Emitted after a successful loadProject() so MainWindow can refresh the UI.
    void projectLoaded();

private:
    LayerStack *m_layerStack = nullptr;
};
