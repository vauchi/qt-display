// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#[cfg(target_os = "linux")]
pub struct PreviewSource {
    pub node_id: u32,
    pub serial: u64,
    pub remote_fd: Option<std::os::fd::OwnedFd>,
}

#[cfg(target_os = "linux")]
pub type SourceResolver = Box<dyn FnMut(&str) -> Result<Option<PreviewSource>, String>>;

#[repr(C)]
pub(super) struct NativeSource {
    pub node_id: u32,
    pub serial: u64,
    pub remote_fd: std::ffi::c_int,
}

#[cfg(target_os = "linux")]
pub(super) unsafe extern "C" fn resolve(
    id: *const std::ffi::c_char,
    source: *mut NativeSource,
    context: *mut std::ffi::c_void,
) -> std::ffi::c_int {
    use std::{ffi::CStr, os::fd::IntoRawFd};
    // SAFETY: Qt passes live stack values on the GUI thread; run_callbacks
    // retains the callback state until every native preview has been destroyed.
    let callbacks = &mut *(context as *mut super::Callbacks);
    let Ok(id) = CStr::from_ptr(id).to_str() else {
        return -1;
    };
    let Some(resolver) = callbacks.preview.as_mut() else {
        return -1;
    };
    match resolver(id) {
        Ok(Some(value)) if value.node_id != u32::MAX && value.serial != 0 => {
            *source = NativeSource {
                node_id: value.node_id,
                serial: value.serial,
                remote_fd: value.remote_fd.map_or(-1, IntoRawFd::into_raw_fd),
            };
            1
        }
        Ok(None) => 0,
        _ => -1,
    }
}
