#pragma once

#include <obs-module.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <atomic>
#include <string>
#include <thread>

// ─────────────────────────────────────────────────────────────────────────────
// RtspSource
//
// OBS custom source: RTSPストリームをGStreamerでデコードし、
// appsink経由でobs_source_frameとして直接OBSに渡す。
// NDI中継不要。OBSのMetalコンポジットに寄生する。
// ─────────────────────────────────────────────────────────────────────────────
class RtspSource {
public:
    explicit RtspSource(obs_source_t *source);
    ~RtspSource();

    // obs_source_info callbacks
    static obs_source_info *get_source_info();
    static const char *get_name(void *) { return "SAO800 RTSPソース"; }

    static void *create(obs_data_t *settings, obs_source_t *source);
    static void destroy(void *data);
    static void update(void *data, obs_data_t *settings);
    static obs_properties_t *get_properties(void *data);
    static void get_defaults(obs_data_t *settings);

private:
    obs_source_t *m_source;
    std::string   m_url;

    GstElement   *m_pipeline = nullptr;
    GstElement   *m_appsink  = nullptr;
    GMainLoop    *m_loop     = nullptr;
    guint         m_bus_watch_id = 0;
    std::thread   m_gst_thread;
    std::atomic<bool> m_running{false};

    void start_pipeline();
    void stop_pipeline();
    void push_frame(GstSample *sample);

    // appsink new-sample callback
    static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data);
    // bus error/eos watcher
    static gboolean on_bus_message(GstBus *bus, GstMessage *msg, gpointer user_data);
};
