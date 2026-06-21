# SAO800 依存関係と既存ローカル差分の保全

**日付:** 2026-06-21  
**作業者:** Codex  
**追跡Issue:** #3（関連: #1, #2）

## 依存関係

- 現行のRTSP→GStreamer→appsink→OBS経路はOND800/FAN800のBLE契約に依存しない。
- OND800からのスロット番号連携は、OND800正本に契約が追加されてから実装する。
- PSYCHO-Py800MCPは別AIが作業中のため読み取り専用。SAO800から変更しない。

## セッション開始時に存在した未コミット差分

次の変更は本セッション開始前から存在していた。削除せず、由来不明の既存作業として保全する。

- `CMakeLists.txt`: OBS Frameworksへのrpath追加
- `src/plugin-main.cpp`: Homebrew GStreamerプラグイン探索パス追加
- `src/rtsp-source.cpp`: OBSロード調査ログ、RTSP TCP化、NV12出力、OBS時刻利用

内容から、OBS内でのプラグインロードと黒画面問題を調査した差分と推定する。ただし実機での成功は
このNote作成時点では再確認していない。

## 洗い出した追加リスク

- RTSP URLを文字列連結して`gst_parse_launch`へ渡しており、空白や記号を含む認証情報で壊れる。
- bus watchのsource IDを保持しておらず、停止時の確実な解除ができない。
- READMEにNDI中継とappsink直接渡しが混在し、現行経路が読み取りにくい。
- `vtdec_hw`は未解決なのでソフトウェアデコードを既定として扱う必要がある。

## ロールバック方針

1. 既存差分をそのまま独立コミットとして保全する。
2. 安全性・終了処理の修正は次の別コミットにする。
3. 文書整理もコード変更と分ける。
4. 各コミットをGitHubへpushする。

## 実機確認TODO

- OBS再起動後のプラグインロード
- 映像表示とNV12色・stride
- RTSP切断・再接続時の終了処理
- `vtdec_hw` caps negotiationの再現と回避策

## 本セッションでの安定化変更

- RTSP URLを`g_shell_quote`で引用し、`gst_parse_launch`構文への混入を防いだ。
- parse失敗時に部分生成されたpipelineを解放するようにした。
- appsink取得失敗とPLAYING遷移失敗を明示的に処理するようにした。
- bus watch IDを保持し、停止時に解除してからmain loop/threadを終了するようにした。
- これらはビルド対象だが、OBS実機での映像確認は上記TODOのまま。

## 検証結果

- `./build.sh`: 成功（CMake configure / C++ compile / bundle link完了）
- sandbox内の`sysctl`に権限制限警告が出たが、ビルド結果には影響しなかった。
- OBSへの再インストール・再起動・実映像確認はまだ行っていない。
