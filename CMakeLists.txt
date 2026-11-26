# SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(vauchi-qt VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Release profile: optimized binary (matches linux-gtk Cargo.toml)
# LTO for cross-TU optimization, strip symbols, trap on overflow
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=auto")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=auto -s")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftrapv")

# --- Coverage support (gcov/lcov) ---
option(COVERAGE "Enable code coverage instrumentation (gcov/lcov)" OFF)
if(COVERAGE)
    message(STATUS "Coverage: ENABLED (gcov/lcov)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network Concurrent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(QRENCODE REQUIRED libqrencode)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(VAUCHI_CAMERA_PREVIEW_DEFAULT ON)
else()
    set(VAUCHI_CAMERA_PREVIEW_DEFAULT OFF)
endif()
option(VAUCHI_CAMERA_PREVIEW "Render hardware-published PipeWire previews" ${VAUCHI_CAMERA_PREVIEW_DEFAULT})
if(VAUCHI_CAMERA_PREVIEW)
    pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3>=0.3.77)
endif()

find_package(Qt6 QUIET COMPONENTS DBus)

# The display shell behind the C ABI of include/qt_display.h (ADR-078
# step 2.2, D1 B): the renderer sources and nothing of the engine. The
# Rust crate in qt-display/ builds this target alone. Nothing in this
# repository links Core: the apps assemble the engine (private#1029).
set(QT_DISPLAY_SOURCES
    src/display_abi.cpp
    src/coreui/displayhost.cpp
    src/coreui/camerapreview.cpp
    src/coreui/camerapreview.h
    src/coreui/previewreader.cpp
    src/coreui/nativepreviewreader.cpp
    src/coreui/displayhost_feedback.cpp
    src/coreui/displayhost_reset.cpp
    src/coreui/displayhost_native_effects.cpp
    src/coreui/displayhost_permissions.cpp
    src/platform/permissionprompt.cpp
    src/platform/deeplink.cpp
    src/coreui/displayhost_file_effects.cpp
    src/coreui/presentationeffectpayload.cpp
    src/coreui/applicationmenubar.cpp
    src/coreui/applicationmenuaccessibility.cpp
    src/coreui/statusitemtray.cpp
    src/platform/filepicker.cpp
    src/coreui/displayhost_overlays.cpp
    src/coreui/navigationsidebar.cpp
    src/coreui/presentationstate.cpp
    src/coreui/presentationaccessibility.cpp
    src/coreui/presentationsurface.cpp
    src/coreui/presentationsurface_list.cpp
    src/coreui/presentationsurface_avatar.cpp
    src/coreui/presentationsurface_nodes.cpp
    src/coreui/presentationsurface_update.cpp
    src/coreui/presentationsurface_qr.cpp
    src/coreui/thememanager.cpp
    src/coreui/thememanager_prepared.cpp
    src/coreui/navigationicons.cpp
    resources/resources.qrc
)
set(VAUCHI_TOKENS_INCLUDE "${CMAKE_SOURCE_DIR}/../themes/generated" CACHE PATH "Path to generated design tokens")
add_library(qt_display STATIC ${QT_DISPLAY_SOURCES})
# The Release flags add -flto=auto, which leaves GCC LTO bytecode in the
# archive; the Rust consumer links with rust-lld, which cannot read it and
# reports the three C symbols undefined. The library is built without LTO.
target_compile_options(qt_display PRIVATE -fno-lto)
target_include_directories(qt_display PUBLIC include PRIVATE src ${QRENCODE_INCLUDE_DIRS} ${VAUCHI_TOKENS_INCLUDE})
target_link_directories(qt_display PUBLIC ${QRENCODE_LIBRARY_DIRS})
target_link_libraries(qt_display PUBLIC Qt6::Widgets Qt6::Core Qt6::Concurrent Qt6::Network ${QRENCODE_LIBRARIES})
if(VAUCHI_CAMERA_PREVIEW)
    target_link_libraries(qt_display PUBLIC PkgConfig::PIPEWIRE)
    target_compile_definitions(qt_display PRIVATE VAUCHI_PIPEWIRE)
