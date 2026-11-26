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
    PreviewBinding(uint32_t id, uint64_t serial) : m_expectedId(id), m_expectedSerial(serial), m_exact(true) {}
    bool observe(uint32_t id, const QString &name, const QString &mediaClass, const QString &serial) {
        bool valid = false;
        const auto number = serial.toULongLong(&valid);
        const bool matches = m_exact ? id == m_expectedId && number == m_expectedSerial : name == m_name;
        if (m_retired || m_id != invalid || id == invalid || !matches || mediaClass != "Video/Source" || !valid || number == 0)
            return false;
        m_id = id;
        m_serial = serial;
        return true;
    }
    void remove(uint32_t id) {
        if (m_id != invalid && m_id == id) {
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
    uint32_t m_expectedId = invalid;
    uint64_t m_expectedSerial = 0;
    bool m_exact = false;
    bool m_retired = false;
};
