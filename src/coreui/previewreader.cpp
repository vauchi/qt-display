// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "previewreader.h"
#include "previewbinding.h"
#include "camerapreview.h"
#include <utility>
#include <cstring>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#ifdef VAUCHI_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/buffers.h>

struct PreviewReader::Impl {
    explicit Impl(QtDisplayPreviewSource source) : binding(source.node_id, source.serial) {
        struct Remote {
            int fd;
            ~Remote() { if (fd >= 0) close(fd); }
        } remote{source.remote_fd};
        if (source.node_id == UINT32_MAX || source.serial == 0 || source.remote_fd < -1) { failed = true; return; }
        pw_init(nullptr, nullptr);
        loop = pw_main_loop_new(nullptr);
        if (!loop) { failed = true; return; }
        context = pw_context_new(pw_main_loop_get_loop(loop), nullptr, 0);
        if (!context) { failed = true; return; }
        if (remote.fd >= 0) {
            // connect_fd takes ownership on both success and failure.
            core = pw_context_connect_fd(context, std::exchange(remote.fd, -1), nullptr, 0);
        } else {
            core = pw_context_connect(context, nullptr, 0);
        }
        if (!core) { failed = true; return; }
        registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
        if (!registry) { failed = true; return; }
        static const auto events = [] {
            pw_registry_events value{};
            value.version = PW_VERSION_REGISTRY_EVENTS;
            value.global = global;
            value.global_remove = removed;
            return value;
        }();
        pw_registry_add_listener(registry, &registryListener, &events, this);
    }
    ~Impl() {
        if (stream) { spa_hook_remove(&streamListener); pw_stream_destroy(stream); }
        if (registry) { spa_hook_remove(&registryListener); pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry)); }
        if (core) pw_core_disconnect(core);
        if (context) pw_context_destroy(context);
        if (loop) pw_main_loop_destroy(loop);
    }
    static void global(void *data, uint32_t id, uint32_t, const char *type, uint32_t, const spa_dict *props) {
        auto &self = *static_cast<Impl *>(data);
        if (!props || !type || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0) return;
        const auto name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
        const auto media = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        const auto serial = spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL);
        if (name && media && serial) self.binding.observe(id, QString::fromUtf8(name), QString::fromUtf8(media), QString::fromUtf8(serial));
    }
    static void removed(void *data, uint32_t id) { static_cast<Impl *>(data)->binding.remove(id); }
    static void stateChanged(void *data, pw_stream_state, pw_stream_state next, const char *) {
        if (next == PW_STREAM_STATE_ERROR) static_cast<Impl *>(data)->failed = true;
    }
    static void formatChanged(void *data, uint32_t id, const spa_pod *param) {
        auto &self = *static_cast<Impl *>(data);
        if (id != SPA_PARAM_Format || !param) return;
        if (spa_format_video_raw_parse(param, &self.format) < 0
            || self.format.format != SPA_VIDEO_FORMAT_RGBA || self.format.size.width == 0
            || self.format.size.height == 0 || self.format.size.width > 1920 || self.format.size.height > 1080) {
            self.failed = true; return;
        }
        uint8_t buffer[512];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const spa_pod *params[] = {static_cast<spa_pod *>(spa_pod_builder_add_object(&builder,
            SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
            SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
            SPA_PARAM_BUFFERS_size, SPA_POD_Int(int(self.format.size.width*self.format.size.height*4)),
            SPA_PARAM_BUFFERS_stride, SPA_POD_Int(int(self.format.size.width*4)),
            SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int((1 << SPA_DATA_MemPtr) | (1 << SPA_DATA_MemFd))))};
        if (pw_stream_update_params(self.stream, params, 1) < 0) self.failed = true;
    }
    static void process(void *data) {
        auto &self = *static_cast<Impl *>(data);
        auto *buffer = pw_stream_dequeue_buffer(self.stream);
        if (!buffer) return;
        struct ReturnBuffer {
            pw_stream *stream;
            pw_buffer *buffer;
            ~ReturnBuffer() { pw_stream_queue_buffer(stream, buffer); }
        } release{self.stream, buffer};
        if (!buffer->buffer || buffer->buffer->n_datas == 0 || !buffer->buffer->datas) return;
        const auto &bytes = buffer->buffer->datas[0];
        if (!bytes.data || !bytes.chunk || bytes.chunk->size == 0
            || (bytes.chunk->flags & SPA_CHUNK_FLAG_CORRUPTED)) return;
        const uint64_t end = uint64_t(bytes.chunk->offset) + bytes.chunk->size;
        if (end > bytes.maxsize) return;
        auto image = CameraPreview::frameFromRgba(QByteArray::fromRawData(static_cast<const char *>(bytes.data), qsizetype(end)),
            self.format.size.width, self.format.size.height, bytes.chunk->stride, bytes.chunk->offset);
        if (!image.isNull()) self.latest = std::move(image);
    }
    void connectStream() {
        if (stream || binding.serial().isEmpty() || binding.retired()) return;
        const auto serial = binding.serial().toUtf8();
        stream = pw_stream_new(core, "Vauchi Qt camera preview", pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Camera",
            PW_KEY_TARGET_OBJECT, serial.constData(), nullptr));
        if (!stream) { failed = true; return; }
        static const auto events = [] {
            pw_stream_events value{};
            value.version = PW_VERSION_STREAM_EVENTS;
            value.state_changed = stateChanged;
            value.param_changed = formatChanged;
            value.process = process;
            return value;
        }();
        pw_stream_add_listener(stream, &streamListener, &events, this);
        uint8_t buffer[512];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        spa_rectangle size{640, 480};
        const spa_pod *params[] = {static_cast<spa_pod *>(spa_pod_builder_add_object(&builder,
            SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format, SPA_POD_Id(SPA_VIDEO_FORMAT_RGBA),
            SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&size)))};
        if (pw_stream_connect(stream, PW_DIRECTION_INPUT, binding.nodeId(),
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_DONT_RECONNECT), params, 1) < 0)
            failed = true;
    }
    PreviewBinding binding;
    bool failed = false;
    pw_main_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_registry *registry = nullptr;
    pw_stream *stream = nullptr;
    spa_hook registryListener{};
    spa_hook streamListener{};
    spa_video_info_raw format{};
    std::optional<QImage> latest;
};

PreviewReader::PreviewReader(QtDisplayPreviewSource source) : m_impl(std::make_unique<Impl>(source)) {}
PreviewReader::~PreviewReader() = default;
bool PreviewReader::closed() const { return m_impl->failed || m_impl->binding.retired(); }
std::optional<QImage> PreviewReader::poll() {
    if (closed()) return {};
    auto *loop = pw_main_loop_get_loop(m_impl->loop);
    pw_loop_enter(loop);
    const auto result = pw_loop_iterate(loop, 0);
    pw_loop_leave(loop);
    if (result < 0) m_impl->failed = true;
    if (closed()) return {};
    m_impl->connectStream();
    return std::exchange(m_impl->latest, {});
}
#else
struct PreviewReader::Impl {};
PreviewReader::PreviewReader(QtDisplayPreviewSource source) : m_impl(std::make_unique<Impl>()) {
#ifdef Q_OS_UNIX
    if (source.remote_fd >= 0) close(source.remote_fd);
#else
    Q_UNUSED(source);
#endif
}
PreviewReader::~PreviewReader() = default;
bool PreviewReader::closed() const { return true; }
std::optional<QImage> PreviewReader::poll() { return {}; }
#endif
