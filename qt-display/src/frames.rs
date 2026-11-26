// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    ffi::{c_char, c_int, c_void, CStr},
    os::fd::{IntoRawFd, OwnedFd},
};

pub enum FrameResource {
    Absent,
    Unchanged,
    New { generation: u64, fd: OwnedFd },
}
pub type FrameResolver = Box<dyn FnMut(&str, u64) -> Result<FrameResource, String>>;

#[repr(C)]
pub(super) struct NativeFrameSource {
    generation: u64,
    frame_fd: c_int,
}

pub(super) unsafe extern "C" fn resolve(
    id: *const c_char,
    generation: u64,
    source: *mut NativeFrameSource,
    context: *mut c_void,
) -> c_int {
    // SAFETY: the native loop calls serially with live stack values; run_callbacks owns this state until return.
    let callbacks = &mut *(context as *mut super::Callbacks);
    let Ok(id) = CStr::from_ptr(id).to_str() else {
        return -1;
    };
    let Some(resolver) = callbacks.frames.as_mut() else {
        return -1;
    };
    match resolver(id, generation) {
        Ok(FrameResource::Absent) => 0,
        Ok(FrameResource::Unchanged) => 2,
        Ok(FrameResource::New {
            generation: next,
            fd,
        }) if next > 0 && next != generation => {
            *source = NativeFrameSource {
                generation: next,
                frame_fd: fd.into_raw_fd(),
            };
            1
        }
        _ => -1,
    }
}
