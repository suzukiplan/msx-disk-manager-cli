# SUZUKI PLAN - MSX Disk Manager for CLI

所要で MSX のディスクイメージを作りたかったのですが、Windowsのフリーソフトを使うしか今のところ手段が無いので、私が必要とする最低限の機能要件を満たすディスクイメージ操作ユーティリティを作ってみました。

> 要望などあれば [issues](https://github.com/suzukiplan/msx-disk-manager-cli/issues) を切っていただければ対応するかもしれません。

## Pre-requests

- GNU Make
- CLANG (C++)

## How to Build and Test

`make` を実行すればビルドとテストが実行されます。

テストで行っていること:

1. WebMSXで作成したディスクイメージから MSX-BASIC のコードを `cp` コマンドで読み込む
2. create コマンドで新規ディスクイメージを生成しつつ 1 で読み込んだ MSX-BASIC のコードを配置
3. 生成したディスクイメージの `info` を確認
4. 生成したディスクイメージの `ls` を確認
5. 作成したディスクイメージから再度MSX-BASICのコードを読み込む（バイナリ差分が出ない事を git 上で確認できる）

```bash
% make
clang++ -o dskmgr src/dskmgr.cpp src/bas2txt.cpp
cd test && make
../dskmgr ./wmsx.dsk cp hello.bas
../dskmgr ./wmsx.dsk cp hoge.bas
../dskmgr ./image.dsk create hello.bas hoge.bas
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
Available Entries: 2/3
../dskmgr ./image.dsk ls
00:----w  HELLO.BAS           23 bytes  1980.00.00 00:00:00  (C:2, S:12)
00:----w  HOGE.BAS            30 bytes  1980.00.00 00:00:00  (C:3, S:14)
../dskmgr ./image.dsk cp hello.bas
../dskmgr ./image.dsk cp hoge.bas
../dskmgr ./image.dsk cat hello.bas
10 PRINT"HELLO,WORLD!"
../dskmgr ./image.dsk cat hoge.bas
10 CLS
20 PRINT"_____HOGE____"
```

## How to Use

|Command|Outline|
|:-:|:-|
|[create](#create)|新規ディスクイメージファイルを生成|
|[info](#info)|ディスクのブートセクタとFATの内容をダンプ|
|[ls](#ls)|ディスクに格納されているファイルの一覧を表示|
|[cp](#cp)|ディスクに格納されているファイルをローカルへコピー|
|[cat](#cat)|ディスクに格納されているファイルをローカルで標準出力|

### create

```bash
./dskmgr image.dsk create [files]
```

- 新規のフォーマット済みのディスクイメージファイル (`image.dsk`) を作成します
- `files` (複数指定可能) へ指定したファイルが書き込まれたディスクイメージが生成されます
  - ファイルサイズやファイル数の上限を超える場合は `Disk Full` エラーで書き込みが失敗します
- `files` を指定しなければ空のディスクイメージが生成されます

### info

```bash
./dskmgr image.dsk info
```

ディスクのブートセクタとFATの内容をダンプします

### ls

```bash
./dskmgr image.dsk ls
```

ディスクに格納されているファイルの一覧を表示します

### cp

```bash
./dskmgr image.dsk cp filename
```

- `filename` で指定した `image.dsk` 内のファイルをローカルへコピーします
- `filename` は大文字と小文字を区別しません（全て大文字と解釈されます）

> BASIC の場合は `cat` コマンドを使えば中間言語からテキスト形式に変換することができます。

### cat

```bash
./dskmgr image.dsk cat filename
```

- `filename` で指定した `image.dsk` 内のファイルをローカルへ標準出力します
- `filename` は大文字と小文字を区別しません（全て大文字と解釈されます）
- 拡張子が `.BAS` の場合、テキストに変換して標準出力します
