// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display_test_harness.h"
#include "coreui/camerapreview.h"
#include <QLineEdit>

using namespace display_test;

namespace {
QJsonObject viewport(const QString &id) {
    return {{"CameraViewport", QJsonObject{{"preview_id", id}}}};
}

// @scenario: contact_exchange.feature :: Successful QR code exchange with proximity
void setup() { schedule(); }
}
Q_COREAPP_STARTUP_FUNCTION(setup)

int main(int argc, char **argv) {
    const QJsonObject qr{{"Qr", QJsonObject{{"id", "qr-input"}, {"purpose", "capture"},
        {"payloads", QJsonArray{}}, {"capture_placeholder", "Paste a code"},
        {"capture_submit_label", "Use code"}, {"accessibility", QJsonObject{{"label", "Scan a contact QR"}}}}}};
    steps = {
        {{surface("first", 1, "Scan", {qr}), profile("first"), viewport("camera-42")}, [] {
            const auto previews = window()->findChildren<CameraPreview *>();
            assert(previews.size() == 1);
            assert(previews.first()->previewId() == "camera-42");
            assert(previews.first()->accessibleName() == "Scan a contact QR");
            const auto inputs = window()->findChildren<QLineEdit *>();
            assert(inputs.size() == 1);
            assert(inputs.first()->placeholderText() == "Paste a code");
            inputs.first()->setText("vauchi://exchange/manual");
            events = {};
            QMetaObject::invokeMethod(inputs.first(), "returnPressed", Qt::DirectConnection);
        }},
        {{}, [] {
            assert((events == QJsonArray{
                event("SurfaceActivated", {{"surface_id", "first"}}),
                event("ValueChanged", {{"surface_id", "first"}, {"binding_id", "qr-input"},
                    {"value", QJsonObject{{"text", "vauchi://exchange/manual"}}}})}));
        }, [] { return events.size() == 2; }},
        {{viewport("camera-43")}, [] {
            assert(window()->findChildren<CameraPreview *>().first()->previewId() == "camera-43");
        }},
        {{"ResetApplication", surface("first", 1, "Scan", {qr}), profile("first")}, [] {
            const auto previews = window()->findChildren<CameraPreview *>();
            assert(previews.size() == 1);
            assert(previews.first()->previewId().isEmpty());
        }},
    };
    const auto result = qt_display_start(argc, argv, receive, &events);
    assert(result == 0);
    return result;
}
