// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Builds the qt_display static library from the repository's CMake
// project and links Qt the way the C++ build does, through pkg-config.
use std::process::Command;

fn pkg_config_libs(modules: &[&str]) -> String {
    let output = Command::new("pkg-config")
        .arg("--libs")
        .args(modules)
        .output()
        .expect("pkg-config is required to link Qt");
    assert!(
        output.status.success(),
        "pkg-config --libs {modules:?} failed"
    );
    String::from_utf8(output.stdout).expect("pkg-config output is UTF-8")
}

fn main() {
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let repo = std::path::Path::new(&manifest_dir)
        .parent()
        .unwrap()
        .to_path_buf();
    let tokens = std::env::var("VAUCHI_TOKENS_INCLUDE").unwrap_or_else(|_| {
        std::path::Path::new(&manifest_dir)
            .join("tokens")
            .display()
            .to_string()
    });
    let camera_preview = std::env::var_os("CARGO_FEATURE_CAMERA_PREVIEW").is_some()
        && std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("linux");
    let built = cmake::Config::new(&repo)
        .define(
            "VAUCHI_CAMERA_PREVIEW",
            if camera_preview { "ON" } else { "OFF" },
        )
        .define("VAUCHI_PERMISSION_TESTS", "OFF")
        .define("VAUCHI_TOKENS_INCLUDE", &tokens)
        .build_target("qt_display")
        .build();
    println!(
        "cargo:rustc-link-search=native={}",
        built.join("build").display()
    );
    println!("cargo:rustc-link-lib=static=qt_display");
    let mut modules = vec![
        "Qt6Widgets",
        "Qt6Gui",
        "Qt6Core",
        "Qt6Concurrent",
        "Qt6Network",
        "libqrencode",
    ];
    if camera_preview {
        modules.push("libpipewire-0.3");
    }
    if Command::new("pkg-config")
        .args(["--exists", "Qt6DBus"])
        .status()
        .is_ok_and(|status| status.success())
    {
        modules.push("Qt6DBus");
    }
    for flag in pkg_config_libs(&modules).split_whitespace() {
        if let Some(lib) = flag.strip_prefix("-l") {
            println!("cargo:rustc-link-lib={lib}");
        } else if let Some(dir) = flag.strip_prefix("-L") {
            println!("cargo:rustc-link-search=native={dir}");
        }
    }
    println!("cargo:rustc-link-lib=stdc++");
    println!("cargo:rerun-if-changed=../src/display_abi.cpp");
    println!("cargo:rerun-if-changed=../src/coreui");
    println!("cargo:rerun-if-changed=../resources");
    println!("cargo:rerun-if-changed=../src/platform/filepicker.cpp");
    println!("cargo:rerun-if-changed=../src/platform/filepicker.h");
    println!("cargo:rerun-if-changed=../src/platform/permissionprompt.cpp");
    println!("cargo:rerun-if-changed=../src/platform/permissionprompt.h");
    println!("cargo:rerun-if-changed=../include/qt_display.h");
    println!("cargo:rerun-if-changed=../CMakeLists.txt");
    println!("cargo:rerun-if-changed=tokens/Tokens.h");
    println!("cargo:rerun-if-env-changed=VAUCHI_TOKENS_INCLUDE");
}
