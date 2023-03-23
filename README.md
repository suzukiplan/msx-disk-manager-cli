# MSX Disk Manager for CLI [![CircleCI](https://dl.circleci.com/status-badge/img/gh/suzukiplan/msx-disk-manager-cli/tree/master.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/suzukiplan/msx-disk-manager-cli/tree/master)

Windows, Linux, macOS などで使用できる fMSX 形式のディスクイメージファイル（.dsk）用のコマンドライン版ユーティリティです。

> 要望などあれば [issues](https://github.com/suzukiplan/msx-disk-manager-cli/issues) を切っていただければ対応するかもしれません。（もちろん Pull Request も歓迎します）
>
> なお、テストは Linux (CI) と macOS (実機) で実施していますが、Windows は持っていないのでノーチェックです。

## Features

- **create:** ローカルファイルからディスクイメージファイルを生成
- **info/ls:** ディスクイメージファイルの各種情報を確認
- **get:** ディスクイメージファイル内の特定ファイルをローカルへ取得
- **put:** 特定のローカルファイルをディスクイメージへ書き込み
- **cat:** ディスクイメージファイル内の特定ファイルをローカルへ標準出力
- **rm:** ディスクイメージファイル内の特定ファイルを削除
- MSX-BASIC の テキスト⇔中間言語 を 相互変換:
  - `create` と `put` でテキスト形式の `.BAS` ファイルを書き込むと中間言語形式に自動変換
  - `cat` で　`.BAS` ファイルを標準出力する時にテキスト形式に自動変換

## Pre-requests

- GNU Make
- CLANG (C++)

## How to Build and Test

`make` を実行すればビルドとテストが実行されます。

テストで行っていること:

1. WebMSXで作成したディスクイメージから MSX-BASIC のコードを `get` コマンドで読み込む
2. `create` コマンドで新規ディスクイメージを生成
3. 生成したディスクイメージの情報を `info` コマンドで確認
4. 生成したディスクイメージの情報を `ls` コマンドで確認
5. 作成したディスクイメージからMSX-BASICのコードを `get` コマンドで読み込む（バイナリ差分が出ない事を git 上で確認用）
6. 作成したディスクイメージからMSX-BASICのコードを `cat` コマンドで出力

```bash
% make
clang++ --std=c++14 -o dskmgr src/dskmgr.cpp
cd test && make
../dskmgr ./wmsx.dsk get hello.bas
../dskmgr ./wmsx.dsk get hoge.bas
../dskmgr ./image.dsk create hello.bas hoge.bas attrac.bas barcode.bas blocks1.bas
hello.bas: Write to disk as a binary file ... 23 bytes
hoge.bas: Write to disk as a binary file ... 30 bytes
attrac.bas: Convert to MSX-BASIC intermediate code ... 978 -> 906 bytes
barcode.bas: Convert to MSX-BASIC intermediate code ... 466 -> 433 bytes
blocks1.bas: Convert to MSX-BASIC intermediate code ... 2097 -> 1877 bytes
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
- dirent#0 (HELLO.BAS) ... 2
- dirent#1 (HOGE.BAS) ... 3
- dirent#2 (ATTRAC.BAS) ... 4
- dirent#3 (BARCODE.BAS) ... 5
- dirent#4 (BLOCKS1.BAS) ... 6,7
Total using cluster: 7 (7168 bytes)
../dskmgr ./image.dsk ls
00:----w  HELLO.BAS           23 bytes  2023.03.23 14:52:24  (C:2, S:14)
00:----w  HOGE.BAS            30 bytes  2023.03.23 14:52:24  (C:3, S:16)
00:----w  ATTRAC.BAS         906 bytes  2023.03.23 14:52:24  (C:4, S:18)
00:----w  BARCODE.BAS        433 bytes  2023.03.23 14:52:24  (C:5, S:20)
00:----w  BLOCKS1.BAS       1877 bytes  2023.03.23 14:52:24  (C:6, S:22)
../dskmgr ./image.dsk get hello.bas
../dskmgr ./image.dsk get hoge.bas
../dskmgr ./image.dsk cat hello.bas
10 PRINT"HELLO,WORLD!"
../dskmgr ./image.dsk cat hoge.bas
10 CLS
20 PRINT"_____HOGE____"
```

## Manual

|Command|Outline|
|:-:|:-|
|[create](#create)|新規ディスクイメージファイルを生成|
|[info](#info)|ディスクのブートセクタと FAT (FAT12) の内容をダンプ|
|[ls](#ls)|ディスクに格納されているファイルの一覧を表示|
|[get](#get)|ディスクに格納されているファイルをローカルへ取得|
|[put](#put)|ローカルファイルをディスクへ書き込む|
|[cat](#cat)|ディスクに格納されているファイルをローカルで標準出力|
|[rm](#rm)|ディスクに格納されている特定のファイルを削除|

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

### get

```bash
./dskmgr image.dsk get filename [as filename2]
```

- `filename` で指定した `image.dsk` 内のファイルをローカルへ取得します
- `[as filename2]` を指定した場合はローカルでは `filename2` で保存されます
- `filename` は大文字と小文字を区別しません（全て大文字と解釈されます）

> BASIC (.BASファイル) の場合は `cat` コマンドを使えば中間言語からテキスト形式に変換することができます。

### put

```bash
./dskmgr image.dsk put filename [as filename2]
```

- `filename` で指定したローカルファイルを `image.dsk` 内へコピーします
- `[as filename2]` を指定した場合は `image.dsk` には `filename2` で保存されます
- `filename` または `filename2` は大文字と小文字を区別しません（全て大文字と解釈されます）
- `image.dsk` 内に `filename` または `filename2` と同じファイル名が存在する場合は上書きされます
- `image.dsk` 内に `filename` または `filename2` と同じファイル名が存在しない場合は新規追加されます
- テキスト形式のBASIC（.BAS）ファイルは中間言語形式に自動変換されます
- ファイルサイズやファイル数の上限を超える場合は `Disk Full` エラーで書き込みが失敗します

### cat

```bash
./dskmgr image.dsk cat filename
```

- `filename` で指定した `image.dsk` 内のファイルをローカルへ標準出力します
- `filename` は大文字と小文字を区別しません（全て大文字と解釈されます）
- 拡張子が `.BAS` の場合、テキストに変換して標準出力します

### rm

```bash
./dskmgr image.dsk rm filename
```

- `filename` で指定した `image.dsk` 内のファイルを削除します

## License

- MSX Disk Manager for CLI ([src/dskmgr.cpp](src/dskmgr.cpp))
  - [MIT](LICENSE.txt)
- MSX-BASIC Filter ([src/basic.hpp](src/basic.hpp))
  - [MIT](LICENSE.txt)
- Test Programs:
  - MSX: Gilbert François Duivesteijn
    - URL: [https://github.com/gilbertfrancois/msx](https://github.com/gilbertfrancois/msx)
    - [Apache License Version 2.0](https://github.com/gilbertfrancois/msx/blob/master/LICENSE)
