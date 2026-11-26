// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "camerapreview.h"
#include "previewreader.h"
#include <QCoreApplication>
#include <QHideEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>

namespace {
QtDisplayPreviewCallback resolver = nullptr;
void *resolverData = nullptr;
}

void CameraPreview::setSourceResolver(QtDisplayPreviewCallback callback, void *userData) {
    resolver = callback;
    resolverData = userData;
}

CameraPreview::CameraPreview(QWidget *parent) : QLabel(parent), m_timer(new QTimer(this)) {
    setObjectName("camera-preview");
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_timer->setInterval(30);
    connect(m_timer, &QTimer::timeout, this, &CameraPreview::tick);
}

CameraPreview::~CameraPreview() = default;

QString CameraPreview::nodeName(const QString &id) {
    const auto bytes = id.toUtf8();
    if (bytes.isEmpty() || bytes.size() > 128) return {};
    return QStringLiteral("vauchi.camera.%1.%2").arg(QCoreApplication::applicationPid())
        .arg(QString::fromLatin1(bytes.toHex()));
}

QImage CameraPreview::frameFromRgba(const QByteArray &bytes, uint32_t width, uint32_t height,
                                   int stride, uint32_t offset) {
    if (width == 0 || height == 0 || width > 1920 || height > 1080
        || stride < 0 || uint64_t(stride) < uint64_t(width)*4) return {};
    const uint64_t end = uint64_t(offset) + uint64_t(height-1)*uint64_t(stride) + uint64_t(width)*4;
    if (end > uint64_t(bytes.size())) return {};
    // PipeWire reuses the mapped buffer as soon as its callback returns.
    return QImage(reinterpret_cast<const uchar *>(bytes.constData()) + offset,
                  int(width), int(height), stride, QImage::Format_RGBA8888).copy();
}

void CameraPreview::setPreviewId(const QString &id) {
    const auto next = nodeName(id).isEmpty() ? QString{} : id;
    if (m_previewId == next) return;
    m_previewId = next;
    m_reader.reset();
    m_frame = {};
    clear();
    updateGeometry();
    if (isVisible() && !m_previewId.isEmpty()) {
        m_timer->start();
    }
}

void CameraPreview::tick() {
    if (!m_reader && resolver && !m_previewId.isEmpty()) {
        QtDisplayPreviewSource source{UINT32_MAX, 0, -1};
        const auto id = m_previewId.toUtf8();
        const auto result = resolver(id.constData(), &source, resolverData);
        if (result == 1) {
            m_reader = std::make_unique<PreviewReader>(source);
        } else if (result != 0) {
            m_timer->stop();
        }
    }
    if (!m_reader) return;
    if (auto frame = m_reader->poll()) {
        m_frame = *frame;
        showFrame();
        updateGeometry();
    }
    if (m_reader->closed()) {
        m_reader.reset();
        m_timer->stop();
        m_frame = {};
        clear();
        updateGeometry();
    }
}

QSize CameraPreview::sizeHint() const { return m_frame.size(); }

void CameraPreview::showFrame() {
    if (!m_frame.isNull())
        setPixmap(QPixmap::fromImage(m_frame).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CameraPreview::resizeEvent(QResizeEvent *event) {
    QLabel::resizeEvent(event);
    showFrame();
}

void CameraPreview::showEvent(QShowEvent *event) {
    QLabel::showEvent(event);
    if (!m_previewId.isEmpty() && !m_reader) {
        m_timer->start();
    }
}

void CameraPreview::hideEvent(QHideEvent *event) {
    m_timer->stop();
    m_reader.reset();
    m_frame = {};
    clear();
    QLabel::hideEvent(event);
}
