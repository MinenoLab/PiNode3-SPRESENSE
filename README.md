# README

## 概要
このプロジェクトは、SPRESENSEを使用し、ノートPCでデータを確認するためのツールを提供します。

## ファイル構成
```
C:.
│  .gitattributes
│  README.md
│
├─arduino
│      arduino.ino
│
└─python
    │  image_checker.py
    │  requirements.txt
    │
    └─lib
            Spresense.py
            SpresenseCameraChecker.py
            __init__.py
```

## 手順

### SPRESENSEのセットアップ
1. Arduino IDEのメニューから\[ツール\]-\[Memory\]設定を`1536KB`に変更してください。
2. `arduino`フォルダ内の`arduino.ino`ファイルをSPRESENSEデバイスに書き込んでください。


### ノートPCでのデータ確認
- Python環境を使用する場合、`Python/image_checker.py`スクリプトを実行してください。
- スクリプトを実行する前に、`requirements.txt`から必要なPython依存関係をインストールしてください。

## 注意事項
- セットアップを進める前に、SPRESENSE用の必要なドライバおよびライブラリをすべてインストールしてください。
- 問題が発生した場合は、`python/lib`ディレクトリ内の提供されたスクリプトやドキュメントを参照してください。
- SPRESENSEの初期セットアップは[公式のドキュメント](https://developer.sony.com/spresense/development-guides/arduino_set_up_ja.html)を参照してください
 
