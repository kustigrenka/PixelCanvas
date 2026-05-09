// ─────────────────────────────────────────────────────────────────────────────
// ProjectIO  –  scratchpad persistence additions (Phase 12)
//
// Add these fragments to your existing ProjectIO::saveProject() and
// loadProject() implementations.  The scratchpad image is stored as
// "scratch.png" inside the .paint ZIP archive alongside the layer PNGs.
//
// In saveProject() — after saving layer PNGs, before closing the ZIP:
// ─────────────────────────────────────────────────────────────────────────────

// ── In saveProject() ─────────────────────────────────────────────────────────
//
//   QImage scratchImg = m_colorSwatchWidget->scratchImage();
//   if (!scratchImg.isNull())
//   {
//       QByteArray pngData;
//       QBuffer buf(&pngData);
//       buf.open(QIODevice::WriteOnly);
//       scratchImg.save(&buf, "PNG");
//       buf.close();
//       zip.addFile("scratch.png", pngData);   // your zip helper
//   }
//
//   // Also persist swatch colours in project.json:
//   QJsonArray swatchArr;
//   for (const QColor &c : m_colorSwatchWidget->swatches())
//       swatchArr.append(c.isValid() ? c.name() : QString());
//   root["swatches"] = swatchArr;


// ── In loadProject() ─────────────────────────────────────────────────────────
//
//   // Restore swatches from JSON
//   if (root.contains("swatches"))
//   {
//       const QJsonArray arr = root["swatches"].toArray();
//       QVector<QColor> swatches(arr.size());
//       for (int i = 0; i < arr.size(); ++i)
//       {
//           const QString hex = arr[i].toString();
//           swatches[i] = hex.isEmpty() ? QColor() : QColor(hex);
//       }
//       m_colorSwatchWidget->setSwatches(swatches);
//   }
//
//   // Restore scratch image
//   if (zip.hasFile("scratch.png"))
//   {
//       QByteArray pngData = zip.readFile("scratch.png");
//       QImage img;
//       img.loadFromData(pngData, "PNG");
//       if (!img.isNull())
//           m_colorSwatchWidget->setScratchImage(img);
//   }


// ─────────────────────────────────────────────────────────────────────────────
// JSON schema addition
// ─────────────────────────────────────────────────────────────────────────────
//
// project.json (extended):
// {
//   "version": 1,
//   "canvasWidth": 2048,
//   "canvasHeight": 2048,
//   "layers": [ ... ],
//   "swatches": ["#ff0000", "#00ff00", "", "", ...]   ← "" means empty slot
// }
//
// scratch.png  ← added to the ZIP root alongside layer_0.png etc.
