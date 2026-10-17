#pragma once

#include "core/Http.h"

#include <QImage>
#include <QPointer>
#include <QQuickAsyncImageProvider>
#include <QQuickImageProvider>
#include <QQuickItem>

namespace sc {

// image://remote/<url>~<referer> — covers and thumbnails, decoded at the
// requested size on a background thread through the disk cache.
class RemoteImageProvider : public QQuickAsyncImageProvider {
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
    static QString encode(const QString &url, const QString &referer);
};

// image://icon/<name>?color=#rrggbb — Lucide icons tinted to a theme colour.
class IconProvider : public QQuickImageProvider {
public:
    IconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

// A chapter page. Decodes off the GUI thread and draws the image as a stack
// of tiles, so very tall webtoon strips stay sharp without exceeding the
// GPU's texture size limit.
class PanelImage : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString referer READ referer WRITE setReferer NOTIFY sourceChanged)
    Q_PROPERTY(int maxDecodeWidth READ maxDecodeWidth WRITE setMaxDecodeWidth NOTIFY sourceChanged)
    Q_PROPERTY(int sourceWidth READ sourceWidth NOTIFY loaded)
    Q_PROPERTY(int sourceHeight READ sourceHeight NOTIFY loaded)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY statusChanged)

public:
    enum Status { Null, Loading, Ready, Error };
    Q_ENUM(Status)

    explicit PanelImage(QQuickItem *parent = nullptr);
    ~PanelImage() override;

    QString source() const { return m_source; }
    void setSource(const QString &source);
    QString referer() const { return m_referer; }
    void setReferer(const QString &referer);
    int maxDecodeWidth() const { return m_maxDecodeWidth; }
    void setMaxDecodeWidth(int width);
    int sourceWidth() const { return m_size.width(); }
    int sourceHeight() const { return m_size.height(); }
    Status status() const { return m_status; }
    QString errorString() const { return m_error; }

    Q_INVOKABLE void reload();

signals:
    void sourceChanged();
    void loaded();
    void statusChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void start();
    // Starts once the current round of property changes is over, so a
    // source and its referer set together load together.
    void startSoon();
    void setStatus(Status status, const QString &error = {});

    QString m_source;
    QString m_referer;
    int m_maxDecodeWidth = 1600;
    QSize m_size;
    Status m_status = Null;
    QString m_error;
    QList<QImage> m_pendingTiles;
    bool m_tilesDirty = false;
    CancelFlag m_cancel;
    int m_generation = 0;
    bool m_startQueued = false;
};

} // namespace sc
