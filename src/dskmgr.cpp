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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 112

int bas2txt(FILE* stream, unsigned char* cBuf); // bas2txt.cpp
static unsigned char diskImage[1440][512];

// NOTE: Boundary-unaware data structure to be expanded at read time
static struct BootSector {
    unsigned char bootJump[3];
    unsigned char oemName[9];
    unsigned short sectorSize;
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
            unsigned char raw;
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

static struct CreateFileInfo {
    int entryCount;
    struct Entry {
        const char* originalPath;
        char name[8];
        char ext[3];
        unsigned int size;
        void* data;
        int sectorSize;
        int clusterSize;
        unsigned short clusterStart;
    } entries[MAX_FILES];
    int totalCluster;
    int totalSector;
    int totalSize;
} cfi;

static bool isLittleEndian()
{
    unsigned short endianCheck = 0x1234;
    return ((char*)&endianCheck)[0] == 0x34;
}

#define BIT_CREATE 0b00000001
#define BIT_INFO 0b00000010
#define BIT_LS 0b00000100
#define BIT_CP 0b00001000
#define BIT_CAT 0b00010000

static void showUsage(int bit)
{
    puts("usage:");
    if (bit & BIT_CREATE) puts("- create .......... dskmgr image.dsk create [files]");
    if (bit & BIT_INFO) puts("- information ..... dskmgr image.dsk info");
    if (bit & BIT_LS) puts("- list files ...... dskmgr image.dsk ls");
    if (bit & BIT_CP) puts("- copy to local ... dskmgr image.dsk cp filename");
    if (bit & BIT_CAT) puts("- stdout file  .... dskmgr image.dsk cat filename");
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
            dir.entries[dir.entryCount].attr.raw = *ptr;
            dir.entries[dir.entryCount].attr.dirent = (*ptr) & 0b00010000 ? true : false;
            dir.entries[dir.entryCount].attr.volumeLabel = (*ptr) & 0b00001000 ? true : false;
            dir.entries[dir.entryCount].attr.systemFile = (*ptr) & 0b00000100 ? true : false;
            dir.entries[dir.entryCount].attr.systemFile = (*ptr) & 0b00000100 ? true : false;
            dir.entries[dir.entryCount].attr.hidden = (*ptr) & 0b00000010 ? true : false;
            dir.entries[dir.entryCount].attr.readOnly = (*ptr) & 0b00000001 ? true : false;
            ptr += 11;
            dir.entries[dir.entryCount].date.minute = ((*ptr) & 0b11100000) >> 5;
            dir.entries[dir.entryCount].date.second = ((*ptr) & 0b00011111) << 1;
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
                } else
                    break;
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
    int fatSize = boot.fatSize * boot.sectorSize;
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
    memcpy(&boot.sectorSize, &diskImage[0][0xB], 2);
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

static void extractBootSectorToDisk()
{
    unsigned char prg[] = {
        0xD0, 0xED, 0x53, 0x59, 0xC0, 0x32, 0xD0, 0xC0, 0x36, 0x56, 0x23, 0x36, 0xC0, 0x31, 0x1F, 0xF5,
        0x11, 0xAB, 0xC0, 0x0E, 0x0F, 0xCD, 0x7D, 0xF3, 0x3C, 0xCA, 0x63, 0xC0, 0x11, 0x00, 0x01, 0x0E,
        0x1A, 0xCD, 0x7D, 0xF3, 0x21, 0x01, 0x00, 0x22, 0xB9, 0xC0, 0x21, 0x00, 0x3F, 0x11, 0xAB, 0xC0,
        0x0E, 0x27, 0xCD, 0x7D, 0xF3, 0xC3, 0x00, 0x01, 0x58, 0xC0, 0xCD, 0x00, 0x00, 0x79, 0xE6, 0xFE,
        0xFE, 0x02, 0xC2, 0x6A, 0xC0, 0x3A, 0xD0, 0xC0, 0xA7, 0xCA, 0x22, 0x40, 0x11, 0x85, 0xC0, 0xCD,
        0x77, 0xC0, 0x0E, 0x07, 0xCD, 0x7D, 0xF3, 0x18, 0xB4, 0x1A, 0xB7, 0xC8, 0xD5, 0x5F, 0x0E, 0x06,
        0xCD, 0x7D, 0xF3, 0xD1, 0x13, 0x18, 0xF2, 0x42, 0x6F, 0x6F, 0x74, 0x20, 0x65, 0x72, 0x72, 0x6F,
        0x72, 0x0D, 0x0A, 0x50, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x6B, 0x65, 0x79,
        0x20, 0x66, 0x6F, 0x72, 0x20, 0x72, 0x65, 0x74, 0x72, 0x79, 0x0D, 0x0A, 0x00, 0x00, 0x4D, 0x53,
        0x58, 0x44, 0x4F, 0x53, 0x20, 0x20, 0x53, 0x59, 0x53, 0x00};
    memcpy(&diskImage[0][0x00], &boot.bootJump, 3);
    memcpy(&diskImage[0][0x03], &boot.oemName, 8);
    memcpy(&diskImage[0][0xB], &boot.sectorSize, 2);
    memcpy(&diskImage[0][0xD], &boot.clusterSize, 1);
    memcpy(&diskImage[0][0xE], &boot.fatPosition, 2);
    memcpy(&diskImage[0][0x10], &boot.fatCopy, 1);
    memcpy(&diskImage[0][0x11], &boot.directoryEntry, 2);
    memcpy(&diskImage[0][0x13], &boot.numberOfSector, 2);
    memcpy(&diskImage[0][0x15], &boot.mediaId, 1);
    memcpy(&diskImage[0][0x16], &boot.fatSize, 2);
    memcpy(&diskImage[0][0x18], &boot.sectorPerTrack, 2);
    memcpy(&diskImage[0][0x1A], &boot.diskSides, 2);
    memcpy(&diskImage[0][0x1C], &boot.hiddenSector, 2);
    memcpy(&diskImage[0][0x1E], prg, sizeof(prg));
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
    printf("    Sector Size: %d bytes\n", boot.sectorSize);
    printf("  Total Sectors: %d\n", boot.numberOfSector);
    printf("   Cluster Size: %d bytes (%d sectors)\n", boot.clusterSize * boot.sectorSize, boot.clusterSize);
    printf("   FAT Position: %d\n", boot.fatPosition);
    printf("       FAT Size: %d bytes (%d sectors)\n", boot.fatSize * boot.sectorSize, boot.fatSize);
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
            printf("- Entry#%d = %d bytes (%d cluster) ... %d: ", i, boot.clusterSize * boot.sectorSize * fat.entries[i].clusterCount, fat.entries[i].clusterCount, fat.entries[i].cluster[0]);
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
        printf("%02X:%c%c%c%c%c  %-12s  %8u bytes  %4d.%02d.%02d %02d:%02d:%02d  (C:%d, S:%d)\n", dir.entries[i].attr.raw, dir.entries[i].attr.dirent ? 'd' : '-', dir.entries[i].attr.volumeLabel ? 'v' : '-', dir.entries[i].attr.systemFile ? 's' : '-', dir.entries[i].attr.hidden ? 'h' : '-', dir.entries[i].attr.readOnly ? '-' : 'w', dir.entries[i].displayName, dir.entries[i].size, dir.entries[i].date.year, dir.entries[i].date.month, dir.entries[i].date.day, dir.entries[i].date.hour, dir.entries[i].date.minute, dir.entries[i].date.second, dir.entries[i].cluster, boot.dataPosition + (dir.entries[i].cluster - 2) * boot.clusterSize);
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
                            int n = size < boot.sectorSize ? size : boot.sectorSize;
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
        int n = size < boot.sectorSize ? size : boot.sectorSize;
        fwrite(diskImage[bp++], 1, n, fp);
        size -= n;
    }
}

