# MSX Disk Manager for CLI [![CircleCI](https://dl.circleci.com/status-badge/img/gh/suzukiplan/msx-disk-manager-cli/tree/master.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/suzukiplan/msx-disk-manager-cli/tree/master)

Windows, Linux, macOS などで使用できる fMSX 形式のディスクイメージファイル（.dsk）用のコマンドライン版ユーティリティです。

> 要望などあれば [issues](https://github.com/suzukiplan/msx-disk-manager-cli/issues) を切っていただければ対応するかもしれません。（もちろん Pull Request も歓迎します）

## Features

- ローカルファイルからディスクイメージファイルを生成（.BASファイルはテキスト形式であれば中間コード形式に変換される）
- ディスクイメージファイルの各種データ確認
- ディスクイメージファイル内の特定ファイルをローカルへ取得（コピー）
- ディスクイメージファイル内の特定ファイルをローカルへ標準出力（.BASファイルは中間言語形式からテキスト形式に変換される）

## Pre-requests

- GNU Make
- CLANG (C++)

## How to Build and Test

`make` を実行すればビルドとテストが実行されます。

テストで行っていること:

1. WebMSXで作成したディスクイメージから MSX-BASIC のコードを `cp` コマンドで読み込む
2. `create` コマンドで新規ディスクイメージを生成:
  a. HELLO.BAS: 1 で読み込んだ MSX-BASIC のコード
  b. HOGE.BAS: 1 で読み込んだ MSX-BASIC のコード
  c. TEXT.BAS: テキスト形式の MSX-BASIC のコード (中間コードに自動変換される)
3. 生成したディスクイメージの情報を `info` コマンドで確認
4. 生成したディスクイメージの情報を `ls` コマンドで確認
5. 作成したディスクイメージからMSX-BASICのコードを `cp` コマンドで読み込む（バイナリ差分が出ない事を git 上で確認用）
6. 作成したディスクイメージからMSX-BASICのコードを `cat` コマンドで出力:
  a. HELLO.BAS: 標準出力
  b. HOGE.BAS: 標準出力
  c. TEXT.BAS: ファイル出力（テキスト差分が出ないことを git 上で確認用）

```bash
% make
clang++ --std=c++14 -o dskmgr src/dskmgr.cpp
cd test && make
../dskmgr ./wmsx.dsk cp hello.bas
../dskmgr ./wmsx.dsk cp hoge.bas
../dskmgr ./image.dsk create hello.bas hoge.bas text.bas
hello.bas: Write to disk as a binary file ... 23 bytes
hoge.bas: Write to disk as a binary file ... 30 bytes
text.bas: Convert to MSX-BASIC intermediate code ... 245 -> 200 bytes
../dskmgr ./image.dsk info
[Boot Sector]
            OEM: SZKPLN01
       Media ID: 0xF9
    Sector Size: 512 bytes
  Total Sectors: 1440
   Cluster Size: 1024 bytes (2 sectors)
   FAT Position: 1
       FAT Size: 1536 bytes (3 sectors)
       FAT Copy: 2
Creatable Files: 112
        Sectors: 9 per track
     Disk Sides: 2
 Hidden Sectors: 0

[FAT]
Fat ID: 0xF9
- Entry#1 = 1024 bytes (1 cluster) ... 2: HELLO.BAS
- Entry#2 = 1024 bytes (1 cluster) ... 3: HOGE.BAS
- Entry#3 = 1024 bytes (1 cluster) ... 4: TEXT.BAS
Available Entries: 3/4
../dskmgr ./image.dsk ls
00:----w  HELLO.BAS           23 bytes  1980.00.00 00:00:00  (C:2, S:12)
00:----w  HOGE.BAS            30 bytes  1980.00.00 00:00:00  (C:3, S:14)
00:----w  TEXT.BAS           200 bytes  1980.00.00 00:00:00  (C:4, S:16)
../dskmgr ./image.dsk cp hello.bas
../dskmgr ./image.dsk cp hoge.bas
../dskmgr ./image.dsk cat hello.bas
10 PRINT"HELLO,WORLD!"
../dskmgr ./image.dsk cat hoge.bas
10 CLS
20 PRINT"_____HOGE____"
../dskmgr ./image.dsk cat text.bas >text.bas
```

## Manual

|Command|Outline|
|:-:|:-|
|[create](#create)|新規ディスクイメージファイルを生成|
|[info](#info)|ディスクのブートセクタと FAT (FAT12) の内容をダンプ|
|[ls](#ls)|ディスクに格納されているファイルの一覧を表示|
|[cp](#cp)|ディスクに格納されているファイルをローカルへコピー|
|[cat](#cat)|ディスクに格納されているファイルをローカルで標準出力|

### create

```bash
./dskmgr image.dsk create [files]
```

- 新規のフォーマット済みのディスクイメージファイル (`image.dsk`) を作成します
- `files` (複数指定可能) へ指定したファイルが書き込まれた `image.dsk` が生成されます
  - テキスト形式のBASIC（.BAS）ファイルは中間言語形式に自動変換されます
  - ファイルサイズやファイル数の上限を超える場合は `Disk Full` エラーで書き込みが失敗します
- `files` を指定しなければ空の `image.dsk` が生成されます

### info

```bash
./dskmgr image.dsk info
```

`image.dsk` のブートセクタと FAT (FAT12) の内容をダンプします

### ls

```bash
./dskmgr image.dsk ls
```

`image.dsk` に格納されているファイルの一覧を表示します

### cp

```bash
./dskmgr image.dsk cp filename
```

- `filename` で指定した `image.dsk` 内のファイルをローカルへコピーします
- `filename` は大文字と小文字を区別しません（全て大文字と解釈されます）

> BASIC (.BASファイル) の場合は `cat` コマンドを使えば中間言語からテキスト形式に変換することができます。

### cat

```bash
./dskmgr image.dsk cat filename
```

- `filename` で指定した `image.dsk` 内のファイルをローカルへ標準出力します
- `filename` は大文字と小文字を区別しません（全て大文字と解釈されます）
- 拡張子が `.BAS` の場合、テキストに変換して標準出力します
