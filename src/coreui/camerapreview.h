// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QImage>
#include <QLabel>
#include <memory>
#include <cstdint>
#include "qt_display.h"

class PreviewReader;
class NativePreviewReader;
class QTimer;
class QShowEvent;
class QHideEvent;

class CameraPreview : public QLabel {
    Q_OBJECT
    Q_PROPERTY(QString previewId READ previewId WRITE setPreviewId)
public:
    explicit CameraPreview(QWidget *parent = nullptr);
    ~CameraPreview() override;
    void setPreviewId(const QString &id);
    QString previewId() const { return m_previewId; }
    static QString nodeName(const QString &previewId);
    static void setSourceResolver(QtDisplayPreviewCallback callback, void *userData);
    static void setFrameResolver(QtDisplayFrameCallback callback, void *userData);
    static QImage frameFromRgba(const QByteArray &bytes, uint32_t width, uint32_t height,
                                int stride, uint32_t offset);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {}; }
protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    void tick();
    void tickNative();
    void showFrame();
    QString m_previewId;
    std::unique_ptr<PreviewReader> m_reader;
    std::unique_ptr<NativePreviewReader> m_nativeReader;
    uint64_t m_generation = 0;
    QImage m_frame;
    QTimer *m_timer;
};
