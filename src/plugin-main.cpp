#include <obs-module.h>
#include "rtsp-source.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("sao800", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "SAO800 — Service Add-on OBS 800 "
           "(RTSPブリッジ / スタンプスロット / OND800ペアリング)";
}

bool obs_module_load(void)
{
    gst_init(nullptr, nullptr);
    obs_register_source(RtspSource::get_source_info());
    blog(LOG_INFO, "[SAO800] プラグイン読み込み完了 (GStreamer %s)",
         gst_version_string());
    return true;
}

void obs_module_unload(void)
{
    gst_deinit();
    blog(LOG_INFO, "[SAO800] プラグインアンロード");
}
