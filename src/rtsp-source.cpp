#include "rtsp-source.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <gst/video/video.h>
#include <gst/app/gstappsink.h>

#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// 静的 obs_source_info 定義
// ─────────────────────────────────────────────────────────────────────────────
static obs_source_info s_source_info = {};

obs_source_info *RtspSource::get_source_info()
{
    s_source_info.id             = "sao800_rtsp_source";
    s_source_info.type           = OBS_SOURCE_TYPE_INPUT;
    s_source_info.output_flags   = OBS_SOURCE_ASYNC_VIDEO;
    s_source_info.get_name       = RtspSource::get_name;
    s_source_info.create         = RtspSource::create;
    s_source_info.destroy        = RtspSource::destroy;
    s_source_info.update         = RtspSource::update;
    s_source_info.get_properties = RtspSource::get_properties;
    s_source_info.get_defaults   = RtspSource::get_defaults;
    return &s_source_info;
}

// ─────────────────────────────────────────────────────────────────────────────
// ライフサイクル
// ─────────────────────────────────────────────────────────────────────────────
RtspSource::RtspSource(obs_source_t *source)
    : m_source(source)
{
}

RtspSource::~RtspSource()
{
    stop_pipeline();
}

void *RtspSource::create(obs_data_t *settings, obs_source_t *source)
{
    blog(LOG_WARNING, "[SAO800] create() called");
    auto *self = new RtspSource(source);
    blog(LOG_WARNING, "[SAO800] RtspSource instance created, calling update()");
    self->update(self, settings);
    blog(LOG_WARNING, "[SAO800] create() done");
    return self;
}

void RtspSource::destroy(void *data)
{
    delete static_cast<RtspSource *>(data);
}

void RtspSource::update(void *data, obs_data_t *settings)
{
    auto *self = static_cast<RtspSource *>(data);
    std::string new_url = obs_data_get_string(settings, "rtsp_url");

    blog(LOG_INFO, "[SAO800] update() called, url='%s' (current='%s')",
         new_url.c_str(), self->m_url.c_str());

    // URLが変わっていない場合は何もしない（keystroke毎のupdate対策）
    // ただしURLが設定されているのにパイプラインが未起動の場合は起動する
    if (new_url == self->m_url && self->m_pipeline != nullptr) return;

    self->stop_pipeline();
    self->m_url = new_url;

    // rtsp:// で始まる完全なURLのみ起動する
    if (self->m_url.size() >= 7 && self->m_url.substr(0, 7) == "rtsp://")
        self->start_pipeline();
    else
        blog(LOG_INFO, "[SAO800] URL未設定またはrtsp://以外のためパイプライン起動スキップ");
}

// ─────────────────────────────────────────────────────────────────────────────
// プロパティ（OBS設定UI）
// ─────────────────────────────────────────────────────────────────────────────
obs_properties_t *RtspSource::get_properties(void *)
{
    auto *props = obs_properties_create();
    obs_properties_add_text(props, "rtsp_url", "RTSP URL", OBS_TEXT_DEFAULT);
    return props;
}

void RtspSource::get_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "rtsp_url", "");
}

// ─────────────────────────────────────────────────────────────────────────────
// GStreamerパイプライン
//
// rtspsrc latency=0 → rtph264depay → h264parse → avdec_h264 → videoconvert
// → video/x-raw,format=I420 → appsink
//
// vtdec_hwはこのカメラのH.264プロファイルで not-negotiated になるため
// 現時点はavdec_h264を使用。vtdec_hw問題解決後に差し替え予定。
// ─────────────────────────────────────────────────────────────────────────────
void RtspSource::start_pipeline()
{
    if (m_url.empty()) return;
    blog(LOG_INFO, "[SAO800] start_pipeline() url='%s'", m_url.c_str());

    // gst_parse_launchの構文としてURLを解釈させないよう、GLibの規則で引用する。
    // 認証情報に空白や引用符が含まれてもパイプライン要素を注入させない。
    gchar *quoted_url = g_shell_quote(m_url.c_str());
    std::string pipeline_str =
        "rtspsrc location=" + std::string(quoted_url) + " latency=0 protocols=tcp ! "
        "rtph264depay ! "
        "h264parse ! "
        "avdec_h264 ! "
        "videoconvert ! "
        "video/x-raw,format=NV12 ! "
        "appsink name=sink emit-signals=true sync=false max-buffers=2 drop=true";
    g_free(quoted_url);

    blog(LOG_INFO, "[SAO800] pipeline: %s", pipeline_str.c_str());
    GError *err = nullptr;
    m_pipeline = gst_parse_launch(pipeline_str.c_str(), &err);
    if (err) {
        blog(LOG_ERROR, "[SAO800] gst_parse_launch failed: %s", err->message);
        g_error_free(err);
        if (m_pipeline) {
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
        }
        return;
    }
    if (!m_pipeline) {
        blog(LOG_ERROR, "[SAO800] gst_parse_launch returned null (no error set)");
        return;
    }
    blog(LOG_INFO, "[SAO800] pipeline created, setting to PLAYING");

    m_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    if (!m_appsink) {
        blog(LOG_ERROR, "[SAO800] appsink 'sink' not found");
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return;
    }
    GstAppSinkCallbacks cbs = {};
    cbs.new_sample = RtspSource::on_new_sample;
    gst_app_sink_set_callbacks(GST_APP_SINK(m_appsink), &cbs, this, nullptr);

    // バスでエラー・EOSを監視
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    m_bus_watch_id = gst_bus_add_watch(bus, RtspSource::on_bus_message, this);
    gst_object_unref(bus);

    m_loop    = g_main_loop_new(nullptr, FALSE);
    m_running = true;

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        blog(LOG_ERROR, "[SAO800] failed to set pipeline to PLAYING");
        stop_pipeline();
        return;
    }

    m_gst_thread = std::thread([this] {
        g_main_loop_run(m_loop);
    });
}

