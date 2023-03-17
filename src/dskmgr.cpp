/**
 * SUZUKI PLAN - MSX Disk Manager for CLI
 * -----------------------------------------------------------------------------
 * The MIT License (MIT)
 * 
 * Copyright (c) 2023 Yoji Suzuki.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * -----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static unsigned char diskImage[1440][512];

// NOTE: Boundary-unaware data structure to be expanded at read time
static struct BootSector {
    unsigned char bootJump[3];
    unsigned char oemName[9];
    unsigned short secotrSize;
    unsigned char clusterSize;
    unsigned short fatPosition;
    unsigned char fatCopy;
    unsigned short directoryEntry;
    unsigned short numberOfSector;
    unsigned char mediaId; // F8: Fixed Media, F0: Removal Media
    unsigned short fatSize;
    unsigned short sectorPerTrack;
    unsigned short diskSides;
    unsigned short hiddenSector;
    int directoryPosition;
    int dataPosition;
} boot;

static struct FAT {
    unsigned char fatId;
    int entryCount;
    struct Entry {
        int clusterCount;
        unsigned short cluster[2048];
    } entries[1440];
} fat;

static struct Directory {
    int entryCount;
    struct Entry {
        bool removed;
        char displayName[16];
        char name[9];
        char ext[4];
        bool dirent;
        unsigned short cluster;
        unsigned int size;
        struct Attribute {
            bool dirent;
            bool volumeLabel;
            bool systemFile;
            bool hidden;
            bool readOnly;
        } attr;
        struct Date {
            int year;
            int month;
            int day;
            int hour;
            int minute;
            int second;
        } date;
    } entries[128];
} dir;

static bool isLittleEndian()
{
    unsigned short endianCheck = 0x1234;
    return ((char*)&endianCheck)[0] == 0x34;
}

#define BIT_CREATE 0b00000001
#define BIT_INFO   0b00000010
#define BIT_LS     0b00000100
#define BIT_CP     0b00001000

static void showUsage(int bit)
{
    puts("usage:");
    if (bit & BIT_CREATE) puts("- create .......... dskmgr image.dsk create");
    if (bit & BIT_INFO) puts("- information ..... dskmgr image.dsk info");
    if (bit & BIT_LS) puts("- list files ...... dskmgr image.dsk ls");
    if (bit & BIT_CP) puts("- copy to local ... dskmgr image.dsk cp filename");
}

static void extractDirectoryFromDisk()
{
    memset(&dir, 0, sizeof(dir));
    //unsigned char* ptr = diskImage[boot.fatPosition + boot.fatSize * boot.fatCopy];
    unsigned char* ptr = diskImage[boot.directoryPosition];
    while (*ptr) {
        if (0xE5 == *ptr) {
            dir.entries[dir.entryCount].removed = true;
            ptr += 32;
        } else {
            dir.entries[dir.entryCount].removed = false;
            memcpy(dir.entries[dir.entryCount].name, ptr, 8);
            ptr += 8;
            memcpy(dir.entries[dir.entryCount].ext, ptr, 3);
            ptr += 3;
            dir.entries[dir.entryCount].attr.dirent = (*ptr) & 0b00010000 ? true : false;
            dir.entries[dir.entryCount].attr.volumeLabel = (*ptr) & 0b00001000 ? true : false;
            dir.entries[dir.entryCount].attr.systemFile = (*ptr) & 0b00000100 ? true : false;
            dir.entries[dir.entryCount].attr.systemFile = (*ptr) & 0b00000100 ? true : false;
            dir.entries[dir.entryCount].attr.hidden = (*ptr) & 0b00000010 ? true : false;
            dir.entries[dir.entryCount].attr.readOnly = (*ptr) & 0b00000001 ? true : false;
            ptr += 11;
            dir.entries[dir.entryCount].date.minute = ((*ptr) & 0b11100000) >> 5;
            dir.entries[dir.entryCount].date.second = (*ptr) & 0b00011111;
            ptr++;
            dir.entries[dir.entryCount].date.hour = ((*ptr) & 0b11111000) >> 3;
            dir.entries[dir.entryCount].date.minute += ((*ptr) & 0b00000111) << 3;
            ptr++;
            dir.entries[dir.entryCount].date.month = ((*ptr) & 0b11100000) >> 5;
            dir.entries[dir.entryCount].date.day = (*ptr) & 0b00011111;
            ptr++;
            dir.entries[dir.entryCount].date.year = 1980 + (((*ptr) & 0b11111110) >> 1);
            dir.entries[dir.entryCount].date.month += ((*ptr) & 0b00000001) << 3;
            ptr++;
            memcpy(&dir.entries[dir.entryCount].cluster, ptr, 2);
            ptr += 2;
            memcpy(&dir.entries[dir.entryCount].size, ptr, 4);
            ptr += 4;
            strcpy(dir.entries[dir.entryCount].displayName, dir.entries[dir.entryCount].name);
            for (int i = strlen(dir.entries[dir.entryCount].displayName) - 1; 0 <= i; i--) {
                if (' ' == dir.entries[dir.entryCount].displayName[i]) {
                    dir.entries[dir.entryCount].displayName[i] = 0;
                } else break;
            }
            if (dir.entries[dir.entryCount].ext[0] != 0 && dir.entries[dir.entryCount].ext[1] != ' ') {
                strcat(dir.entries[dir.entryCount].displayName, ".");
                strcat(dir.entries[dir.entryCount].displayName, dir.entries[dir.entryCount].ext);
            }
        }
        dir.entryCount++;
    }
}

static void extractFatFromDisk()
{
    memset(&fat, 0, sizeof(fat));
    int fatSize = boot.fatSize * boot.secotrSize;
    unsigned char* ptr = diskImage[boot.fatPosition];
    fat.fatId = ptr[0];
    if (ptr[1] != 0xFF) return;
    if (ptr[2] != 0xFF) return;
    ptr += 3;
    fatSize -= 3;
    fat.entryCount = 1;
    unsigned short* data = (unsigned short*)malloc(fatSize / 3 * 2 * 2);
    if (!data) {
        puts("No memory");
        exit(-1);
    }
    int dataIndex = 0;
    while (0 < fatSize) {
        if (0 == (dataIndex & 1)) {
            unsigned short s = *ptr;
            data[dataIndex] = s;
            ptr++;
            fatSize--;
            s = *ptr;
            data[dataIndex] |= (s & 0x0F) << 8;
        } else {
            unsigned short s = *ptr;
            data[dataIndex] = (s & 0xF0) >> 4;
            ptr++;
            fatSize--;
            s = *ptr;
            data[dataIndex] |= s << 4;
            ptr++;
            fatSize--;
        }
        dataIndex++;
    }
    for (int i = 0; i < dataIndex; i++) {
        if (data[i] != 0x0FFF) {
            fat.entries[fat.entryCount].cluster[fat.entries[fat.entryCount].clusterCount++] = data[i];
        } else {
            fat.entryCount++;
        }
    }
    free(data);
}

static void extractBootSectorFromDisk()
{
    memset(&boot, 0, sizeof(boot));
    memcpy(&boot.bootJump, &diskImage[0][0x00], 3);
    memcpy(&boot.oemName, &diskImage[0][0x03], 8);
    memcpy(&boot.secotrSize, &diskImage[0][0xB], 2);
    memcpy(&boot.clusterSize, &diskImage[0][0xD], 1);
    memcpy(&boot.fatPosition, &diskImage[0][0xE], 2);
    memcpy(&boot.fatCopy, &diskImage[0][0x10], 1);
    memcpy(&boot.directoryEntry, &diskImage[0][0x11], 2);
    memcpy(&boot.numberOfSector, &diskImage[0][0x13], 2);
    memcpy(&boot.mediaId, &diskImage[0][0x15], 1);
    memcpy(&boot.fatSize, &diskImage[0][0x16], 2);
    memcpy(&boot.sectorPerTrack, &diskImage[0][0x18], 2);
    memcpy(&boot.diskSides, &diskImage[0][0x1A], 2);
    memcpy(&boot.hiddenSector, &diskImage[0][0x1C], 2);
    boot.directoryPosition = boot.fatPosition + boot.fatSize * boot.fatCopy;
    boot.dataPosition = boot.directoryPosition + 5;
}

static bool readDisk(const char* dsk)
{
    FILE* fp = fopen(dsk, "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size != sizeof(diskImage)) {
        puts("Unsupported file size (not 720KB)");
        fclose(fp);
        return false;
    }
    fseek(fp, 0, SEEK_SET);
    if (size != fread(diskImage, 1, size, fp)) {
        puts("I/O error");
        fclose(fp);
        return false;
    }
    fclose(fp);
    extractBootSectorFromDisk();
    extractFatFromDisk();
    extractDirectoryFromDisk();
    return true;
}

static int info(const char* dsk)
{
    if (!readDisk(dsk)) return 2;
    puts("[Boot Sector]");
    printf("            OEM: %s\n", boot.oemName);
    printf("       Media ID: 0x%02X\n", boot.mediaId);
    printf("    Sector Size: %d bytes\n", boot.secotrSize);
    printf("  Total Sectors: %d\n", boot.numberOfSector);
    printf("   Cluster Size: %d bytes (%d sectors)\n", boot.clusterSize * boot.secotrSize, boot.clusterSize);
    printf("   FAT Position: %d\n", boot.fatPosition);
    printf("       FAT Size: %d bytes (%d sectors)\n", boot.fatSize * boot.secotrSize, boot.fatSize);
    printf("       FAT Copy: %d\n", boot.fatCopy);
    printf("Creatable Files: %d\n", boot.directoryEntry);
    printf("        Sectors: %d per track\n", boot.sectorPerTrack);
    printf("     Disk Sides: %d\n", boot.diskSides);
    printf(" Hidden Sectors: %d\n", boot.hiddenSector);
    puts("\n[FAT]");
    printf("Fat ID: 0x%02X\n", fat.fatId);
    int availableEntries = 0;
    for (int i = 0; i < fat.entryCount; i++) {
        if (0 < fat.entries[i].clusterCount) {
            printf("- Entry#%d = %d bytes (%d cluster) ... %d: ", i, boot.clusterSize * boot.secotrSize * fat.entries[i].clusterCount, fat.entries[i].clusterCount, fat.entries[i].cluster[0]);
            bool found = false;
            for (int j = 0; j < dir.entryCount; j++) {
                for (int jj = 0; jj < fat.entries[i].clusterCount; jj++) {
                    if (dir.entries[j].cluster == fat.entries[i].cluster[jj]) {
                        puts(dir.entries[j].displayName);
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
            if (!found) {
                puts("???");
            }
            availableEntries++;
        }
    }
    printf("Available Entries: %d/%d\n", availableEntries, fat.entryCount);
    return 0;
}

static int ls(const char* dsk)
{
    if (!readDisk(dsk)) return 2;
    for (int i = 0; i < dir.entryCount; i++) {
        if (dir.entries[i].removed) continue;
        printf("%c%c%c%c%c  %-12s  %8u bytes  %4d.%02d.%02d %02d:%02d:%02d  (C:%d, S:%d)\n"
            , dir.entries[i].attr.dirent ? 'd' : '-'
            , dir.entries[i].attr.volumeLabel ? 'v' : '-'
            , dir.entries[i].attr.systemFile ? 's' : '-'
            , dir.entries[i].attr.hidden ? 'h' : '-'
            , dir.entries[i].attr.readOnly ? '-' : 'w'
            , dir.entries[i].displayName
            , dir.entries[i].size
            , dir.entries[i].date.year
            , dir.entries[i].date.month
            , dir.entries[i].date.day
            , dir.entries[i].date.hour
            , dir.entries[i].date.minute
            , dir.entries[i].date.second
            , dir.entries[i].cluster
            , boot.dataPosition + (dir.entries[i].cluster - 2) * boot.clusterSize
            );
    }
    return 0;
}

static void wr(FILE* fp, int di)
{
    int size = dir.entries[di].size;
    int bp = boot.dataPosition;
    // FATエントリを検索
    for (int i = 0; i < fat.entryCount; i++) {
        if (0 < fat.entries[i].clusterCount) {
            for (int ii = 0; ii < fat.entries[i].clusterCount; ii++) {
                if (fat.entries[i].cluster[ii] == dir.entries[di].cluster) {
                    for (int j = ii; j < fat.entries[i].clusterCount; j++) {
                        for (int k = 0; k < boot.clusterSize; k++) {
                            int s = bp + (fat.entries[i].cluster[j] - 1) * boot.clusterSize + k;
                            int n = size < boot.secotrSize ? size : boot.secotrSize;
                            fwrite(diskImage[s], 1, n, fp);
                            size -= n;
                            if (size < 1) {
                                return;
                            }
                        }
                    }
                    return;
                }
            }
        }
    }
    // FATテーブルが見つからないのでディレクトリエントリの先頭クラスタからシーケンシャルに読み出す
    // ※MSXのディスクのFATは概ね壊れているのでこのケースに読み出しが行われる
    bp += (dir.entries[di].cluster - 1) * boot.clusterSize;
    while (0 < size && bp < boot.numberOfSector) {
        int n = size < boot.secotrSize ? size : boot.secotrSize;
        fwrite(diskImage[bp++], 1, n, fp);
        size -= n;
    }
}

static int cp(const char* dsk, char* displayName)
{
    char localFileName[16];
    char name[9];
    char ext[4];
    if (sizeof(localFileName) <= strlen(displayName)) {
        puts("File not found (invalid length)");
        return 4;
    }
    strcpy(localFileName, displayName);
    char* cp = strchr(displayName, '.');
    if (cp) {
        *cp = 0;
        cp++;
        if (3 < strlen(cp)) {
            puts("File not found (invalid ext length)");
            return 4;
        }
        strcpy(ext, cp);
    } else {
        strcpy(ext, "   ");
    }
    if (8 < strlen(displayName)) {
        puts("File not found (invalid name length)");
        return 4;
    }
    strncpy(name, displayName, 8);
    name[8] = 0;
    ext[3] = 0;
    int i;
    for (i = 0; ext[i]; i++) ext[i] = toupper(ext[i]);
    for (; i < 3; i++) ext[i] = ' ';
    for (i = 0; name[i]; i++) name[i] = toupper(name[i]);
    for (; i < 8; i++) name[i] = ' ';
    if (!readDisk(dsk)) return 2;
    for (int i = 0; i < dir.entryCount; i++) {
        if (dir.entries[i].removed) continue;
        if (strcmp(dir.entries[i].name, name) == 0) {
            if (strcmp(dir.entries[i].ext, ext) == 0) {
                FILE* fp = fopen(localFileName, "wb");
                wr(fp, i);
                fclose(fp);
                return 0;
            }
        }
    }
    puts("File not found");
    return 4;
}

int main(int argc, char* argv[])
{
    if (!isLittleEndian()) {
        puts("Sorry, this program is executable only little-endian environment.");
        return 255;
    }
    if (argc < 3) {
        showUsage(0xFF);
        return 1;
    }
    if (0 == strcmp(argv[2], "info")) {
        if (argc != 3) {
            showUsage(BIT_INFO);
            return 1;
        }
        return info(argv[1]);
    } else if (0 == strcmp(argv[2], "ls")) {
        if (argc != 3) {
            showUsage(BIT_LS);
            return 1;
        }
        return ls(argv[1]);
    } else if (0 == strcmp(argv[2], "cp")) {
        if (argc != 4) {
            showUsage(BIT_CP);
            return 1;
        }
        return cp(argv[1], argv[3]);
    } else {
        showUsage(0xFF);
        return 1;
    }
    return 0;
}
