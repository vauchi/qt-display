<!-- SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

> **Mirror:** This repo is a read-only mirror of [gitlab.com/vauchi/qt-display](https://gitlab.com/vauchi/qt-display). Please open issues and merge requests there.

[![Pipeline](https://img.shields.io/endpoint?url=https://vauchi.gitlab.io/qt-display/badges/pipeline.json&label=pipeline)](https://gitlab.com/vauchi/qt-display)
[![REUSE](https://api.reuse.software/badge/gitlab.com/vauchi/qt-display)](https://api.reuse.software/info/gitlab.com/vauchi/qt-display)

> [!NOTE]
> **You're early — and that's the point.** Vauchi is pre-alpha and
> under heavy development: not yet ready for production, and APIs may
> change without notice. If you're here now, you can help shape it —
> try it, break it, and tell us what's missing.

# Vauchi Qt display

The Qt display shell for Vauchi — living contact cards, exchanged in
person. It is a library: it draws the `Command`s of the Vauchi contract
with Qt6 Widgets and reports what the user does as `Event`s. It links no
engine and no hardware code. The `linux-app` repository assembles it with
the engine and the hardware shell into the `qvauchi` desktop app.

## Prerequisites

- Qt6 development libraries (`qt6-base-dev`)
- `libqrencode`
- CMake 3.16+
- C++17 compiler

## Build

```bash
mkdir build && cd build
cmake ..
make
ctest
```

The Rust crate in `qt-display/` wraps the library's C ABI
(`include/qt_display.h`) for Rust apps and builds the `qt_display`
target through CMake.

## Architecture

- **DisplayHost** applies command batches and owns the window, menus,
  overlays, file pickers and the status item
- **PresentationState** retains surfaces, active-pane composition, contextual
  controls, and overlays by opaque identifier
- **PresentationSurface** maps generic nodes to native Qt widgets
- **`qt_display_render`** draws a screen catalog or one fixture to PNG
  files, for visual checks

All business logic lives in Vauchi's Core. This repository is a pure
rendering layer.

## License

GPL-3.0-or-later
