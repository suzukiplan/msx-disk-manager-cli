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
#include "basic.hpp"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_FILES 112

static BasicFilter bf;
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
    unsigned char bootJump2[2];
    unsigned char idLabel[7];
    unsigned char dirtyFlag;
    unsigned char idValue[4];
    unsigned char reserved[5];
    unsigned char bootProgram[0x1D0];
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
        unsigned char dateRaw[4];
    } entries[128];
} dir;

static struct CreateFileInfo {
    int entryCount;
    struct Entry {
        char name[8];
        char ext[3];
        unsigned char date[4];
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
#define BIT_WR 0b00010000
#define BIT_CAT 0b00100000
#define BIT_RM 0b01000000

static void showUsage(int bit)
{
    puts("usage:");
    if (bit & BIT_CREATE) puts("- create .......... dskmgr image.dsk create [files]");
    if (bit & BIT_INFO) puts("- information ..... dskmgr image.dsk info");
    if (bit & BIT_LS) puts("- list files ...... dskmgr image.dsk ls");
    if (bit & BIT_CP) puts("- copy to local ... dskmgr image.dsk get filename [as filename2]");
    if (bit & BIT_WR) puts("- copy to disk .... dskmgr image.dsk put filename [as filename2]");
    if (bit & BIT_CAT) puts("- stdout file  .... dskmgr image.dsk cat filename");
    if (bit & BIT_RM) puts("- remove file  .... dskmgr image.dsk rm filename");
}

static const unsigned char* now()
{
    static unsigned char buf[4] = {0, 0, 0, 0};
    if (0 == buf[0] && 0 == buf[1] && 0 == buf[2] && 0 == buf[3]) {
        time_t t1 = time(nullptr);
        struct tm* t2 = localtime(&t1);
        buf[0] = (t2->tm_min & 0b00000111) << 5;
        buf[0] |= (t2->tm_sec & 0b00111110) >> 1;
        buf[1] = (t2->tm_hour & 0b00011111) << 3;
        buf[1] |= (t2->tm_min & 0b00111000) >> 3;
        buf[2] = ((t2->tm_mon + 1) & 0b00000111) << 5;
        buf[2] |= (t2->tm_mday) & 0b00011111;
        buf[3] = ((t2->tm_year - 80) & 0b01111111) << 1;
        buf[3] |= ((t2->tm_mon + 1) & 0b00001000) >> 3;
    }
    return buf;
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
            memcpy(dir.entries[dir.entryCount].dateRaw, ptr, 4);
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
    memcpy(&boot.bootJump2, &diskImage[0][0x1E], 2);
    memcpy(&boot.idLabel, &diskImage[0][0x20], 6);
    memcpy(&boot.dirtyFlag, &diskImage[0][0x26], 1);
    memcpy(&boot.idValue, &diskImage[0][0x27], 4);
    memcpy(&boot.reserved, &diskImage[0][0x2B], 5);
    memcpy(&boot.bootProgram, &diskImage[0][0x30], sizeof(boot.bootProgram));
    boot.directoryPosition = boot.fatPosition + boot.fatSize * boot.fatCopy;
    boot.dataPosition = boot.directoryPosition + 5;
}

static void extractBootSectorToDisk()
{
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
    memcpy(&diskImage[0][0x1E], &boot.bootJump2, 2);
    memcpy(&diskImage[0][0x20], &boot.idLabel, 6);
    memcpy(&diskImage[0][0x26], &boot.dirtyFlag, 1);
    memcpy(&diskImage[0][0x27], &boot.idValue, 4);
    memcpy(&diskImage[0][0x2B], &boot.reserved, 5);
    memcpy(&diskImage[0][0x30], &boot.bootProgram, 0x1D0);
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
    if (0 == memcmp(boot.idLabel, "VOL_ID", 6)) {
        printf("      Volume ID: %02X,%02X,%02X,%02X\n", boot.idValue[0], boot.idValue[1], boot.idValue[2], boot.idValue[3]);
    }
    printf("     Dirty Flag: %02X\n", boot.dirtyFlag);
    puts("\n[FAT]");
    printf("Fat ID: 0x%02X\n", fat.fatId);
    int usingCluster = 1;
    for (int i = 0; i < dir.entryCount; i++) {
        if (!dir.entries[i].removed) {
            printf("- dirent#%d (%s) ... %d", i, dir.entries[i].displayName, dir.entries[i].cluster);
            usingCluster++;
            for (int j = 0; j < fat.entries[i + 1].clusterCount; j++) {
                printf(",%d", fat.entries[i + 1].cluster[j]);
                usingCluster++;
            }
            printf("\n");
        }
    }
    printf("Total using cluster: %d (%d bytes)\n", usingCluster, usingCluster * boot.clusterSize * boot.sectorSize);
    return 0;
}

static int ls(const char* dsk)
{
    if (!readDisk(dsk)) return 2;
    int totalSize = 0;
    int totalCluster = 0;
    int fileCount = 0;
    int cs = boot.sectorSize * boot.clusterSize;
    for (int i = 0; i < dir.entryCount; i++) {
        if (dir.entries[i].removed) continue;
        printf("%02X:%c%c%c%c%c  %-12s  %8u bytes  %4d.%02d.%02d %02d:%02d:%02d  (C:%d, S:%d)\n", dir.entries[i].attr.raw, dir.entries[i].attr.dirent ? 'd' : '-', dir.entries[i].attr.volumeLabel ? 'v' : '-', dir.entries[i].attr.systemFile ? 's' : '-', dir.entries[i].attr.hidden ? 'h' : '-', dir.entries[i].attr.readOnly ? '-' : 'w', dir.entries[i].displayName, dir.entries[i].size, dir.entries[i].date.year, dir.entries[i].date.month, dir.entries[i].date.day, dir.entries[i].date.hour, dir.entries[i].date.minute, dir.entries[i].date.second, dir.entries[i].cluster, boot.dataPosition + (dir.entries[i].cluster - 1) * boot.clusterSize);
        totalSize += dir.entries[i].size;
        totalCluster += dir.entries[i].size / cs + (dir.entries[i].size % cs ? 1 : 0);
        fileCount++;
    }
    if (0 < fileCount) {
        int freeCluster = ((boot.numberOfSector - (boot.dataPosition + 2)) / boot.clusterSize) - totalCluster;
        printf("Total Size: %7d bytes\n", totalSize);
        printf(" Free Size: %7d bytes (%d clusters)\n", cs * freeCluster, freeCluster);
    }
    return 0;
}

static void wm(FILE* fp, unsigned char* buf, int di)
{
    int size = dir.entries[di].size;
    // 最初のクラスタをディレクトリエントリから出力
    for (int i = 0; 0 < size && i < boot.clusterSize; i++) {
        int sector = boot.dataPosition;
        sector += (dir.entries[di].cluster - 1) * boot.clusterSize;
        sector += i;
        int n = size < boot.sectorSize ? size : boot.sectorSize;
        if (fp) {
            fwrite(diskImage[sector], 1, n, fp);
        }
        if (buf) {
            memcpy(buf, diskImage[sector], n);
            buf += n;
        }
        size -= n;
    }
    // 2番目以降のクラスとをFATから出力
    for (int ii = 0; 0 < size && ii < fat.entries[di + 1].clusterCount; ii++) {
        for (int i = 0; 0 < size && i < boot.clusterSize; i++) {
            int sector = boot.dataPosition;
            sector += (fat.entries[di + 1].cluster[ii] - 1) * boot.clusterSize;
            sector += i;
            int n = size < boot.sectorSize ? size : boot.sectorSize;
            if (fp) {
                fwrite(diskImage[sector], 1, n, fp);
            }
            if (buf) {
                memcpy(buf, diskImage[sector], n);
                buf += n;
            }
            size -= n;
        }
    }
}

static int parseDisplayName(char* displayName, char* name, char* ext)
{
    if (16 <= strlen(displayName)) {
        puts("File not found (invalid length)");
        return 4;
    }
    char* cp = strchr(displayName, '.');
    if (cp) {
        *cp = 0;
        cp++;
        if (3 < strlen(cp)) {
            puts("File not found (invalid ext length)");
            return 4;
        }
        strcpy(ext, cp);
        cp--;
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
    if (cp) *cp = '.';
    return 0;
}

static int get(const char* dsk, char* displayName, const char* getAs)
{
    char localFileName[16];
    char name[9];
    char ext[4];
    int parseError = parseDisplayName(displayName, name, ext);
    strcpy(localFileName, displayName);
    if (parseError) return parseError;
    if (!readDisk(dsk)) return 2;
    for (int i = 0; i < dir.entryCount; i++) {
        if (dir.entries[i].removed) continue;
        if (strcmp(dir.entries[i].name, name) == 0) {
            if (strcmp(dir.entries[i].ext, ext) == 0) {
                FILE* fp = fopen(getAs ? getAs : localFileName, "wb");
                wm(fp, nullptr, i);
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
    char name[9];
    char ext[4];
    int parseError = parseDisplayName(displayName, name, ext);
    if (parseError) return parseError;
    if (!readDisk(dsk)) return 2;
    for (int i = 0; i < dir.entryCount; i++) {
        if (dir.entries[i].removed) continue;
        if (strcmp(dir.entries[i].name, name) == 0) {
            if (strcmp(dir.entries[i].ext, ext) == 0) {
                if (0 == strncmp(ext, "BAS", 3)) {
                    unsigned char* buf = (unsigned char*)malloc(dir.entries[i].size);
                    wm(nullptr, buf, i);
                    bf.bas2txt(stdout, buf);
                    free(buf);
                } else {
                    wm(stdout, nullptr, i);
                }
                return 0;
            }
        }
    }
    puts("File not found");
    return 4;
}

static bool setCreateFileInfo(int idx, const char* name, int nameLen, const char* ext, int extLen, unsigned char* data, size_t dataSize)
{
    cfi.entries[idx].size = dataSize;
    cfi.entries[idx].sectorSize = cfi.entries[idx].size / 512;
    cfi.entries[idx].sectorSize += cfi.entries[idx].size % 512 != 0 ? 1 : 0;
    cfi.entries[idx].clusterSize = cfi.entries[idx].sectorSize / 2;
    cfi.entries[idx].clusterSize += cfi.entries[idx].sectorSize % 2;
    cfi.totalSize += cfi.entries[idx].size;
    cfi.totalSector += cfi.entries[idx].sectorSize;
    cfi.totalCluster += cfi.entries[idx].clusterSize;
    if ((1440 - 1 - 3 * 2 - 5) / 2 <= cfi.totalCluster) {
        puts("Disk Full");
        return false;
    }
    cfi.entries[idx].data = data;
    memset(cfi.entries[idx].name, 0x20, 8);
    memcpy(cfi.entries[idx].name, name, nameLen < 8 ? nameLen : 8);
    for (int i = 0; i < 8; i++) cfi.entries[idx].name[i] = toupper(cfi.entries[idx].name[i]);
    memset(cfi.entries[idx].ext, 0x20, 3);
    if (ext) {
        memcpy(cfi.entries[idx].ext, ext, extLen < 3 ? extLen : 3);
        for (int i = 0; i < 3; i++) cfi.entries[idx].ext[i] = toupper(cfi.entries[idx].ext[i]);
    }
    return true;
}

static bool addCreateFileInfo(const char* path, const char* putAs = nullptr)
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
    fseek(fp, 0, SEEK_SET);
    unsigned char* bin = (unsigned char*)malloc(cfi.entries[idx].size + 1);
    if (!bin) {
        puts("No memory");
        fclose(fp);
        return false;
    }
    bin[cfi.entries[idx].size] = 0;
    if (cfi.entries[idx].size != fread(bin, 1, cfi.entries[idx].size, fp)) {
        puts("I/O error");
        fclose(fp);
        return false;
    }
    fclose(fp);
    size_t basSize = 0;
    unsigned char* bas = nullptr;
    const char* name = strrchr(path, '/');
    if (!name) name = strrchr(path, '\\');
    name = name ? name + 1 : path;
    const char* ext = strchr(name, '.');
    int nameLen = ext ? (int)(ext - name) : (int)strlen(name);
    int extLen = ext ? strlen(ext + 1) : 0;
    ext = ext ? ext + 1 : 0;
    if (3 == extLen && 0 == strncasecmp(ext, "BAS", 3)) {
        bas = bf.txt2bas((char*)bin, &basSize);
    }
    if (bas) {
        printf("%s: Convert to MSX-BASIC intermediate code ... %d -> %lu bytes", path, cfi.entries[idx].size, basSize);
        cfi.entries[idx].size = (int)basSize;
        free(bin);
        bin = bas;
    } else {
        printf("%s: Write to disk as a binary file ... %d bytes", path, cfi.entries[idx].size);
    }
    if (putAs) {
        printf(" as %s\n", putAs);
        name = putAs;
        ext = strchr(name, '.');
        nameLen = ext ? (int)(ext - name) : (int)strlen(name);
        extLen = ext ? strlen(ext + 1) : 0;
        ext = ext ? ext + 1 : 0;
    } else {
        printf("\n");
    }
    memcpy(cfi.entries[cfi.entryCount].date, now(), 4);
    if (!setCreateFileInfo(idx, name, nameLen, ext, extLen, bas ? bas : bin, cfi.entries[idx].size)) {
        return false;
    }
    cfi.entryCount++;
    return true;
}

static int create(const char* dskPath)
{
    // Create Boot Sector
    unsigned char bootJump[3] = {0xEB, 0xFE, 0x90};
    unsigned char bootJump2[2] = {0xD0, 0xED};
    unsigned char dos1[0x1D0] = {
        0xd0,                   // ret     nc                              ;[0030] d0
        0xed, 0x53, 0x6a, 0xc0, // ld      ($c06a),de                      ;[0031] ed 53 6a c0
        0x32, 0x72, 0xc0,       // ld      ($c072),a                       ;[0035] 32 72 c0
        0x36, 0x67,             // ld      (hl),$67                        ;[0038] 36 67
        0x23,                   // inc     hl                              ;[003a] 23
        0x36, 0xc0,             // ld      (hl),$c0                        ;[003b] 36 c0
        0x31, 0x1f, 0xf5,       // ld      sp,$f51f                        ;[003d] 31 1f f5
        0x11, 0xab, 0xc0,       // ld      de,$c0ab                        ;[0040] 11 ab c0
        0x0e, 0x0f,             // ld      c,$0f                           ;[0043] 0e 0f
        0xcd, 0x7d, 0xf3,       // call    $f37d                           ;[0045] cd 7d f3
        0x3c,                   // inc     a                               ;[0048] 3c
        0x28, 0x26,             // jr      z,$0071                         ;[0049] 28 26
        0x11, 0x00, 0x01,       // ld      de,$0100                        ;[004b] 11 00 01
        0x0e, 0x1a,             // ld      c,$1a                           ;[004e] 0e 1a
        0xcd, 0x7d, 0xf3,       // call    $f37d                           ;[0050] cd 7d f3
        0x21, 0x01, 0x00,       // ld      hl,$0001                        ;[0053] 21 01 00
        0x22, 0xb9, 0xc0,       // ld      ($c0b9),hl                      ;[0056] 22 b9 c0
        0x21, 0x00, 0x3f,       // ld      hl,$3f00                        ;[0059] 21 00 3f
        0x11, 0xab, 0xc0,       // ld      de,$c0ab                        ;[005c] 11 ab c0
        0x0e, 0x27,             // ld      c,$27                           ;[005f] 0e 27
        0xcd, 0x7d, 0xf3,       // call    $f37d                           ;[0061] cd 7d f3
        0xc3, 0x00, 0x01,       // jp      $0100                           ;[0064] c3 00 01
        0x69,                   // ld      l,c                             ;[0067] 69
        0xc0,                   // ret     nz                              ;[0068] c0
        0xcd, 0x00, 0x00,       // call    $0000                           ;[0069] cd 00 00
        0x79,                   // ld      a,c                             ;[006c] 79
        0xe6, 0xfe,             // and     $fe                             ;[006d] e6 fe
        0xd6, 0x02,             // sub     $02                             ;[006f] d6 02
        0xf6, 0x00,             // or      $00                             ;[0071] f6 00
        0xca, 0x22, 0x40,       // jp      z,$4022                         ;[0073] ca 22 40
        0x11, 0x85, 0xc0,       // ld      de,$c085                        ;[0076] 11 85 c0
        0x0e, 0x09,             // ld      c,$09                           ;[0079] 0e 09
        0xcd, 0x7d, 0xf3,       // call    $f37d                           ;[007b] cd 7d f3
        0x0e, 0x07,             // ld      c,$07                           ;[007e] 0e 07
        0xcd, 0x7d, 0xf3,       // call    $f37d                           ;[0080] cd 7d f3
        0x18, 0xb8,             // jr      $003d                           ;[0083] 18 b8
        // 以下データ
        0x42, 0x6F, 0x6F, 0x74, 0x20, 0x65, 0x72, 0x72, // Boot err
        0x6F, 0x72, 0x0D, 0x0A, 0x50, 0x72, 0x65, 0x73, // or..Pres
        0x73, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x6B, 0x65, // s any ke
        0x79, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x72, 0x65, // y for re
        0x74, 0x72, 0x79, 0x0D, 0x0A, 0x24, 0x00, 0x4D, // try..$.M
        0x53, 0x58, 0x44, 0x4F, 0x53, 0x20, 0x20, 0x53, // SXDOS  S
        0x59, 0x53,                                     // YS
    };
    unsigned char dos2[0x1D0] = {
        0xC0, 0x0E, 0x0F, 0xCD, 0x7D, 0xF3, 0x3C, 0xCA, 0x63, 0xC0, 0x11, 0x00, 0x01, 0x0E, 0x1A, 0xCD,
        0x7D, 0xF3, 0x21, 0x01, 0x00, 0x22, 0xB9, 0xC0, 0x21, 0x00, 0x3F, 0x11, 0xAB, 0xC0, 0x0E, 0x27,
        0xCD, 0x7D, 0xF3, 0xC3, 0x00, 0x01, 0x58, 0xC0, 0xCD, 0x00, 0x00, 0x79, 0xE6, 0xFE, 0xFE, 0x02,
        0xC2, 0x6A, 0xC0, 0x3A, 0xD0, 0xC0, 0xA7, 0xCA, 0x22, 0x40, 0x11, 0x85, 0xC0, 0xCD, 0x77, 0xC0,
        0x0E, 0x07, 0xCD, 0x7D, 0xF3, 0x18, 0xB4, 0x1A, 0xB7, 0xC8, 0xD5, 0x5F, 0x0E, 0x06, 0xCD, 0x7D,
        0xF3, 0xD1, 0x13, 0x18, 0xF2, 0x42, 0x6F, 0x6F, 0x74, 0x20, 0x65, 0x72, 0x72, 0x6F, 0x72, 0x0D,
        0x0A, 0x50, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x20, 0x66,
        0x6F, 0x72, 0x20, 0x72, 0x65, 0x74, 0x72, 0x79, 0x0D, 0x0A, 0x00, 0x00, 0x4D, 0x53, 0x58, 0x44,
        0x4F, 0x53, 0x20, 0x20, 0x53, 0x59, 0x53, 0x00};
    srand((unsigned int)time(NULL));
    memcpy(boot.bootJump, bootJump, 3);
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
    memcpy(boot.bootJump2, bootJump2, 2);
    memcpy(boot.idLabel, "VOL_ID", 6);
    boot.dirtyFlag = 0x36;
    if (0 == boot.idValue[0]) {
        // 未設定の場合は乱数を設定 (put/rm向けに設定済みの場合は維持)
        boot.idValue[0] = 0x01 | (rand() & 0xFE);
        boot.idValue[1] = rand() & 0xFF;
        boot.idValue[2] = rand() & 0xFF;
        boot.idValue[3] = rand() & 0xFF;
    }
    memset(boot.reserved, 0, 5);
    memcpy(boot.bootProgram, dos1, sizeof(dos1)); // 暫定的にDOS1のブートプログラムを設定
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
        // 最初のクラスタ番号はディレクトリエントリにのみ記憶
        cfi.entries[i].clusterStart = (unsigned short)c++;
        // 2番目以降のクラスタ番号をFATに記憶
        for (int ii = 1; ii < cfi.entries[i].clusterSize; ii++) {
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
        // 終端コード (0xFFF)
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
    bool isDOS1 = false;
    bool isDOS2 = false;
    unsigned char* d = &diskImage[boot.directoryPosition][0];
    for (int i = 0; i < cfi.entryCount; i++) {
        // Directory
        memcpy(d, cfi.entries[i].name, 8);
        d += 8;
        memcpy(d, cfi.entries[i].ext, 3);
        d += 3;
        *d = 0;
        d += 11;
        memcpy(d, cfi.entries[i].date, 4);
        d += 4;
        memcpy(d, &cfi.entries[i].clusterStart, 2);
        d += 2;
        memcpy(d, &cfi.entries[i].size, 4);
        d += 4;
        // Check MSXDOS.SYS or MSXDOS2.SYS
        if (0 == memcmp(cfi.entries[i].ext, "SYS", 3)) {
            isDOS1 = 0 == memcmp(cfi.entries[i].name, "MSXDOS  ", 8);
            isDOS2 = 0 == memcmp(cfi.entries[i].name, "MSXDOS2 ", 8);
        }
        // File Content
        memcpy(diskImage[boot.dataPosition + (cfi.entries[i].clusterStart - 1) * boot.clusterSize], cfi.entries[i].data, cfi.entries[i].size);
    }

    // update boot program to DOS2 from DOS1
    if (isDOS2) {
        memcpy(boot.bootProgram, dos2, sizeof(dos2));
        extractBootSectorToDisk();
    }

    // Write Disk Image
    FILE* fp = fopen(dskPath, "wb");
    if (NULL == fp) {
        puts("I/O error");
        return 6;
    }
    if (sizeof(diskImage) != fwrite(diskImage, 1, sizeof(diskImage), fp)) {
        puts("I/O error");
        fclose(fp);
        return 6;
    }
    fclose(fp);
    return 0;
}

static int put(const char* dsk, char* path, const char* putAs)
{
    char displayName[4096];
    char name[9];
    char ext[4];
    if (putAs) {
        strcpy(displayName, putAs);
    } else {
        char* cp = strrchr(path, '/');
        if (!cp) cp = strrchr(path, '\\');
        cp = cp ? cp + 1 : path;
        strcpy(displayName, cp);
    }
    int parseError = parseDisplayName(displayName, name, ext);
    if (parseError) return parseError;
    if (!readDisk(dsk)) return 2;
    bool isOverwrite = false;
    cfi.entryCount = 0;
    for (int i = 0; i < dir.entryCount; i++) {
        if (dir.entries[i].removed) continue;
        if (strcasecmp(dir.entries[i].name, name) == 0 && strcasecmp(dir.entries[i].ext, ext) == 0) {
            // 既存ファイルを更新
            isOverwrite = true;
            if (!addCreateFileInfo(path, putAs)) {
                return -1;
            }
        } else {
            // 既存ファイルをそのまま維持
            unsigned char* data = (unsigned char*)malloc(dir.entries[i].size);
            if (!data) {
                puts("No memory");
                return -1;
            }
            wm(nullptr, data, i);
            memcpy(cfi.entries[cfi.entryCount].date, dir.entries[i].dateRaw, 4);
            if (!setCreateFileInfo(cfi.entryCount++, dir.entries[i].name, 8, dir.entries[i].ext, 3, data, dir.entries[i].size)) {
                return -1;
            }
        }
    }
    if (!isOverwrite) {
        if (!addCreateFileInfo(path, putAs)) {
            return -1;
        }
    }
    memset(diskImage, 0, sizeof(diskImage));
    return create(dsk);
}

static int rm(const char* dsk, char* path)
{
    char displayName[4096];
    char name[9];
    char ext[4];
    char* cp = strrchr(path, '/');
    if (!cp) cp = strrchr(path, '\\');
    cp = cp ? cp + 1 : path;
    strcpy(displayName, cp);
    int parseError = parseDisplayName(displayName, name, ext);
    if (parseError) return parseError;
    if (!readDisk(dsk)) return 2;
    bool removed = false;
    cfi.entryCount = 0;
    for (int i = 0; i < dir.entryCount; i++) {
        if (dir.entries[i].removed) continue;
        if (strcasecmp(dir.entries[i].name, name) == 0 && strcasecmp(dir.entries[i].ext, ext) == 0) {
            removed = true;
        } else {
            // 既存ファイルをそのまま維持
            unsigned char* data = (unsigned char*)malloc(dir.entries[i].size);
            if (!data) {
                puts("No memory");
                return -1;
            }
            wm(nullptr, data, i);
            memcpy(cfi.entries[cfi.entryCount].date, dir.entries[i].dateRaw, 4);
            if (!setCreateFileInfo(cfi.entryCount++, dir.entries[i].name, 8, dir.entries[i].ext, 3, data, dir.entries[i].size)) {
                return -1;
            }
        }
    }
    if (!removed) {
        puts("File not found");
        return -1;
    }
    memset(diskImage, 0, sizeof(diskImage));
    return create(dsk);
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
    if (0 == strcasecmp(argv[2], "info")) {
        if (argc != 3) {
            showUsage(BIT_INFO);
            return 1;
        }
        return info(argv[1]);
    } else if (0 == strcasecmp(argv[2], "ls") || 0 == strcasecmp(argv[2], "dir")) {
        if (argc != 3) {
            showUsage(BIT_LS);
            return 1;
        }
        return ls(argv[1]);
    } else if (0 == strcasecmp(argv[2], "cp") || 0 == strcmp(argv[2], "get")) {
        if (argc != 4 && argc != 6) {
            showUsage(BIT_CP);
            return 1;
        }
        if (argc == 6 && 0 != strcasecmp(argv[4], "as")) {
            showUsage(BIT_CP);
            return 1;
        }
        return get(argv[1], argv[3], 6 == argc ? argv[5] : nullptr);
    } else if (0 == strcasecmp(argv[2], "wt") || 0 == strcasecmp(argv[2], "put")) {
        if (argc != 4 && argc != 6) {
            showUsage(BIT_WR);
            return 1;
        }
        if (argc == 6 && 0 != strcasecmp(argv[4], "as")) {
            showUsage(BIT_CP);
            return 1;
        }
        memset(&cfi, 0, sizeof(cfi));
        return put(argv[1], argv[3], 6 == argc ? argv[5] : nullptr);
    } else if (0 == strcasecmp(argv[2], "cat")) {
        if (argc != 4) {
            showUsage(BIT_CAT);
            return 1;
        }
        return cat(argv[1], argv[3]);
    } else if (0 == strcasecmp(argv[2], "rm") || 0 == strcasecmp(argv[2], "del") || 0 == strcasecmp(argv[2], "delete")) {
        if (argc != 4) {
            showUsage(BIT_RM);
            return 1;
        }
        return rm(argv[1], argv[3]);
    } else if (0 == strcasecmp(argv[2], "create")) {
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
