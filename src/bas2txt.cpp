// Original: http://ryusendo.rdy.jp/?page_id=268
// Modified by Y.Suzuki
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* wordTable[] = {
    ">", "EE", "CMD", "D7", "ERR", "E2", "LIST", "93", "PAINT", "BF", "SPRITE", "C7",
    "=", "EF", "COLOR", "BD", "ERROR", "A6", "LLIST", "9E", "PDL", "FFA4", "SQR", "FF87",
    "<", "F0", "CONT", "99", "EXP", "FF8B", "LOAD", "B5", "PEEK", "FF97", "STEP", "DC",
    "+", "F1", "COPY", "D6", "FIELD", "B1", "LOC", "FFAC", "PLAY", "C1", "STICK", "FFA2",
    "-", "F2", "COS", "FF8C", "FILES", "B7", "LOCATE", "D8", "POINT", "ED", "STOP", "90",
    "*", "F3", "CSAVE", "9A", "FIX", "FFA1", "LOF", "FFAD", "POKE", "98", "STR$", "FF93",
    "/", "F4", "CSNG", "FF9F", "FN", "DE", "LOG", "FF8A", "POS", "FF91", "STRIG", "FFA3",
    "^", "F5", "CSRLIN", "E8", "FOR", "82", "LPOS", "FF9C", "PRESET", "C3", "STRING$", "E3",
    "\\", "FC", "CVD", "FFAA", "FPOS", "FFA7", "LPRINT", "9D", "PRINT", "91", "SWAP", "A4",
    "ABS", "FF86", "CVI", "FFA8", "FRE", "FF8F", "LSET", "B8", "PSET", "C2", "TAB(", "DB",
    "AND", "F6", "CVS", "FFA9", "GET", "B2", "MAX", "CD", "PUT", "B3", "TAN", "FF8D",
    "ASC", "FF95", "DATA", "84", "GOSUB", "8D", "MERGE", "B6", "READ", "87", "THEN", "DA",
    "ATN", "FF8E", "DEF", "97", "GOTO", "89", "MID$", "FF83", "REM", "3A8F", "TIME", "CB",
    "ATTR$", "E9", "DEFDBL", "AE", "HEX$", "FF9B", "MKD$", "FFB0", "RENUM", "AA", "TO", "D9",
    "AUTO", "A9", "DEFINT", "AC", "IF", "8B", "MKI$", "FFAE", "RESTORE", "8C", "TROFF", "A3",
    "BASE", "C9", "DEFSNG", "AD", "IMP", "FA", "MKS$", "FFAF", "RESUME", "A7", "TRON", "A2",
    "BEEP", "C0", "DEFSTR", "AB", "INKEY$", "EC", "MOD", "FB", "RETURN", "8E", "USING", "E4",
    "BIN$", "FF9D", "DELETE", "A8", "INP", "FF90", "MOTOR", "CE", "RIGHT$", "FF82", "USR", "DD",
    "BLOAD", "CF", "DIM", "86", "INPUT", "85", "NAME", "D3", "RND", "FF88", "VAL", "FF94",
    "BSAVE", "D0", "DRAW", "BE", "INSTR", "E5", "NEW", "94", "RSET", "B9", "VARPTR", "E7",
    "CALL", "CA", "DSKF", "FFA6", "INT", "FF85", "NEXT", "83", "RUN", "8A", "VDP", "C8",
    "CDBL", "FFA0", "DSKI$", "EA", "IPL", "D5", "NOT", "E0", "SAVE", "BA", "VPEEK", "FF98",
    "CHR$", "FF96", "DSKO$", "D1", "KEY", "CC", "OCT$", "FF9A", "SCREEN", "C5", "VPOKE", "C6",
    "CINT", "FF9E", "ELSE", "3AA1", "KILL", "D4", "OFF", "EB", "SET", "D2", "WAIT", "96",
    "CIRCLE", "BC", "END", "81", "LEFT$", "FF81", "ON", "95", "SGN", "FF84", "WIDTH", "A0",
    "CLEAR", "92", "EOF", "FFAB", "LEN", "FF92", "OPEN", "B0", "SIN", "FF89", "XOR", "F8",
    "CLOAD", "9B", "EQV", "F9", "LET", "88", "OR", "F7", "SOUND", "C4",
    "CLOSE", "B4", "ERASE", "A5", "LFILES", "BB", "OUT", "9C", "SPACE$", "FF99",
    "CLS", "9F", "ERL", "E1", "LINE", "AF", "PAD", "FFA5", "SPC(", "DF", "", ""};

