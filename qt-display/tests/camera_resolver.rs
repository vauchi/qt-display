// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    cell::Cell,
    rc::Rc,
    time::{Duration, Instant},
};

// @internal — the Rust callback reaches the actual native preview widget.
fn main() {
    let requested = Rc::new(Cell::new(false));
    let seen = requested.clone();
    let received = requested.clone();
    let start = Instant::now();
    let exit = qt_display::run_with_camera(
        &[
            "camera-resolver-test".into(),
            "-platform".into(),
            "offscreen".into(),
        ],
        Box::new(|_| {}),
        Box::new(move || {
            assert!(
                start.elapsed() < Duration::from_secs(3),
                "preview resolver was never called"
            );
            if requested.get() {
                qt_display::stop();
                return;
            }
            qt_display::render(r#"[{"CameraViewport":{"preview_id":"camera-42"}},{"ReplaceSurface":{"surface":{"surface_id":"capture","revision":1,"title":"Scan","layout":"fixed","tokens":{},"nodes":[{"Qr":{"id":"qr","purpose":"capture","payloads":[],"capture_placeholder":"Paste code","capture_submit_label":"Use code","accessibility":{"label":"Camera"}}}]}}},{"SetPresentationProfile":{"profile":{"window_class":"expanded","pane_layout":"single","primary_surface":"capture","active_surface":"capture"}}}]"#).unwrap();
        }),
        Box::new(move |id| {
            assert_eq!(id, "camera-42");
            seen.set(true);
            Ok(None)
        }),
    );
    assert_eq!(exit, 0);
    assert!(received.get());
}
