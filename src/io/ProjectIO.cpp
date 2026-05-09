#include "ProjectIO.h"
#include "LayerStack.h"
#include "Layer.h"

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <zlib.h>   // ships with Qt / always present on Linux (zlib1g-dev)

// ─────────────────────────────────────────────────────────────────────────────
//  Minimal ZIP writer / reader
//
//  The ZIP format is simple enough to implement in ~120 lines:
//    - Local file header  (30 + name bytes)
//    - Deflated (or stored) file data
//    - Central directory  (46 + name bytes per entry)
//    - End-of-central-directory record (22 bytes)
//
//  We store entries DEFLATED (zlib deflate, wbits = -15).
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// ── CRC-32 via zlib ───────────────────────────────────────────────────────────
quint32 crc32Buf(const QByteArray &data)
{
    uLong crc = ::crc32(0L, Z_NULL, 0);
    crc = ::crc32(crc,
                  reinterpret_cast<const Bytef *>(data.constData()),
                  static_cast<uInt>(data.size()));
    return static_cast<quint32>(crc);
}

// ── Deflate raw (wbits = -15, no zlib header) ────────────────────────────────
QByteArray deflateRaw(const QByteArray &src)
{
    z_stream zs{};
    deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED,
                 -15, 8, Z_DEFAULT_STRATEGY);

    QByteArray out;
    out.resize(static_cast<int>(deflateBound(&zs,
                   static_cast<uLong>(src.size()))));

    zs.next_in  = reinterpret_cast<Bytef *>(
                      const_cast<char *>(src.constData()));
    zs.avail_in = static_cast<uInt>(src.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out= static_cast<uInt>(out.size());

    deflate(&zs, Z_FINISH);
    out.resize(static_cast<int>(zs.total_out));
    deflateEnd(&zs);
    return out;
}

