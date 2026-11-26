// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Qt display shell's native C API (ADR-078 step 2.2, D1 B).
// The app owns the engine and the main thread; the shell only renders
// command batches and reports what the user did as contract events.
#ifndef QT_DISPLAY_H
#define QT_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One contract Event, JSON-encoded. The string lives for the call only.
typedef void (*QtDisplayEventCallback)(const char *event_json, void *user_data);

// Runs on the GUI thread so the app can drain its hardware and engine inboxes.
typedef void (*QtDisplayPollCallback)(void *user_data);

typedef struct QtDisplayPreviewSource {
    uint32_t node_id;
    uint64_t serial;
    int remote_fd;
} QtDisplayPreviewSource;

// GUI-thread lookup of an app-owned stream: 1 ready, 0 pending, -1 failed.
// A ready remote_fd transfers ownership to the display; -1 uses the native
// connection. Each consumer requires a fresh, unconnected PipeWire remote.
typedef int (*QtDisplayPreviewCallback)(const char *preview_id,
                                       QtDisplayPreviewSource *source, void *user_data);

int qt_display_start_with_camera(int argc, char **argv, QtDisplayEventCallback on_event,
                                QtDisplayPollCallback on_poll, QtDisplayPreviewCallback on_preview,
                                void *user_data);


// Runs the Qt event loop on the calling thread until qt_display_stop or
// the window closes; returns the process exit code. argv follows main's
// convention and may carry Qt's own flags.
int qt_display_start(int argc, char **argv, QtDisplayEventCallback on_event,
                     void *user_data);

// Like start, with an initial and recurring (50 ms) host poll. Both callbacks
// borrow user_data only until this call returns. A null on_poll disables it.
int qt_display_start_with_poll(int argc, char **argv, QtDisplayEventCallback on_event,
                              QtDisplayPollCallback on_poll, void *user_data);


// Renders a JSON array of contract Commands. Safe from any thread once
// qt_display_start runs: the batch is posted to the GUI thread. Returns
// 0, or -1 when the batch is not a JSON array or no shell is running.
int qt_display_render(const char *commands_json, size_t len);

// Asks the running event loop to quit; qt_display_start then returns.
void qt_display_stop(void);
// Before an app starts its engine: hands this launch's vauchi:// links to a
// running shell and returns 1, so the caller exits; returns 0 when there is
// no link or no running shell. Call it before qt_display_start.
int qt_display_hand_off_links(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif
