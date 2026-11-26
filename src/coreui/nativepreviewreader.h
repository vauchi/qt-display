// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QImage>
#include <cstdint>
#include <optional>

class NativePreviewReader {
public:
    explicit NativePreviewReader(int fd) : m_fd(fd) {}
    ~NativePreviewReader();
    NativePreviewReader(const NativePreviewReader &) = delete;
    NativePreviewReader &operator=(const NativePreviewReader &) = delete;
    std::optional<QImage> poll();
    bool closed() const { return m_closed; }
private:
    std::optional<QImage> read();
    int m_fd;
    uint64_t m_generation = 0;
    bool m_closed = false;
};