void RtspSource::stop_pipeline()
{
    if (!m_pipeline) return;
    m_running = false;

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    if (m_bus_watch_id != 0) {
        g_source_remove(m_bus_watch_id);
        m_bus_watch_id = 0;
    }
    if (m_loop) {
        g_main_loop_quit(m_loop);
    }
    if (m_gst_thread.joinable())
        m_gst_thread.join();
    if (m_loop) {
        g_main_loop_unref(m_loop);
        m_loop = nullptr;
    }

    if (m_appsink) {
        gst_object_unref(m_appsink);
        m_appsink = nullptr;
    }
    gst_object_unref(m_pipeline);
    m_pipeline = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// フレームをOBSに渡す
// ─────────────────────────────────────────────────────────────────────────────
void RtspSource::push_frame(GstSample *sample)
{
    static std::atomic<int> frame_count{0};
    int n = ++frame_count;
    if (n == 1 || n == 30)
        blog(LOG_WARNING, "[SAO800] push_frame #%d called", n);
    GstBuffer     *buf  = gst_sample_get_buffer(sample);
    GstCaps       *caps = gst_sample_get_caps(sample);
    GstVideoInfo   vinfo;

    if (!gst_video_info_from_caps(&vinfo, caps)) {
        blog(LOG_ERROR, "[SAO800] gst_video_info_from_caps failed");
        return;
    }

    if (n == 1)
        blog(LOG_WARNING, "[SAO800] frame: %dx%d fmt=%d stride=[%d,%d,%d] offset=[%zu,%zu,%zu]",
             vinfo.width, vinfo.height, (int)GST_VIDEO_INFO_FORMAT(&vinfo),
             vinfo.stride[0], vinfo.stride[1], vinfo.stride[2],
             vinfo.offset[0], vinfo.offset[1], vinfo.offset[2]);

    GstMapInfo map;
    if (!gst_buffer_map(buf, &map, GST_MAP_READ)) {
        blog(LOG_ERROR, "[SAO800] gst_buffer_map failed");
        return;
    }

    obs_source_frame frame = {};
    frame.width     = vinfo.width;
    frame.height    = vinfo.height;
    frame.format    = VIDEO_FORMAT_NV12;
    frame.timestamp = os_gettime_ns();

    // NV12: plane0=Y, plane1=UV(interleaved)
    frame.data[0]     = map.data + vinfo.offset[0];
    frame.linesize[0] = vinfo.stride[0];
    frame.data[1]     = map.data + vinfo.offset[1];
    frame.linesize[1] = vinfo.stride[1];

    obs_source_output_video(m_source, &frame);

    gst_buffer_unmap(buf, &map);
}

GstFlowReturn RtspSource::on_new_sample(GstAppSink *sink, gpointer user_data)
{
    auto *self   = static_cast<RtspSource *>(user_data);
    GstSample *s = gst_app_sink_pull_sample(sink);
    if (s) {
        self->push_frame(s);
        gst_sample_unref(s);
    }
    return GST_FLOW_OK;
}

gboolean RtspSource::on_bus_message(GstBus *, GstMessage *msg, gpointer user_data)
{
    auto *self = static_cast<RtspSource *>(user_data);
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError *err = nullptr;
        gchar  *dbg = nullptr;
        gst_message_parse_error(msg, &err, &dbg);
        blog(LOG_ERROR, "[SAO800] GStreamer error: %s (%s)",
             err->message, dbg ? dbg : "");
        g_error_free(err);
        g_free(dbg);
        // 自動再接続は将来実装
        if (self->m_loop)
            g_main_loop_quit(self->m_loop);
        break;
    }
    case GST_MESSAGE_EOS:
        blog(LOG_WARNING, "[SAO800] EOS received, stream ended");
        if (self->m_loop)
            g_main_loop_quit(self->m_loop);
        break;
    default:
        break;
    }
    return TRUE;
}
