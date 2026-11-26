// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/camerapreview.h"
#include "coreui/previewbinding.h"
#include "coreui/previewreader.h"
#include <QApplication>
#include <QColor>
#include <cassert>
#include <limits>
#ifdef Q_OS_UNIX
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// @internal — the display consumes only its app's prepared preview stream.
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const auto expected = QStringLiteral("vauchi.camera.%1.63616d6572612d3432")
        .arg(QCoreApplication::applicationPid());
    assert(CameraPreview::nodeName("camera-42") == expected);
    assert(CameraPreview::nodeName({}).isEmpty());
    assert(CameraPreview::nodeName(QString(129, 'x')).isEmpty());
    PreviewBinding binding(expected);
    assert(!binding.observe(1, "webcam", "Video/Source", "101"));
    assert(!binding.observe(2, expected, "Audio/Source", "102"));
    assert(binding.observe(3, expected, "Video/Source", "103"));
    assert(binding.nodeId() == 3);
    assert(binding.serial() == "103");
    binding.remove(1);
    assert(binding.nodeId() == 3);
    binding.remove(3);
    assert(binding.retired());
    assert(!binding.observe(4, expected, "Video/Source", "104"));
    assert(binding.nodeId() == std::numeric_limits<uint32_t>::max());

    PreviewBinding descriptor(42, 1042);
    assert(!descriptor.observe(41, "same", "Video/Source", "1042"));
    assert(!descriptor.observe(42, "same", "Video/Source", "1043"));
    assert(!descriptor.observe(42, "same", "Audio/Source", "1042"));
    assert(descriptor.observe(42, "any", "Video/Source", "1042"));
    assert(descriptor.nodeId() == 42);
    assert(descriptor.serial() == "1042");
    descriptor.remove(42);
    assert(descriptor.retired());
    assert(!descriptor.observe(42, "any", "Video/Source", "1042"));
    PreviewBinding invalid(UINT32_MAX, 1);
    assert(!invalid.observe(UINT32_MAX, "", "Video/Source", "1"));
    PreviewBinding zero(42, 0);
    assert(!zero.observe(42, "", "Video/Source", "0"));

#ifdef Q_OS_UNIX
    for (const auto id : {uint32_t(42), UINT32_MAX}) {
        int sockets[2];
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
        {
            PreviewReader reader(QtDisplayPreviewSource{id, 1042, sockets[0]});
        }
        assert(fcntl(sockets[0], F_GETFD) == -1);
        assert(errno == EBADF);
        close(sockets[1]);
    }
#endif

    for (int padding = 0; padding < 8; ++padding) {
        for (int offset = 0; offset < 8; ++offset) {
            const int stride = 4 + padding;
            QByteArray bytes(offset + stride + 4, char(77));
            bytes.replace(offset, 4, QByteArray::fromHex("0a141eff"));
            bytes.replace(offset + stride, 4, QByteArray::fromHex("28323cff"));
            const auto image = CameraPreview::frameFromRgba(bytes, 1, 2, stride, offset);
            assert(image.size() == QSize(1, 2));
            assert(image.pixelColor(0, 0) == QColor(10, 20, 30, 255));
            assert(image.pixelColor(0, 1) == QColor(40, 50, 60, 255));
        }
    }
    assert(CameraPreview::frameFromRgba(QByteArray(8, 0), 0, 1, 4, 0).isNull());
    assert(CameraPreview::frameFromRgba(QByteArray(8, 0), 1, 0, 4, 0).isNull());
    assert(CameraPreview::frameFromRgba(QByteArray(8, 0), 1921, 1, 7684, 0).isNull());
    assert(CameraPreview::frameFromRgba(QByteArray(8, 0), 1, 1081, 4, 0).isNull());
    assert(CameraPreview::frameFromRgba(QByteArray(8, 0), 1, 1, -4, 0).isNull());
    assert(CameraPreview::frameFromRgba(QByteArray(8, 0), 1, 1, 3, 0).isNull());
    assert(CameraPreview::frameFromRgba(QByteArray(7, 0), 1, 2, 4, 0).isNull());
    assert(CameraPreview::frameFromRgba(QByteArray(8, 0), 1, 2, 4, UINT32_MAX).isNull());
    return 0;
}
