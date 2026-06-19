# 2026-06-19 RTSPラグ積算問題解決・SAO800始動

## 作業者
Claude (Sonnet 4.6) + みつる

## 発端
OND800記事（https://note.com/fusamofu326/n/n51d42eb48a1a）のANRAN AR-W360-POE RTSPソースが
OBSのメディアソースで時間とともにラグが積算していく問題の解決。

## 問題の原因
OBSのメディアソース（FFmpegベース）はRTSPのPTS/DTSが不正確なカメラ（中華系）で
受信バッファが積算する。ANRANのH.264タイムスタンプが不正確なことが主因。

## 試したこと・結果

### 1. VLCビデオソース
→ Mac版OBSはVLC入れてもVLCビデオソースが表示されない（ビット数不一致の既知問題）
→ 断念

### 2. GStreamer → NDI変換
→ **解決策として採用**

**環境構築手順:**
```bash
brew install gstreamer
# NDI SDK: https://downloads.ndi.tv/Tools/NDIToolsInstaller.pkg からインストール
```

**動作コマンド:**
```bash
DYLD_LIBRARY_PATH=/usr/local/lib gst-launch-1.0 \
  rtspsrc location="rtsp://admin:admin@192.168.0.215:554/stream1" latency=0 \
  ! rtph264depay \
  ! h264parse \
  ! avdec_h264 \
  ! videoconvert \
  ! ndisink ndi-name="AnranCamera"
```

OBSのNDI Sourceで `AnranCamera` を選択 → ラグゼロ確認。

### 3. vtdec_hw（HWデコード）による負荷軽減
→ **失敗**。not-negotiated (-4) エラー。
→ `config-interval=-1` 追加、`"video/x-h264,stream-format=avc,alignment=au"` 指定も試みたが解決せず。
→ このカメラのH.264プロファイルがvtdec_hwの受け入れるキャップに合わない模様。
→ 要調査: カメラ側のH.264プロファイル確認、vtdec_hwのキャップ詳細確認。

### 4. avdec_h264 max-threads=2
→ CPU 43.5% → 30-35% に若干改善。有意な差ではない。
→ 1080p 25fps H.264ソフトウェアデコード+NDI送信でこの程度は仕方ない範囲。

## 現状のCPU使用率
- gst-launch-1.0: ~30-35%（avdec_h264 ソフトウェアデコード）
- 許容範囲内だが、vtdec_hw解決で10%以下を目指したい

## SAO800への昇華

この作業を通じてSAO800（Service Add-on OBS 800）のアーキテクチャが確定した。

**コンセプト**: OBSプラグインとしてOBSのMetalレンダラーに寄生し、以下を提供する:
1. RTSPブリッジ（GStreamer appsink → OBSテクスチャ直渡し、NDI中継不要）
2. スタンプスロットシステム（NSFW座標 → OBS Metalコンポジット）
3. OND800ペアリング（スロット番号受信・実行）
4. AIオフロード（Whisper字幕・Vision自動スイッチ・BPMシンカー）
5. フェールオーバー管制（SaaS死活監視・プラットフォームキャスト切替）
6. NSFWサニタイズレイヤー（SphereOS/LLaMa → ギャグ変換 → SaaS）

## 次にやること

- [ ] vtdec_hw問題の原因特定（カメラH.264プロファイル確認）
- [ ] OBSプラグインC++スケルトン作成
- [ ] GStreamer appsink → OBSテクスチャ直渡しの実装
- [ ] SAO800 GitHubリポジトリ公開

## 環境スナップショット
- GStreamer: 1.28.4 (`/usr/local/Cellar/gstreamer/1.28.4`)
- NDI SDK: `/usr/local/lib/libndi.dylib`
- OBS: NDI Source プラグイン導入済み
- カメラ: ANRAN AR-W360-POE, 192.168.0.215, stream1, 1080p 25fps H.264
- stream2: カメラ側でKill済み（VGA CIA 15fpsがメインストリームに遅延を引き起こすため）
