// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "coreui/nativepreviewreader.h"
#include "coreui/camerapreview.h"
#include <QApplication>
#include <QColor>
#include <QTemporaryFile>
#include <QtEndian>
#include <QElapsedTimer>
#include <cassert>
#include <fcntl.h>
#include <unistd.h>

static QByteArray header() {
    QByteArray bytes(32, 0);
    bytes.replace(0, 8, "VFRAME01");
    const uint32_t words[] = {2, 1, 8, 2};
    for (int i = 0; i < 4; ++i) qToLittleEndian(words[i], bytes.data() + 8 + 4*i);
    qToLittleEndian(uint64_t(1), bytes.data() + 24);
    return bytes;
}

struct Resource { QTemporaryFile *file; uint64_t generation; };
static int resolve(const char *id, uint64_t previous, QtDisplayFrameSource *source, void *data) {
    assert(QByteArray(id) == "scan");
    auto &resource = *static_cast<Resource *>(data);
    if (previous == resource.generation) return 2;
    source->generation = resource.generation;
    source->frame_fd = open(resource.file->fileName().toLocal8Bit().constData(), O_RDONLY | O_CLOEXEC);
    assert(source->frame_fd >= 0);
    return 1;
}

template <typename Condition> static void until(Condition condition) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        assert(timer.elapsed() < 3000);
        QCoreApplication::processEvents();
    }
}

// @internal — read only the app's prepared frame resource and own copied pixels.
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryFile file;
    assert(file.open());
    assert(file.resize(32 + 1920 * 1080 * 4));
    auto bytes = header();
    assert(pwrite(file.handle(), bytes.constData(), 32, 0) == 32);
    const auto pixels = QByteArray::fromHex("010203ff040506ff");
    assert(pwrite(file.handle(), pixels.constData(), 8, 32) == 8);
    int descriptor = open(file.fileName().toLocal8Bit().constData(), O_RDONLY | O_CLOEXEC);
    assert(descriptor >= 0);
    QImage retained;
    {
        NativePreviewReader reader(descriptor);
        const auto frame = reader.poll();
        assert(frame && frame->size() == QSize(2, 1));
        retained = *frame;
        assert(frame->pixelColor(0, 0) == QColor(1, 2, 3, 255));
        assert(frame->pixelColor(1, 0) == QColor(4, 5, 6, 255));
        assert(!reader.poll());
        bytes.fill(0, 24);
        assert(pwrite(file.handle(), bytes.constData(), 24, 8) == 24);
        assert(!reader.poll());
        assert(reader.closed());
    }
    assert(fcntl(descriptor, F_GETFD) == -1);
    assert(retained.pixelColor(1, 0) == QColor(4, 5, 6, 255));
    for (int offset : {0, 8, 12, 16, 20, 24}) {
        bytes = header();
        if (offset == 24) bytes.replace(offset, 8, QByteArray(8, 0));
        else bytes.replace(offset, 4, QByteArray(4, char(255)));
        assert(pwrite(file.handle(), bytes.constData(), 32, 0) == 32);
        NativePreviewReader invalid(open(file.fileName().toLocal8Bit().constData(), O_RDONLY | O_CLOEXEC));
        assert(!invalid.poll());
        assert(invalid.closed());
    }
    bytes = header();
    assert(pwrite(file.handle(), bytes.constData(), 32, 0) == 32);
    Resource resource{&file, 1};
    CameraPreview::setFrameResolver(resolve, &resource);
    CameraPreview preview;
    preview.setPreviewId("scan");
    preview.resize(320, 240);
    preview.show();
    until([&] { return preview.sizeHint() == QSize(2, 1); });
    assert(!preview.pixmap().isNull());
    qToLittleEndian(uint32_t(3), bytes.data() + 8);
    qToLittleEndian(uint32_t(2), bytes.data() + 12);
    qToLittleEndian(uint32_t(24), bytes.data() + 16);
    assert(pwrite(file.handle(), bytes.constData(), 32, 0) == 32);
    ++resource.generation;
    until([&] { return preview.sizeHint() == QSize(3, 2); });
    assert(!preview.pixmap().isNull());
    bytes.fill(0, 24);
    assert(pwrite(file.handle(), bytes.constData(), 24, 8) == 24);
    until([&] { return preview.pixmap().isNull(); });
    assert(preview.pixmap().isNull());
    bytes = header();
    assert(pwrite(file.handle(), bytes.constData(), 32, 0) == 32);
    ++resource.generation;
    until([&] { return preview.sizeHint() == QSize(2, 1); });
    preview.hide();
    assert(preview.pixmap().isNull());
    CameraPreview::setFrameResolver(nullptr, nullptr);
    return 0;
}
