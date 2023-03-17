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
