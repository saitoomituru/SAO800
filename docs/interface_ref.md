# SAO800 インターフェース参照

**正本:** [OND800/docs/interface_spec.md](https://github.com/saitoomituru/OND800/blob/main/docs/interface_spec.md)

**参照している正本版:** Season 3 v2.1

変更はOND800リポジトリ側が行う。このファイルはSAO800実装者向けの参照索引。

---

## SAO800が実装すべきインターフェース

## 経路の区別

- **現在の実装経路:** IPカメラのRTSPをSAO800が直接受信し、GStreamer appsinkからOBSへ渡す。
- **OND800との正規映像経路:** OND800が公開するNDIをOBS/SAO800側で受ける。将来実装。
- **制御経路:** OND800からのスロット番号契約は正本に未定義のため、現時点では実装しない。

現在のRTSPソースは単独で価値を持つ先行実装であり、NDI契約の代用品として扱わない。

### NDI映像受信（OND800 → SAO800）

- OND800が送出するNDIストリームを受信するOBSソースとして動作
- ストリーム名: `OND800_{hostname}_{camera_index}`
- デフォルト: 1920x1080 @ 30fps

### NDI MIDIメタデータ受信（OND800 → SAO800）

```xml
<ndi_midi version="1.0">
  <note channel="11" pitch="36" velocity="100" type="note_on"/>
</ndi_midi>
```

MIDIノート → DMXブリッジ、照明制御、BPMタップに使用。

### NDI上流メタデータ送信（SAO800 → OND800）

```xml
<!-- BPM解析結果 -->
<ndi_zero800_analysis version="1.0">
  <bpm value="128.5" confidence="0.95"/>
  <sentiment score="0.8" label="盛り上がり"/>
</ndi_zero800_analysis>

<!-- PTZ制御 -->
<ndi_zero800_control version="1.0">
  <ptz pan="0.3" tilt="-0.1" zoom="1.0"/>
</ndi_zero800_control>
```

### スペック申告（接続時・obs-websocket v5経由）

OND800が接続を検出したら即座に送信する。

```json
{
  "type": "sao800_capabilities",
  "features": [
    "bpm_detect",
    "vad",
    "llm_sentence",
    "face_encode",
    "encode_offload",
    "sentiment",
    "midi_dmx_bridge"
  ]
}
```

実装済みfeatureのみ `features` に含める。OND800はこのリストでGUI選択肢を動的生成する。

### 解像度ヒント（Connection Metadata）

```xml
<ndi_capabilities_info>
  <video xres="1920" yres="1080" frame_rate_N="30000" frame_rate_D="1001"/>
</ndi_capabilities_info>
```

**最終決定権はOND800が持つ。**

---

## SAO800が知らなくていいこと

- 顔データ・Identity Store（OND800の所有物）
- FAN800のUUID・物理状態
- OBSのシーン構成の詳細（obs-websocket v5で命令を受けるだけ）

---

## MIDIチャンネル割当（Loadout準拠）

| チャンネル | 用途 |
|---|---|
| 10 | ドラム（FAN800発火フィードバック） |
| 11 | FAN800演出イベント |
| 12 | エフェクト |

詳細はOND800 `docs/datastore_arch.md` のLoadoutセクション参照。