endif()

option(VAUCHI_PERMISSION_PORTAL "Enable the camera permission portal" ON)
if(Qt6DBus_FOUND AND VAUCHI_PERMISSION_PORTAL)
    target_link_libraries(qt_display PUBLIC Qt6::DBus)
    target_compile_definitions(qt_display PUBLIC VAUCHI_HAS_PERMISSION_PORTAL)
endif()

enable_testing()

add_executable(camera_preview_test tests/camera_preview_test.cpp)
target_include_directories(camera_preview_test PRIVATE src)
target_link_libraries(camera_preview_test PRIVATE qt_display)
target_compile_options(camera_preview_test PRIVATE -UNDEBUG)
add_test(NAME camera_preview COMMAND camera_preview_test)
if(UNIX)
    add_executable(native_preview_test tests/native_preview_test.cpp)
    target_include_directories(native_preview_test PRIVATE src)
    target_link_libraries(native_preview_test PRIVATE qt_display)
    target_compile_options(native_preview_test PRIVATE -UNDEBUG)
    add_test(NAME native_preview COMMAND native_preview_test)
    set_tests_properties(native_preview PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
set_tests_properties(camera_preview PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(display_camera_preview_test tests/display_camera_preview_test.cpp)
target_include_directories(display_camera_preview_test PRIVATE src)
target_link_libraries(display_camera_preview_test PRIVATE qt_display)
target_compile_options(display_camera_preview_test PRIVATE -UNDEBUG)
add_test(NAME display_camera_preview COMMAND display_camera_preview_test)
set_tests_properties(display_camera_preview PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(display_list_layout_test tests/display_list_layout_test.cpp)
target_include_directories(display_list_layout_test PRIVATE src)
target_link_libraries(display_list_layout_test PRIVATE qt_display)
target_compile_options(display_list_layout_test PRIVATE -UNDEBUG)
foreach(case inline reflow large_text events disabled detail checkbox density)
    add_test(NAME display_list_${case} COMMAND display_list_layout_test ${case})
    set_tests_properties(display_list_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_environment_test tests/display_environment_test.cpp)
target_link_libraries(display_environment_test PRIVATE qt_display)
target_compile_options(display_environment_test PRIVATE -UNDEBUG)
add_test(NAME display_environment COMMAND display_environment_test)
add_test(NAME display_environment_reduced_motion COMMAND display_environment_test)
set_tests_properties(display_environment PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_tests_properties(display_environment_reduced_motion PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_REDUCE_MOTION=1")
add_executable(display_copy_test tests/display_copy_test.cpp)
target_link_libraries(display_copy_test PRIVATE qt_display)
target_compile_options(display_copy_test PRIVATE -UNDEBUG)
add_test(NAME display_copy_test COMMAND display_copy_test)
set_tests_properties(display_copy_test PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(display_poll_test tests/display_poll_test.cpp)
target_link_libraries(display_poll_test PRIVATE qt_display)
target_compile_options(display_poll_test PRIVATE -UNDEBUG)
add_test(NAME display_poll_test COMMAND display_poll_test)
set_tests_properties(display_poll_test PROPERTIES TIMEOUT 10
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(display_launch_link_test tests/display_launch_link_test.cpp)
target_link_libraries(display_launch_link_test PRIVATE qt_display)
target_compile_options(display_launch_link_test PRIVATE -UNDEBUG)
foreach(case launch relay alone)
    add_test(NAME display_launch_link_${case} COMMAND display_launch_link_test ${case})
    set_tests_properties(display_launch_link_${case} PROPERTIES TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;VAUCHI_DEEP_LINK_SERVER=vauchi-qt-test-${case}")
endforeach()

add_executable(display_contract_test tests/display_contract_test.cpp)
target_link_libraries(display_contract_test PRIVATE qt_display)
target_compile_options(display_contract_test PRIVATE -UNDEBUG)
foreach(case composition context navigation revisions values focus)
    add_test(NAME display_${case} COMMAND display_contract_test ${case})
    set_tests_properties(display_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_overlay_test tests/display_overlay_test.cpp)
target_link_libraries(display_overlay_test PRIVATE qt_display)
target_compile_options(display_overlay_test PRIVATE -UNDEBUG)
foreach(case information navigation menu dismissal)
    add_test(NAME display_overlay_${case} COMMAND display_overlay_test ${case})
    set_tests_properties(display_overlay_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_application_menu_test tests/display_application_menu_test.cpp)
target_link_libraries(display_application_menu_test PRIVATE qt_display)
target_compile_options(display_application_menu_test PRIVATE -UNDEBUG)
foreach(case availability prepared accessibility revisions replacement quit)
    add_test(NAME display_application_menu_${case} COMMAND display_application_menu_test ${case})
    set_tests_properties(display_application_menu_${case} PROPERTIES TIMEOUT 10
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_status_item_test tests/display_status_item_test.cpp)
target_link_libraries(display_status_item_test PRIVATE qt_display)
target_compile_options(display_status_item_test PRIVATE -UNDEBUG)
foreach(case availability prepared accessibility show_window revisions replacement reset quit)
    add_test(NAME display_status_item_${case} COMMAND display_status_item_test ${case})
    set_tests_properties(display_status_item_${case} PROPERTIES TIMEOUT 10
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_file_picker_test tests/display_file_picker_test.cpp)
target_link_libraries(display_file_picker_test PRIVATE qt_display)
target_compile_options(display_file_picker_test PRIVATE -UNDEBUG)
foreach(case availability dialog cancel selection limits replacement)
    add_test(NAME display_file_picker_${case} COMMAND display_file_picker_test ${case})
    set_tests_properties(display_file_picker_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_screen_settings_test tests/display_screen_settings_test.cpp)
target_link_libraries(display_screen_settings_test PRIVATE qt_display)
target_compile_options(display_screen_settings_test PRIVATE -UNDEBUG)
add_test(NAME display_screen_settings_test COMMAND display_screen_settings_test)
set_tests_properties(display_screen_settings_test PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(display_export_test tests/display_export_test.cpp)
target_link_libraries(display_export_test PRIVATE qt_display)
target_compile_options(display_export_test PRIVATE -UNDEBUG)
foreach(case selection cancellation invalid replacement empty)
    add_test(NAME display_export_${case} COMMAND display_export_test ${case})
    set_tests_properties(display_export_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_legacy_picker_test tests/display_legacy_picker_test.cpp)
target_link_libraries(display_legacy_picker_test PRIVATE qt_display)
target_compile_options(display_legacy_picker_test PRIVATE -UNDEBUG)
foreach(case file image cancel limits)
    add_test(NAME display_legacy_picker_${case} COMMAND display_legacy_picker_test ${case})
    set_tests_properties(display_legacy_picker_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_native_effects_test tests/display_native_effects_test.cpp)
target_link_libraries(display_native_effects_test PRIVATE qt_display)
target_compile_options(display_native_effects_test PRIVATE -UNDEBUG)
foreach(case alert toast url malformed)
    add_test(NAME display_native_effects_${case} COMMAND display_native_effects_test ${case})
    set_tests_properties(display_native_effects_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_desktop_effects_test tests/display_desktop_effects_test.cpp)
target_link_libraries(display_desktop_effects_test PRIVATE qt_display)
target_compile_options(display_desktop_effects_test PRIVATE -UNDEBUG)
foreach(case gesture share back identity)
    add_test(NAME display_desktop_effects_${case} COMMAND display_desktop_effects_test ${case})
    set_tests_properties(display_desktop_effects_${case} PROPERTIES TIMEOUT 10
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
# Offscreen activates every shown window by itself; only an X server with no
# window manager, as in the AT-SPI jobs, shows whether the display does.
find_program(XVFB_RUN xvfb-run)
if(XVFB_RUN)
    add_test(NAME display_desktop_effects_active_x11
             COMMAND ${XVFB_RUN} -a $<TARGET_FILE:display_desktop_effects_test> active)
    set_tests_properties(display_desktop_effects_active_x11 PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=xcb")
elseif(DEFINED ENV{CI})
    message(FATAL_ERROR "xvfb-run is required in CI for display_desktop_effects_active_x11")
endif()

add_executable(display_reset_test tests/display_reset_test.cpp)
target_link_libraries(display_reset_test PRIVATE qt_display)
target_compile_options(display_reset_test PRIVATE -UNDEBUG)
foreach(case cached trailing dialogs queued)
    add_test(NAME display_reset_${case} COMMAND display_reset_test ${case})
    set_tests_properties(display_reset_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

add_executable(display_approved_effects_test tests/display_approved_effects_test.cpp)
target_link_libraries(display_approved_effects_test PRIVATE qt_display)
target_compile_options(display_approved_effects_test PRIVATE -UNDEBUG)
foreach(case qr animation reduced disabled none reset)
    add_test(NAME display_approved_effects_${case} COMMAND display_approved_effects_test ${case})
    set_tests_properties(display_approved_effects_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
add_test(NAME display_approved_effects_invocation COMMAND ${CMAKE_COMMAND}
    -DBIN=$<TARGET_FILE:display_approved_effects_test>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_ignored_invocation.cmake)
set_tests_properties(display_approved_effects_invocation PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(display_theme_test tests/display_theme_test.cpp)
target_link_libraries(display_theme_test PRIVATE qt_display)
target_compile_options(display_theme_test PRIVATE -UNDEBUG)
foreach(case appearance focus fallback reload invalid unsafe_font tones reset unicode_font active resources)
    add_test(NAME display_theme_${case} COMMAND display_theme_test ${case})
    set_tests_properties(display_theme_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

option(VAUCHI_PERMISSION_TESTS "Build isolated D-Bus permission tests" ON)
if(VAUCHI_PERMISSION_TESTS)
add_executable(display_permission_test tests/display_permission_test.cpp)
target_link_libraries(display_permission_test PRIVATE qt_display)
target_compile_options(display_permission_test PRIVATE -UNDEBUG)
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
set(permission_cases unavailable malformed)
if(Qt6DBus_FOUND AND VAUCHI_PERMISSION_PORTAL)
    list(APPEND permission_cases granted denied failed duplicate wrong_path spoof reset vanished alternate early foreign flood alternate_reset oversized deferred_reset)
endif()
foreach(case IN LISTS permission_cases)
    add_test(NAME display_permission_${case} COMMAND ${DBUS_RUN_SESSION}
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/permission_bus.conf"
        -- $<TARGET_FILE:display_permission_test> ${case})
    set_tests_properties(display_permission_${case} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
endif()

# Catalog and fixture renders on the library the apps assemble, for VRT and
# test:screen-catalog, without Core (private#1029).
add_executable(qt_display_render tools/qt_display_render.cpp src/coreui/screencatalog.cpp)
target_include_directories(qt_display_render PRIVATE src)
target_link_libraries(qt_display_render PRIVATE qt_display)
add_test(NAME display_render_fixture_smoke
    COMMAND ${CMAKE_COMMAND}
        -DBIN=$<TARGET_FILE:qt_display_render>
        -DFIXTURE=${CMAKE_SOURCE_DIR}/tests/fixtures/generic_surface.json
        -DPNG_OUT=${CMAKE_CURRENT_BINARY_DIR}/display_render_fixture_smoke.png
        -P ${CMAKE_SOURCE_DIR}/tests/render_fixture_smoke.cmake)
add_test(NAME display_render_catalog_smoke
    COMMAND ${CMAKE_COMMAND}
        -DBIN=$<TARGET_FILE:qt_display_render>
        -DCATALOG=${CMAKE_SOURCE_DIR}/tests/fixtures/catalog_smoke.json
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/display_render_catalog_smoke
        -P ${CMAKE_SOURCE_DIR}/tests/render_catalog_smoke.cmake)

# Presentation reducer state: multi-surface retention and active context.
# The fixture corpus is replayed by contract_replay_test below.
add_executable(presentation_state_test
    tests/presentation_state_test.cpp
    src/coreui/presentationstate.cpp)
target_include_directories(presentation_state_test PRIVATE src)
target_link_libraries(presentation_state_test PRIVATE Qt6::Core)
add_test(NAME presentation_state_test COMMAND presentation_state_test)

# Contract replay (ADR-078 D-11). Not a ctest case: it takes the fixtures of
# the Core in core-ref as arguments, which the contract:replay job fetches.
add_executable(contract_replay_test
    tests/contract_replay_test.cpp
    src/coreui/presentationstate.cpp)
target_include_directories(contract_replay_test PRIVATE src)
target_link_libraries(contract_replay_test PRIVATE Qt6::Core)

# Generic presentation surface: native controls emit only opaque protocol
# identifiers and generic values.
add_executable(presentation_surface_test
    tests/presentation_surface_test.cpp)
target_include_directories(presentation_surface_test PRIVATE
    src ${QRENCODE_INCLUDE_DIRS})
target_link_directories(presentation_surface_test PRIVATE
    ${QRENCODE_LIBRARY_DIRS})
target_link_libraries(presentation_surface_test PRIVATE
    Qt6::Widgets Qt6::Core ${QRENCODE_LIBRARIES})
add_test(NAME presentation_surface_test COMMAND presentation_surface_test)

add_executable(presentation_image_test
    tests/presentation_image_test.cpp)
target_include_directories(presentation_image_test PRIVATE
    src ${QRENCODE_INCLUDE_DIRS})
target_link_directories(presentation_image_test PRIVATE
    ${QRENCODE_LIBRARY_DIRS})
target_link_libraries(presentation_image_test PRIVATE
    Qt6::Widgets Qt6::Core ${QRENCODE_LIBRARIES})
add_test(NAME presentation_image_test COMMAND presentation_image_test)

# Pure QR placement/error-correction mapping: Core's display code
# `placement` (side + top-left corner, in permille of the node's square)
# and `error_correction` level fields (vauchi/private#450, as on
# iOS/macOS).
add_executable(presentation_qr_placement_test
    tests/presentation_qr_placement_test.cpp)
target_include_directories(presentation_qr_placement_test PRIVATE
    src ${QRENCODE_INCLUDE_DIRS})
target_link_directories(presentation_qr_placement_test PRIVATE
    ${QRENCODE_LIBRARY_DIRS})
target_link_libraries(presentation_qr_placement_test PRIVATE
    Qt6::Widgets Qt6::Core ${QRENCODE_LIBRARIES})
add_test(NAME presentation_qr_placement_test
    COMMAND presentation_qr_placement_test)

# Core's pinned group under a surface's content (vauchi/private#1304).
add_executable(presentation_pinned_actions_test
    tests/presentation_pinned_actions_test.cpp)
target_include_directories(presentation_pinned_actions_test PRIVATE
    src ${QRENCODE_INCLUDE_DIRS})
target_link_directories(presentation_pinned_actions_test PRIVATE
    ${QRENCODE_LIBRARY_DIRS})
target_link_libraries(presentation_pinned_actions_test PRIVATE
    Qt6::Widgets Qt6::Core ${QRENCODE_LIBRARIES})
add_test(NAME presentation_pinned_actions_test
    COMMAND presentation_pinned_actions_test)

foreach(renderer_target presentation_surface_test presentation_image_test
        presentation_qr_placement_test presentation_pinned_actions_test)
    target_link_libraries(${renderer_target} PRIVATE qt_display)
endforeach()

# Persistent per-surface navigation: PresentationState's SetNavigation
# reducer plus the sidebar model/widget it feeds.
add_executable(presentation_navigation_test
    tests/presentation_navigation_test.cpp
    src/coreui/presentationstate.cpp
    src/coreui/navigationicons.cpp
    src/coreui/navigationsidebar.cpp)
target_include_directories(presentation_navigation_test PRIVATE src)
target_link_libraries(presentation_navigation_test PRIVATE
    Qt6::Widgets Qt6::Core)
add_test(NAME presentation_navigation_test COMMAND presentation_navigation_test)

# Navigation icon test: Core icon token -> theme icon name resolution
add_executable(navigation_icons_test
    tests/navigation_icons_test.cpp
    src/coreui/navigationicons.cpp
    resources/resources.qrc)
target_include_directories(navigation_icons_test PRIVATE src)
target_link_libraries(navigation_icons_test PRIVATE Qt6::Widgets Qt6::Core)
add_test(NAME navigation_icons_test COMMAND navigation_icons_test)

# Deep links: only vauchi:// arguments are reported, and a second launch
# hands its link to the running instance (vauchi/private#650). The desktop
# entry that registers the scheme is the app's (linux-app).
add_executable(deep_link_test tests/deep_link_test.cpp src/platform/deeplink.cpp)
target_include_directories(deep_link_test PRIVATE src)
target_link_libraries(deep_link_test PRIVATE Qt6::Core Qt6::Network)
add_test(NAME deep_link_test COMMAND deep_link_test)

add_executable(file_picker_test tests/file_picker_test.cpp src/platform/filepicker.cpp)
target_include_directories(file_picker_test PRIVATE src)
target_link_libraries(file_picker_test PRIVATE Qt6::Widgets Qt6::Core Qt6::Concurrent)
add_test(NAME file_picker_test COMMAND file_picker_test)
set_tests_properties(file_picker_test PROPERTIES TIMEOUT 30)

# ThemeManager test: palette generation, stylesheet output, file loading,
# brand font registration. Needs resources.qrc so registerBrandFonts() has
# real :/fonts/... payloads to register, same as the qt_display library.
add_executable(theme_test
    tests/theme_test.cpp
    src/coreui/thememanager.cpp
    src/coreui/thememanager_prepared.cpp
    resources/resources.qrc)
target_include_directories(theme_test PRIVATE include ${VAUCHI_TOKENS_INCLUDE})
target_link_libraries(theme_test PRIVATE Qt6::Widgets Qt6::Core)
add_test(NAME theme_test COMMAND theme_test)

# Screen-catalog document parsing for `qt_display_render --render-catalog`: Core's
# screen_catalog_v1 shape, the presentation-contract fallback, file-name
# safety of code_ids, and per-variant output names.
add_executable(screen_catalog_test
    tests/screen_catalog_test.cpp
    src/coreui/screencatalog.cpp)
target_include_directories(screen_catalog_test PRIVATE src)
target_link_libraries(screen_catalog_test PRIVATE Qt6::Core)
add_test(NAME screen_catalog_test COMMAND screen_catalog_test)


# Every C++ test here states its expectations with assert(). NDEBUG compiles
# assert() to nothing, so a Release-configured build turns the whole suite into
# no-ops that exit 0 while checking nothing — indistinguishable from passing.
# CI configures without CMAKE_BUILD_TYPE, so it is unaffected, but any local
# `-DCMAKE_BUILD_TYPE=Release` build reports a silent false green (observed
# 2026-08-07: a controller test passed in Release while a real failure was
# present, and only surfaced in a Debug build).
#
# Undefine NDEBUG on the test targets so their assertions survive every build
# type. Applied per-target rather than globally: the library itself keeps
# whatever the chosen build type implies.
foreach(vauchi_test_target
        presentation_state_test
        presentation_surface_test
        presentation_image_test
        presentation_qr_placement_test
        presentation_navigation_test
        navigation_icons_test
        theme_test
        screen_catalog_test
        deep_link_test
        file_picker_test)
    target_compile_options(${vauchi_test_target} PRIVATE -UNDEBUG)
endforeach()

# AT-SPI probe harness: presents a SurfaceSpec fixture through the production
# PresentationSurface and stays alive so tests/atspi/ can read the live bus.
# Not an add_test target — tests/atspi/run-tests.sh drives it under Xvfb.
add_executable(atspi_fixture_probe
    tests/atspi_fixture_probe.cpp)
target_include_directories(atspi_fixture_probe PRIVATE
    src ${QRENCODE_INCLUDE_DIRS})
target_link_directories(atspi_fixture_probe PRIVATE
    ${QRENCODE_LIBRARY_DIRS})
target_link_libraries(atspi_fixture_probe PRIVATE qt_display
    Qt6::Widgets Qt6::Core ${QRENCODE_LIBRARIES})