static int hextoi(const char* srcStr)
{
    int value = 0;
    int cc, cb;
    int i = 0;

    while (srcStr[i]) {
        cc = srcStr[i++];
        cb = -1;
        if (cc >= '0' & cc <= '9') { cb = cc - '0'; }
        if (cc >= 'A' & cc <= 'F') { cb = cc - 'A' + 10; }
        if (cc >= 'a' & cc <= 'f') { cb = cc - 'a' + 10; }
        if (cb == -1) { return value; }
        value = value << 4;
        value |= cb;
    }
    return value;
}

static int scanTable(int srcCode)
{
    int i = 0;
    while (wordTable[i][0]) {
        if (hextoi(wordTable[i + 1]) == srcCode) {
            return i;
        }
        i += 2;
    }
    return -1;
}

int bas2txt(FILE* stream, unsigned char* cBuf)
{

    int x;
    int lp;
    int ofs;
    int lineNum;
    int scode;
    int ivalue, linevalue;
    char tmpc[32];
    int itmp;

    x = 1;
    ofs = 0x8000;
    lp = (cBuf[x + 1] << 8) | cBuf[x];
    x += 2;
    while (lp != 0) {
        lineNum = (cBuf[x + 1] << 8) | cBuf[x];
        x += 2;
        fprintf(stream, "%d ", lineNum);
        while (cBuf[x]) {
            scode = cBuf[x++];
            switch (scode) {
                case 0xff:
                    scode = (scode << 8) | (cBuf[x++]);
                    fprintf(stream, "%s", wordTable[scanTable(scode)]);
                    break;
                case 0x3a:
                    if (cBuf[x] > 0x81) {
                        scode = (scode << 8) | (cBuf[x++]);
                        if (cBuf[x] == 0xe6) {
                            x++;
                            fprintf(stream, "'");
                        } else {
                            itmp = scanTable(scode);
                            if (itmp > 0) {
                                fprintf(stream, "%s", wordTable[itmp]);
                            } else {
                                x--;
                            }
                        }
                    } else {
                        fprintf(stream, ":");
                    }
                    break;
                case 0x0c: //hex num
                    ivalue = (cBuf[x]) | (cBuf[x + 1] << 8);
                    x += 2;
                    fprintf(stream, "&H%X", ivalue);
                    break;
                case 0x0e: //line num(line)
                    ivalue = (cBuf[x]) | (cBuf[x + 1] << 8);
                    x += 2;
                    fprintf(stream, "%X", ivalue);
                    break;
                case 0x0d: //line num(addr)
                    ivalue = (cBuf[x]) | (cBuf[x + 1] << 8);
                    ivalue -= ofs;
                    linevalue = (cBuf[ivalue]) | (cBuf[ivalue + 1] << 8);
                    fprintf(stream, "%d", linevalue);
                    x += 2;
                    break;
                case 0x0f: //num 10-255
                    ivalue = (cBuf[x]);
                    x += 1;
                    fprintf(stream, "%d", ivalue);
                    break;
                case 0x11: //num 0
                case 0x12: //num 1
                case 0x13: //num 2
                case 0x14: //num 3
                case 0x15: //num 4
                case 0x16: //num 5
                case 0x17: //num 6
                case 0x18: //num 7
                case 0x19: //num 8
                case 0x1a: //num 9
                    fprintf(stream, "%d", scode - 0x11);
                    break;
                case 0x1c: //int num
                    ivalue = (cBuf[x]) | (cBuf[x + 1] << 8);
                    x += 2;
                    fprintf(stream, "%d", ivalue);
                    break;
                default:
                    if (scode < 0x80) {
                        tmpc[0] = scode;
                        tmpc[1] = 0;
                        fprintf(stream, "%s", tmpc);
                    } else {
                        fprintf(stream, "%s", wordTable[scanTable(scode)]);
                    }
                    break;
            }
        }
        fprintf(stream, "\n");
        x = lp - ofs;
        lp = (cBuf[x + 1] << 8) | cBuf[x];
        x += 2;
    }

    return 0;
}