// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QImage>
#include <QString>
#include <memory>
#include <optional>
#include "qt_display.h"

class PreviewReader {
public:
    explicit PreviewReader(QtDisplayPreviewSource source);
    ~PreviewReader();
    std::optional<QImage> poll();
    bool closed() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
