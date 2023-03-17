# SUZUKI PLAN - MSX Disk Manager for CLI **(WIP)**

所要で MSX のディスクイメージを作りたかったのですが、「Windowsのフリーソフトを使うように」という情報しかなく、Windowsが動くパソコンが壊れていて動かないので mac でも動く MSX のディスクユーティリティが欲しくて作成中です。

## WIP status

- [ ] Create New Disk Image
- [x] Dump Disk Information
- [x] List
- [ ] Copy to Local

## Pre-requests

- GNU Make
- CLANG (C++)

## How to Build

```bash
make
```

## How to Test

```bash
% make test
clang++ -o dskmgr src/dskmgr.cpp
========================================
cd test && ../dskmgr ./wmsx.dsk info
[Boot Sector]
            OEM: WMSX    
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
Available Entries: 0/3
========================================
cd test && ../dskmgr ./wmsx.dsk ls
----w  HELLO.BAS           23 bytes  2023.03.17 09:37:56  (C:2, S:12)
----w  HOGE.BAS            30 bytes  2023.03.17 09:38:54  (C:3, S:14)
========================================
cd test && ../dskmgr ./wmsx.dsk cp hello.bas
cd test && ../dskmgr ./wmsx.dsk cp hoge.bas
```

## How to Use

### Create New Disk Image

```bash
./dskmgr create image.dsk [files]
```

- 新規のフォーマット済みのディスクイメージファイル (`image.dsk`) を作成します
- `files` (複数指定可能) へ指定したファイルが書き込まれたディスクイメージが生成されます
  - ファイルサイズやファイル数の上限を超える場合は `Disk Full` エラーで書き込みが失敗します
- `files` を指定しなければ空のディスクイメージが生成されます

### Dump Disk Information

```bash
./dskmgr info image.dsk
```

ディスクのブートセクタとFATの内容をダンプします。

### List

```bash
./dskmgr ls image.dsk
```

ディスクに格納されているファイルの一覧を表示します

### Copy to Local

```bash
./dskmgr cp image.dsk filename
```

- `filename` で指定した `image.dsk` 内のファイルをローカルへコピーします。
- `filename` は大文字と小文字を区別しません（全て大文字と解釈されます）
