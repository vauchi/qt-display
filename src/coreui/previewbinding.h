// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QString>
#include <cstdint>
#include <limits>
#include <utility>

class PreviewBinding {
public:
    explicit PreviewBinding(QString name) : m_name(std::move(name)) {}
    bool observe(uint32_t id, const QString &name, const QString &mediaClass, const QString &serial) {
        bool valid = false;
        serial.toULongLong(&valid);
        if (m_retired || m_id != invalid || name != m_name || mediaClass != "Video/Source" || !valid)
            return false;
        m_id = id;
        m_serial = serial;
        return true;
    }
    void remove(uint32_t id) {
        if (m_id == id) {
            m_id = invalid;
            m_serial.clear();
            m_retired = true;
        }
    }
    uint32_t nodeId() const { return m_id; }
    QString serial() const { return m_serial; }
    bool retired() const { return m_retired; }
private:
    static constexpr uint32_t invalid = std::numeric_limits<uint32_t>::max();
    QString m_name;
    QString m_serial;
    uint32_t m_id = invalid;
    bool m_retired = false;
};
