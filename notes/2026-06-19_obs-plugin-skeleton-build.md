# 2026-06-19 OBSプラグインC++スケルトン ビルド成功

## 作業者
Claude (Sonnet 4.6) + みつる

## 成果

SAO800 シーズン1第2フェーズ: OBSプラグインスケルトンのビルド成功。

### 生成物
```
SAO800/
  CMakeLists.txt
  build.sh
  cmake/MacOSXBundleInfo.plist.in
  src/
    plugin-main.cpp   — obs_module_load / obs_module_unload
    rtsp-source.hpp   — RtspSource クラス定義
    rtsp-source.cpp   — GStreamer appsink → obs_source_frame 直渡し実装
```

### ビルド済みバイナリ
- `build/sao800.plugin` (58KB, Mach-O 64-bit bundle x86_64)
- インストール先: `~/Library/Application Support/obs-studio/plugins/sao800/bin/sao800.plugin`

## ビルド手順

```bash
cd SAO800/
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build . --parallel 4
# 手動インストール（install targetも使える）
cp -r sao800.plugin ~/Library/Application\ Support/obs-studio/plugins/sao800/bin/
```

## ハマりポイントと解決策

### 1. simde not found
- OBSヘッダー（`util/sse-intrin.h`）が `simde/x86/sse2.h` を要求
- 解決: `brew install simde` → `/usr/local/include` をinclude pathに追加

### 2. obsconfig.h not found
- OBSのcmakeビルドで生成されるファイル。FetchContentではソースのみ取得されるため生成されない
- 解決: `build/obsconfig.h` を手動作成（バージョン定義と機能フラグのみ）
- 内容は `libobs/obsconfig.h.in` を参照して手で埋めた

### 3. libgstapp-1.0 not found (リンクエラー)
- `pkg-config --libs` の `-L` パスが cmake に伝わっていなかった
- 解決: `target_link_directories(sao800 PRIVATE ${GST_LIBRARY_DIRS})` を追加

## 現在のパイプライン実装

```
RTSP (ANRAN AR-W360-POE)
  → rtspsrc latency=0
  → rtph264depay → h264parse
  → avdec_h264               ← ソフトウェアデコード（vtdec_hw問題未解決）
  → videoconvert
  → video/x-raw,format=I420
  → appsink (emit-signals=true, max-buffers=2, drop=true)
  → obs_source_output_video()
  → OBS Metal コンポジット
```

NDIを経由しないダイレクトブリッジ。

## 次にやること

- [ ] OBS再起動してプラグインロード確認（ログ: `[SAO800] プラグイン読み込み完了`）
- [ ] ソース追加UIで「SAO800 RTSPソース」が表示されるか確認
- [ ] RTSP URL入力 → 映像が映るか実機確認
- [ ] vtdec_hw問題: VideoToolbox caps確認 (`gst-inspect-1.0 vtdec_hw`)
- [ ] スタンプスロットシステム設計（OBS Sceneコンポジット活用）
- [ ] OND800ペアリング受信口（UDP/WebSocket）設計

## 環境
- macOS (Hackintosh X99 Extreme4)
- OBS 32.1.2
- GStreamer 1.28.4 (Homebrew `/usr/local/Cellar/gstreamer/1.28.4`)
- AppleClang 16.0.0 / CMake 3.31.3
- simde 0.8.2 (Homebrew)
