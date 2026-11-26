// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "nativepreviewreader.h"
#include "camerapreview.h"
#include <QtEndian>
#ifdef Q_OS_UNIX
#include <cerrno>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static bool readAt(int fd, char *bytes, size_t count, off_t offset) {
    size_t done = 0;
    while (done < count) {
        const auto result = pread(fd, bytes + done, count - done, offset + off_t(done));
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return false;
        done += size_t(result);
    }
    return true;
}
#endif

NativePreviewReader::~NativePreviewReader() {
#ifdef Q_OS_UNIX
    if (m_fd >= 0) close(m_fd);
#endif
}

std::optional<QImage> NativePreviewReader::poll() {
    if (m_closed) return {};
#ifdef Q_OS_UNIX
    if (flock(m_fd, LOCK_SH | LOCK_NB) != 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) m_closed = true;
        return {};
    }
    auto result = read();
    if (flock(m_fd, LOCK_UN) != 0) { m_closed = true; return {}; }
    return result;
#else
    m_closed = true;
    return {};
#endif
}

std::optional<QImage> NativePreviewReader::read() {
    m_closed = true;
#ifdef Q_OS_UNIX
    struct stat metadata {};
    if (fstat(m_fd, &metadata) != 0 || !S_ISREG(metadata.st_mode) || metadata.st_size != 32 + 1920 * 1080 * 4) return {};
    QByteArray header(32, 0);
    if (!readAt(m_fd, header.data(), 32, 0) || header.first(8) != "VFRAME01") return {};
    const auto word = [&header](int offset) { return qFromLittleEndian<uint32_t>(header.constData() + offset); };
    const auto width = word(8), height = word(12), bytes = word(16), state = word(20);
    const auto generation = qFromLittleEndian<uint64_t>(header.constData() + 24);
    if ((state == 0 || state == 1) && width == 0 && height == 0 && bytes == 0) {
        m_closed = state == 0;
        return {};
    }
    if (state != 2 || width == 0 || width > 1920 || height == 0 || height > 1080 || bytes != width * height * 4 || generation == 0) return {};
    m_closed = false;
    if (generation == m_generation) return {};
    QByteArray pixels(qsizetype(bytes), 0);
    if (!readAt(m_fd, pixels.data(), bytes, 32)) { m_closed = true; return {}; }
    auto frame = CameraPreview::frameFromRgba(pixels, width, height, int(width * 4), 0);
    if (frame.isNull()) { m_closed = true; return {}; }
    m_generation = generation;
    return frame;
#else
    return {};
#endif
}