// ── Inflate raw ──────────────────────────────────────────────────────────────
QByteArray inflateRaw(const QByteArray &src, quint32 uncompressedSize)
{
    QByteArray out(static_cast<int>(uncompressedSize), '\0');

    z_stream zs{};
    inflateInit2(&zs, -15);

    zs.next_in  = reinterpret_cast<Bytef *>(
                      const_cast<char *>(src.constData()));
    zs.avail_in = static_cast<uInt>(src.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out= static_cast<uInt>(out.size());

    inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    return out;
}

// ── Little-endian write helpers ───────────────────────────────────────────────
void writeU16(QByteArray &b, quint16 v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
}
void writeU32(QByteArray &b, quint32 v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
    b.append(static_cast<char>((v >> 16) & 0xFF));
    b.append(static_cast<char>((v >> 24) & 0xFF));
}
quint16 readU16(const QByteArray &b, int off)
{
    return static_cast<quint16>(
        (static_cast<quint8>(b[off])) |
        (static_cast<quint8>(b[off+1]) << 8));
}
quint32 readU32(const QByteArray &b, int off)
{
    return (static_cast<quint8>(b[off])) |
           (static_cast<quint8>(b[off+1]) << 8) |
           (static_cast<quint8>(b[off+2]) << 16) |
           (static_cast<quint8>(b[off+3]) << 24);
}

// ─────────────────────────────────────────────────────────────────────────────
//  ZipWriter
// ─────────────────────────────────────────────────────────────────────────────
struct ZipEntry {
    QByteArray name;
    quint32    crc;
    quint32    compSize;
    quint32    uncompSize;
    quint32    localOffset;
};

struct ZipWriter {
    QByteArray buf;
    QVector<ZipEntry> entries;

    void addFile(const QByteArray &name, const QByteArray &data)
    {
        const quint32 crc       = crc32Buf(data);
        const QByteArray comp   = deflateRaw(data);
        const quint32 localOff  = static_cast<quint32>(buf.size());

        // Local file header
        writeU32(buf, 0x04034b50u);   // signature
        writeU16(buf, 20);            // version needed
        writeU16(buf, 0);             // flags
        writeU16(buf, 8);             // compression: deflate
        writeU16(buf, 0);             // mod time
        writeU16(buf, 0);             // mod date
        writeU32(buf, crc);
        writeU32(buf, static_cast<quint32>(comp.size()));
        writeU32(buf, static_cast<quint32>(data.size()));
        writeU16(buf, static_cast<quint16>(name.size()));
        writeU16(buf, 0);             // extra field length
        buf.append(name);
        buf.append(comp);

        entries.push_back({name, crc,
                           static_cast<quint32>(comp.size()),
                           static_cast<quint32>(data.size()),
                           localOff});
    }

    QByteArray finalise()
    {
        const quint32 cdOffset = static_cast<quint32>(buf.size());

        for (const ZipEntry &e : entries) {
            writeU32(buf, 0x02014b50u);   // central dir signature
            writeU16(buf, 20);            // version made by
            writeU16(buf, 20);            // version needed
            writeU16(buf, 0);             // flags
            writeU16(buf, 8);             // deflate
            writeU16(buf, 0);             // mod time
            writeU16(buf, 0);             // mod date
            writeU32(buf, e.crc);
            writeU32(buf, e.compSize);
            writeU32(buf, e.uncompSize);
            writeU16(buf, static_cast<quint16>(e.name.size()));
            writeU16(buf, 0);             // extra
            writeU16(buf, 0);             // comment
            writeU16(buf, 0);             // disk start
            writeU16(buf, 0);             // internal attr
            writeU32(buf, 0);             // external attr
            writeU32(buf, e.localOffset);
            buf.append(e.name);
        }

        const quint32 cdSize = static_cast<quint32>(buf.size()) - cdOffset;

        // End of central directory record
        writeU32(buf, 0x06054b50u);
        writeU16(buf, 0);   // disk number
        writeU16(buf, 0);   // disk with CD
        writeU16(buf, static_cast<quint16>(entries.size()));
        writeU16(buf, static_cast<quint16>(entries.size()));
        writeU32(buf, cdSize);
        writeU32(buf, cdOffset);
        writeU16(buf, 0);   // comment length

        return buf;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ZipReader  – locate files via the central directory
// ─────────────────────────────────────────────────────────────────────────────
struct ZipReader {
    const QByteArray &data;

    // Find end-of-central-directory record (scan from end)
    bool findEOCD(int &eocdOff) const
    {
        for (int i = data.size() - 22; i >= 0; --i) {
            if (readU32(data, i) == 0x06054b50u) {
                eocdOff = i;
                return true;
            }
        }
        return false;
    }

    // Returns uncompressed bytes for the named entry, or empty on failure.
    QByteArray extractFile(const QByteArray &name) const
    {
        int eocdOff = 0;
        if (!findEOCD(eocdOff)) return {};

        const quint32 cdOffset = readU32(data, eocdOff + 16);
        const quint16 numFiles = readU16(data, eocdOff + 8);

        int pos = static_cast<int>(cdOffset);
        for (int i = 0; i < numFiles; ++i) {
            if (pos + 46 > data.size()) break;
            if (readU32(data, pos) != 0x02014b50u) break;

            const quint16 nameLen  = readU16(data, pos + 28);
            const quint16 extraLen = readU16(data, pos + 30);
            const quint16 commLen  = readU16(data, pos + 32);
            const quint32 compSz   = readU32(data, pos + 20);
            const quint32 uncompSz = readU32(data, pos + 24);
            const quint16 method   = readU16(data, pos + 10);
            const quint32 localOff = readU32(data, pos + 42);

            const QByteArray entryName =
                data.mid(pos + 46, nameLen);

            pos += 46 + nameLen + extraLen + commLen;

            if (entryName != name) continue;

            // Jump to local header to find actual data offset
            const int lhPos = static_cast<int>(localOff);
            if (lhPos + 30 > data.size()) return {};

            const quint16 lNameLen  = readU16(data, lhPos + 26);
            const quint16 lExtraLen = readU16(data, lhPos + 28);
            const int dataStart = lhPos + 30 + lNameLen + lExtraLen;

            const QByteArray compData =
                data.mid(dataStart, static_cast<int>(compSz));

            if (method == 0)   // stored
                return compData;

            return inflateRaw(compData, uncompSz);
        }
        return {};
    }
};

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Blend mode helpers
// ─────────────────────────────────────────────────────────────────────────────

static QString blendModeToString(QPainter::CompositionMode mode)
{
    switch (mode) {
    case QPainter::CompositionMode_SourceOver:  return QStringLiteral("SourceOver");
    case QPainter::CompositionMode_Multiply:    return QStringLiteral("Multiply");
    case QPainter::CompositionMode_Screen:      return QStringLiteral("Screen");
    case QPainter::CompositionMode_Overlay:     return QStringLiteral("Overlay");
    case QPainter::CompositionMode_Darken:      return QStringLiteral("Darken");
    case QPainter::CompositionMode_Lighten:     return QStringLiteral("Lighten");
    case QPainter::CompositionMode_ColorDodge:  return QStringLiteral("ColorDodge");
    case QPainter::CompositionMode_ColorBurn:   return QStringLiteral("ColorBurn");
    case QPainter::CompositionMode_HardLight:   return QStringLiteral("HardLight");
    case QPainter::CompositionMode_SoftLight:   return QStringLiteral("SoftLight");
    case QPainter::CompositionMode_Difference:  return QStringLiteral("Difference");
    case QPainter::CompositionMode_Exclusion:   return QStringLiteral("Exclusion");
    default:                                    return QStringLiteral("SourceOver");
    }
}

static QPainter::CompositionMode blendModeFromString(const QString &s)
{
    if (s == QLatin1String("Multiply"))   return QPainter::CompositionMode_Multiply;
    if (s == QLatin1String("Screen"))     return QPainter::CompositionMode_Screen;
    if (s == QLatin1String("Overlay"))    return QPainter::CompositionMode_Overlay;
    if (s == QLatin1String("Darken"))     return QPainter::CompositionMode_Darken;
    if (s == QLatin1String("Lighten"))    return QPainter::CompositionMode_Lighten;
    if (s == QLatin1String("ColorDodge")) return QPainter::CompositionMode_ColorDodge;
    if (s == QLatin1String("ColorBurn"))  return QPainter::CompositionMode_ColorBurn;
    if (s == QLatin1String("HardLight"))  return QPainter::CompositionMode_HardLight;
    if (s == QLatin1String("SoftLight"))  return QPainter::CompositionMode_SoftLight;
    if (s == QLatin1String("Difference")) return QPainter::CompositionMode_Difference;
    if (s == QLatin1String("Exclusion"))  return QPainter::CompositionMode_Exclusion;
    return QPainter::CompositionMode_SourceOver;
}

static QByteArray imageToPngBytes(const QImage &img)
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return bytes;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ProjectIO
// ─────────────────────────────────────────────────────────────────────────────

ProjectIO::ProjectIO(QObject *parent)
    : QObject(parent)
{}

bool ProjectIO::saveProject(const QString &path)
{
    if (!m_layerStack || m_layerStack->count() == 0)
        return false;

    const QSize sz = m_layerStack->canvasSize();
    const int   n  = m_layerStack->count();

    ZipWriter zip;

    // Build JSON manifest
    QJsonArray layerArray;
    for (int i = 0; i < n; ++i) {
        Layer *layer = m_layerStack->layerAt(i);
        if (!layer) continue;

        QJsonObject obj;
        obj[QStringLiteral("name")]      = layer->name;
        obj[QStringLiteral("opacity")]   = layer->opacity;
        obj[QStringLiteral("blendMode")] = blendModeToString(layer->blendMode);
        obj[QStringLiteral("visible")]   = layer->visible;
        obj[QStringLiteral("file")]      = QStringLiteral("layer_%1.png").arg(i);
        layerArray.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("version")]      = 1;
    root[QStringLiteral("canvasWidth")]  = sz.width();
    root[QStringLiteral("canvasHeight")] = sz.height();
    root[QStringLiteral("layers")]       = layerArray;

    zip.addFile("project.json",
                QJsonDocument(root).toJson(QJsonDocument::Indented));

    // Add layer PNGs
    for (int i = 0; i < n; ++i) {
        Layer *layer = m_layerStack->layerAt(i);
        if (!layer) continue;

        const QImage toSave =
            (layer->pixels.format() == QImage::Format_ARGB32_Premultiplied)
            ? layer->pixels
            : layer->pixels.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        const QByteArray entryName =
            QStringLiteral("layer_%1.png").arg(i).toUtf8();
        zip.addFile(entryName, imageToPngBytes(toSave));
    }

    // Atomic write
    QSaveFile saveFile(path);
    if (!saveFile.open(QIODevice::WriteOnly))
        return false;
    saveFile.write(zip.finalise());
    return saveFile.commit();
}

bool ProjectIO::loadProject(const QString &path)
{
    if (!m_layerStack)
        return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray fileData = f.readAll();
    f.close();

    ZipReader zip{fileData};

    const QByteArray jsonBytes = zip.extractFile("project.json");
    if (jsonBytes.isEmpty())
        return false;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject root    = doc.object();
    const int         version = root[QStringLiteral("version")].toInt(0);
    const int         canvasW = root[QStringLiteral("canvasWidth")].toInt(0);
    const int         canvasH = root[QStringLiteral("canvasHeight")].toInt(0);

    if (version < 1 || canvasW <= 0 || canvasH <= 0)
        return false;

    const QJsonArray layerArray = root[QStringLiteral("layers")].toArray();
    if (layerArray.isEmpty())
        return false;

    m_layerStack->init(QSize(canvasW, canvasH));

    bool firstLayer = true;
    for (const QJsonValue &val : layerArray) {
        const QJsonObject obj = val.toObject();

        const QString name     = obj[QStringLiteral("name")].toString(QStringLiteral("Layer"));
        const qreal   opacity  = obj[QStringLiteral("opacity")].toDouble(1.0);
        const QString blendStr = obj[QStringLiteral("blendMode")].toString();
        const bool    visible  = obj[QStringLiteral("visible")].toBool(true);
        const QString fileName = obj[QStringLiteral("file")].toString();

        const QByteArray pngBytes = zip.extractFile(fileName.toUtf8());
        if (pngBytes.isEmpty())
            return false;

        QImage img;
        if (!img.loadFromData(pngBytes, "PNG"))
            return false;

        if (img.format() != QImage::Format_ARGB32_Premultiplied)
            img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        const int idx = firstLayer ? 0 : m_layerStack->addLayer(name);
        firstLayer = false;

        Layer *layer = m_layerStack->layerAt(idx);
        if (!layer)
            return false;

        layer->pixels    = img;
        layer->name      = name;
        layer->opacity   = std::clamp(opacity, 0.0, 1.0);
        layer->blendMode = blendModeFromString(blendStr);
        layer->visible   = visible;
    }

    m_layerStack->setActiveLayer(0);
    emit projectLoaded();
    return true;
}

bool ProjectIO::exportFlat(const QString &path)
{
    if (!m_layerStack)
        return false;

    const QSize sz = m_layerStack->canvasSize();
    if (sz.isEmpty())
        return false;

    QImage composited(sz, QImage::Format_ARGB32_Premultiplied);
    composited.fill(Qt::transparent);
    m_layerStack->recompositeRect(composited, QRect(QPoint(0, 0), sz));

    return composited.save(path);
}