static void wm(unsigned char* buf, int di)
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
                            int n = size < boot.sectorSize ? size : boot.sectorSize;
                            memcpy(buf, diskImage[s], n);
                            buf += n;
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
        int n = size < boot.sectorSize ? size : boot.sectorSize;
        memcpy(buf, diskImage[bp++], n);
        size -= n;
    }
}

static int parseDisplayName(char* displayName, char* localFileName, char* name, char* ext)
{
    if (16 <= strlen(displayName)) {
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
    return 0;
}

static int cp(const char* dsk, char* displayName)
{
    char localFileName[16];
    char name[9];
    char ext[4];
    int parseError = parseDisplayName(displayName, localFileName, name, ext);
    if (parseError) return parseError;
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

static int cat(const char* dsk, char* displayName)
{
    char localFileName[16];
    char name[9];
    char ext[4];
    int parseError = parseDisplayName(displayName, localFileName, name, ext);
    if (parseError) return parseError;
    if (!readDisk(dsk)) return 2;
    for (int i = 0; i < dir.entryCount; i++) {
        if (dir.entries[i].removed) continue;
        if (strcmp(dir.entries[i].name, name) == 0) {
            if (strcmp(dir.entries[i].ext, ext) == 0) {
                if (0 == strncmp(ext, "BAS", 3)) {
                    unsigned char* buf = (unsigned char*)malloc(dir.entries[i].size);
                    wm(buf, i);
                    bas2txt(stdout, buf);
                    free(buf);
                } else {
                    wr(stdout, i);
                }
                return 0;
            }
        }
    }
    puts("File not found");
    return 4;
}

static bool addCreateFileInfo(const char* path)
{
    if (MAX_FILES <= cfi.entryCount) {
        puts("Disk Full");
        return false;
    }
    int idx = cfi.entryCount;
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        printf("File not found: %s\n", path);
        return false;
    }
    fseek(fp, 0, SEEK_END);
    cfi.entries[idx].size = (int)ftell(fp);
    if (cfi.entries[idx].size < 1) {
        puts("I/O error");
        fclose(fp);
        return false;
    }
    cfi.entries[idx].sectorSize = cfi.entries[idx].size / 512;
    cfi.entries[idx].sectorSize += cfi.entries[idx].size % 512 != 0 ? 1 : 0;
    cfi.entries[idx].clusterSize = cfi.entries[idx].sectorSize / 2;
    cfi.entries[idx].clusterSize += cfi.entries[idx].sectorSize % 2;
    cfi.totalSize += cfi.entries[idx].size;
    cfi.totalSector += cfi.entries[idx].sectorSize;
    cfi.totalCluster += cfi.entries[idx].clusterSize;
    if ((1440 - 1 - 3 * 2 - 5) / 2 <= cfi.totalCluster) {
        puts("Disk Full");
        fclose(fp);
        return false;
    }
    fseek(fp, 0, SEEK_SET);
    cfi.entries[idx].data = malloc(cfi.entries[idx].size);
    if (!cfi.entries[idx].data) {
        puts("No memory");
        fclose(fp);
        return false;
    }
    if (cfi.entries[idx].size != fread(cfi.entries[idx].data, 1, cfi.entries[idx].size, fp)) {
        puts("I/O error");
        fclose(fp);
        free(cfi.entries[idx].data);
        cfi.entries[idx].data = NULL;
        return false;
    }
    fclose(fp);
    cfi.entries[idx].originalPath = path;
    const char* name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char* ext = strchr(name, '.');
    int nameLen = ext ? (int)(ext - name) : (int)strlen(name);
    int extLen = ext ? strlen(ext + 1) : 0;
    ext = ext ? ext + 1 : 0;
    memset(cfi.entries[idx].name, 0x20, 8);
    memcpy(cfi.entries[idx].name, name, nameLen < 8 ? nameLen : 8);
    for (int i = 0; i < 8; i++) {
        cfi.entries[idx].name[i] = toupper(cfi.entries[idx].name[i]);
    }
    memset(cfi.entries[cfi.entryCount].ext, 0x20, 3);
    if (ext) {
        memcpy(cfi.entries[cfi.entryCount].ext, ext, extLen < 3 ? extLen : 3);
    }
    for (int i = 0; i < 3; i++) cfi.entries[idx].ext[i] = toupper(cfi.entries[idx].ext[i]);
    cfi.entryCount++;
    return true;
}

static void releaseCFI()
{
    for (int i = 0; i < MAX_FILES; i++) {
        if (cfi.entries[i].data) {
            free(cfi.entries[i].data);
        }
    }
}

static int create(const char* dskPath)
{
    // Create Boot Sector
    unsigned char bootJump[3] = {0xEB, 0xFE, 0x90};
    boot.bootJump[0] = 0xEB;
    boot.bootJump[1] = 0xFE;
    boot.bootJump[2] = 0x90;
    memcpy(boot.oemName, "SZKPLN01", 8);
    boot.sectorSize = 512;
    boot.clusterSize = 2;
    boot.fatPosition = 1;
    boot.fatCopy = 2;
    boot.directoryEntry = MAX_FILES;
    boot.numberOfSector = 1440;
    boot.mediaId = 0xF9;
    boot.fatSize = 3;
    boot.sectorPerTrack = 9;
    boot.diskSides = 2;
    boot.hiddenSector = 0;
    extractBootSectorToDisk();

    // Create FAT
    unsigned char* f = &diskImage[boot.fatPosition][0];
    *f = 0xF9;
    f++;
    *f = 0xFF;
    f++;
    *f = 0xFF;
    f++;
    int c = 2;
    int fs = 0;
    for (int i = 0; i < cfi.entryCount; i++) {
        cfi.entries[i].clusterStart = (unsigned short)c;
        for (int ii = 0; ii < cfi.entries[i].clusterSize; ii++) {
            if (0 == fs) {
                *f = (unsigned char)(c & 0xFF);
                f++;
                *f = ((unsigned char)(c & 0xF00)) >> 8;
            } else {
                (*f) |= ((unsigned char)(c & 0x00F)) << 4;
                f++;
                *f = (unsigned char)((c & 0xFF0) >> 4);
                f++;
            }
            c++;
            fs = 1 - fs;
        }
        // write FFF
        if (0 == fs) {
            *f = 0xFF;
            f++;
            *f = 0x0F;
        } else {
            *f |= 0xF0;
            f++;
            *f = 0xFF;
            f++;
        }
        fs = 1 - fs;
    }

    // Copy FAT
    for (int i = 1; i < boot.fatCopy; i++) {
        memcpy(diskImage[boot.fatPosition + boot.fatSize * i], diskImage[boot.fatPosition], boot.fatSize * boot.sectorSize);
    }

    // Create Directory & File Content
    unsigned char* d = &diskImage[boot.directoryPosition][0];
    for (int i = 0; i < cfi.entryCount; i++) {
        // Directory
        memcpy(d, cfi.entries[i].name, 8);
        d += 8;
        memcpy(d, cfi.entries[i].ext, 3);
        d += 3;
        *d = 0;
        d += 11;
        d += 2; // hour/min/sec
        d += 2; // year/month/day
        memcpy(d, &cfi.entries[i].clusterStart, 2);
        d += 2;
        memcpy(d, &cfi.entries[i].size, 4);
        d += 4;
        // File Content
        memcpy(diskImage[boot.dataPosition + (cfi.entries[i].clusterStart - 1) * boot.clusterSize], cfi.entries[i].data, cfi.entries[i].size);
    }

    // Write Disk Image
    FILE* fp = fopen(dskPath, "wb");
    if (NULL == fp) {
        puts("I/O error");
        releaseCFI();
        return 6;
    }
    if (sizeof(diskImage) != fwrite(diskImage, 1, sizeof(diskImage), fp)) {
        puts("I/O error");
        releaseCFI();
        fclose(fp);
        return 6;
    }
    fclose(fp);
    releaseCFI();
    return 0;
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
    } else if (0 == strcmp(argv[2], "ls") || 0 == strcmp(argv[2], "dir")) {
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
    } else if (0 == strcmp(argv[2], "cat")) {
        if (argc != 4) {
            showUsage(BIT_CAT);
            return 1;
        }
        return cat(argv[1], argv[3]);
    } else if (0 == strcmp(argv[2], "create")) {
        if (argc < 3) {
            showUsage(BIT_CREATE);
            return 1;
        }
        memset(&cfi, 0, sizeof(cfi));
        for (int i = 3; i < argc; i++) {
            if (!addCreateFileInfo(argv[i])) {
                return 5;
            }
        }
        return create(argv[1]);
    } else {
        showUsage(0xFF);
        return 1;
    }
    return 0;
}
