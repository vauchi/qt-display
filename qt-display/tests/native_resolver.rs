// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    cell::Cell,
    rc::Rc,
    time::{Duration, Instant},
};

// @internal — the native resource callback crosses the actual C ABI and widget.
fn main() {
    let requested = Rc::new(Cell::new(false));
    let seen = requested.clone();
    let received = requested.clone();
    let start = Instant::now();
    let exit = qt_display::run_with_native_preview(
        &[
            "native-resolver-test".into(),
            "-platform".into(),
            "offscreen".into(),
        ],
        Box::new(|_| {}),
        Box::new(move || {
            assert!(
                start.elapsed() < Duration::from_secs(3),
                "native resource callback was never called"
            );
            if requested.get() {
                qt_display::stop();
                return;
            }
            qt_display::render(r#"[{"CameraViewport":{"preview_id":"camera-42"}},{"ReplaceSurface":{"surface":{"surface_id":"capture","revision":1,"title":"Scan","layout":"fixed","tokens":{},"nodes":[{"Qr":{"id":"qr","purpose":"capture","payloads":[],"capture_placeholder":"Paste code","capture_submit_label":"Use code","accessibility":{"label":"Camera"}}}]}}},{"SetPresentationProfile":{"profile":{"window_class":"expanded","pane_layout":"single","primary_surface":"capture","active_surface":"capture"}}}]"#).unwrap();
        }),
        Box::new(move |id, generation| {
            assert_eq!(id, "camera-42");
            assert_eq!(generation, 0);
            seen.set(true);
            Ok(qt_display::FrameResource::Absent)
        }),
    );
    assert_eq!(exit, 0);
    assert!(received.get());
}
