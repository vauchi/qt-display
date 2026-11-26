// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QImage>
#include <QLabel>
#include <memory>
#include <cstdint>

class PreviewReader;
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
    void showFrame();
    QString m_previewId;
    std::unique_ptr<PreviewReader> m_reader;
    QImage m_frame;
    QTimer *m_timer;
};
