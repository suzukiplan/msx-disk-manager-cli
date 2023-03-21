/**
 * SUZUKI PLAN - MSX-BASIC Filter
 * Mutual conversion between MSX-BASIC text and plane text
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
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class BasicFilter
{
  public:
    void bas2txt(FILE* stream, unsigned char* cBuf)
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
                        fprintf(stream, "%s", getStatementFromCode(scode)->word);
                        break;
                    case 0x3a:
                        if (cBuf[x] == 0xA1) {
                            x++;
                            scode = 0x3AA1; // else
                            fprintf(stream, "%s", getStatementFromCode(scode)->word);
                        } else if (cBuf[x] == 0x8F && cBuf[x + 1] == 0xE6) {
                            x += 2;
                            scode = 0x3A8FE6; // '
                            fprintf(stream, "%s", getStatementFromCode(scode)->word);
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
                        fprintf(stream, "%d", ivalue);
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
                        fprintf(stream, "%%%d", ivalue);
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
                        fprintf(stream, "%%%d", scode - 0x11);
                        break;
                    case 0x1c: //int num
                        ivalue = (cBuf[x]) | (cBuf[x + 1] << 8);
                        x += 2;
                        fprintf(stream, "%%%d", ivalue);
                        break;
                    case 0x1d: //単精度BCD浮動小数点数
                    {
                        fprintf(stream, "!%s", bcdFloatS2String(&cBuf[x]));
                        x += 4;
                        break;
                    }
                    case 0x1f: //倍精度BCD浮動小数点数
                    {
                        fprintf(stream, "#%s", bcdFloatD2String(&cBuf[x]));
                        x += 8;
                        break;
                    }
                    default:
                        if (scode < 0x80) {
                            fputc(scode, stream);
                        } else {
                            fprintf(stream, "%s", getStatementFromCode(scode)->word);
                        }
                        break;
                }
            }
            fprintf(stream, "\n");
            x = lp - ofs;
            lp = (cBuf[x + 1] << 8) | cBuf[x];
            x += 2;
        }
    }

    unsigned char* txt2bas(const char* src, size_t* basSize)
    {
        size_t len = strlen(src);
        char* text = (char*)malloc(len + 1);
        if (!text) {
            puts("No memory");
            return nullptr;
        }
        strcpy(text, src);
        unsigned char* result = (unsigned char*)malloc(len + 1);
        if (!result) {
            puts("No memory");
            free(text);
            return nullptr;
        }
        int ptr = 0;
        int ln = 0;
        unsigned short offset;
        int optr;
        result[ptr++] = 0xFF;
        char* line = strtok(text, "\n");
        while (line) {
            ln++;
            // CRLFがある場合は潰しておく
            char* cp = strchr(line, '\r');
            if (cp) *cp = 0;
            cp = strchr(line, '\n');
            if (cp) *cp = 0;
            trimstring(line);

            // 空行はスキップ
            trimstring(line);
            if (0 == *line) {
                line = strtok(NULL, "\n");
                continue;
            }

            // オフセット更新
            if (1 < ptr) {
                offset = 0x8000 + ptr;
                memcpy(&result[optr], &offset, 2);
            }
            optr = ptr;
            result[ptr++] = 0xFF; // dummy (current offset)
            result[ptr++] = 0xFF; // dummy (current offset)

            // 行番号を取得
            int lineNumber = atoi(line);
            if (lineNumber < 1 || 65535 < lineNumber) {
                free(text);
                free(result);
                return nullptr;
            }
            while (isdigit(*line)) line++;
            trimstring(line);
            result[ptr++] = lineNumber & 0x00FF;
            result[ptr++] = (lineNumber & 0xFF00) >> 8;

            // 構文解析
            char previous = 0x00;
            while (*line) {
                // ダブルクォーテーションで囲まれている範囲をそのまま出力
                if ('"' == *line) {
                    result[ptr++] = '"';
                    for (++line; *line && *line != '"'; line++) {
                        result[ptr++] = *line;
                    }
                    if (*line == '"') {
                        result[ptr++] = '"';
                        line++;
                    }
                    previous = '"';
                    continue;
                }
                // ステートメント解析
                auto st = getStatementFromWord(line);
                if (0 < st->code) {
                    if (st->code < 0x100) {
                        // single byte statement
                        result[ptr++] = (unsigned char)(st->code & 0xFF);
                    } else if (st->code < 0x10000) {
                        // 2 bytes statement
                        result[ptr++] = (unsigned char)((st->code & 0xFF00) >> 8);
                        result[ptr++] = (unsigned char)(st->code & 0xFF);
                    } else if (st->code < 0x1000000) {
                        // 3 bytes statement
                        result[ptr++] = (unsigned char)((st->code & 0xFF0000) >> 16);
                        result[ptr++] = (unsigned char)((st->code & 0xFF00) >> 8);
                        result[ptr++] = (unsigned char)(st->code & 0xFF);
                    } else {
                        // 4 bytes statement
                        result[ptr++] = (unsigned char)((st->code & 0xFF000000) >> 24);
                        result[ptr++] = (unsigned char)((st->code & 0xFF0000) >> 16);
                        result[ptr++] = (unsigned char)((st->code & 0xFF00) >> 8);
                        result[ptr++] = (unsigned char)(st->code & 0xFF);
                    }
                    line += strlen(st->word);
                    if (st->code == 0x8F) {
                        // REM (コメント) を検出したので行末までそのまま出力
                        while (*line) {
                            result[ptr++] = *line;
                            line++;
                        }
                    } else if (st->code == 0x89 || st->code == 0x8D) {
                        // GOTO/GOSUB を検出したので行番号を出力
                        while (' ' == *line || '\t' == *line) line++;
                        int i = atoi(line);
                        result[ptr++] = ' ';
                        result[ptr++] = 0x0E;
                        result[ptr++] = i & 0xFF;
                        result[ptr++] = (i >> 8) & 0xFF;
                        while (isdigit(*line)) line++;
                    }
                    previous = st->code;
                    continue;
                }
                // 8進数解析
                if (0 == strncasecmp(line, "&O", 2)) {
                    line += 2;
                    int len = 0;
                    auto hex = oct2i(line, &len);
                    line += len;
                    result[ptr++] = 0x0B;
                    result[ptr++] = (unsigned char)(hex & 0xFF);
                    result[ptr++] = (unsigned char)((hex & 0xFF00) >> 8);
                    previous = 0x0B;
                    continue;
                }
                // 16進数変換
                if (0 == strncasecmp(line, "&H", 2)) {
                    line += 2;
                    int len = 0;
                    auto hex = hex2i(line, &len);
                    line += len;
                    result[ptr++] = 0x0C;
                    result[ptr++] = (unsigned char)(hex & 0xFF);
                    result[ptr++] = (unsigned char)((hex & 0xFF00) >> 8);
                    previous = 0;
                    continue;
                }
                // 10進数変換
                if (isDecimal(line)) {
                    bool prefix = !isdigit(*line);
                    if (prefix) line++;
                    int i = atoi(line);
                    printf("decimal = %d\n", i);
                    if (i < 10) {
                        result[ptr++] = 0x11 + i;
                    } else if (i < 256) {
                        result[ptr++] = 0x0F;
                        result[ptr++] = i & 0xFF;
                    } else {
                        result[ptr++] = 0x1C;
                        result[ptr++] = i & 0xFF;
                        result[ptr++] = (i >> 8) & 0xFF;
                    }
                    while (isdigit(*line)) line++;
                    if (!prefix) line++;
                    previous = 0;
                    continue;
                }
                // 実数 (単精度 or 倍精度のBCD浮動小数点数)
                if (isDouble(previous, line) || isFloat(line)) {
                    bool isDouble = this->isDouble(previous, line);
                    bool prefix = !isdigit(*line);
                    if (prefix) line++;
                    result[ptr++] = isDouble ? 0x1F : 0x1D;
                    // 数字列切り出し
                    char fstr[128];
                    memset(fstr, 0, sizeof(fstr));
                    int fptr = 0;
                    while (fptr < sizeof(fstr) - 1 && (isdigit(*line) || '.' == *line)) {
                        fstr[fptr++] = *line;
                        line++;
                    }
                    if (!prefix) line++;
                    makeBcdFloat(fstr, &result[ptr], isDouble);
                    ptr += isDouble ? 8 : 4;
                    previous = 0;
                    continue;
                }
                // 2進数変換
                if (0 == strncasecmp(line, "&B", 2)) {
                    line += 2;
                    result[ptr++] = '&';
                    result[ptr++] = 'B';
                    while ('0' == *line || '1' == *line) {
                        result[ptr++] = *line;
                    }
                    previous = 0;
                    continue;
                }
                // 何れにも該当しない (そのまま出力)
                result[ptr++] = *line;
                previous = *line;
                line++;
            }
            result[ptr++] = 0;
            line = strtok(NULL, "\n");
        }
        // オフセット更新
        if (1 < ptr) {
            offset = 0x8000 + ptr;
            memcpy(&result[optr], &offset, 2);
        }
        result[ptr++] = 0;
        result[ptr++] = 0;
        *basSize = ptr;
        free(text);
        return result;
    }

    void txt2bas_free(unsigned char* bas)
    {
        free(bas);
    }

  private:
    struct StatementRecord {
        char word[16];
        unsigned int code;
    } stbl[256] = {{"", 0}, {">", 0xEE}, {"CMD", 0xD7}, {"ERR", 0xE2}, {"LIST", 0x93}, {"PAINT", 0xBF}, {"SPRITE", 0xC7}, {"=", 0xEF}, {"COLOR", 0xBD}, {"ERROR", 0xA6}, {"LLIST", 0x9E}, {"PDL", 0xFFA4}, {"SQR", 0xFF87}, {"<", 0xF0}, {"CONT", 0x99}, {"EXP", 0xFF8B}, {"LOAD", 0xB5}, {"PEEK", 0xFF97}, {"STEP", 0xDC}, {"+", 0xF1}, {"COPY", 0xD6}, {"FIELD", 0xB1}, {"LOC", 0xFFAC}, {"PLAY", 0xC1}, {"STICK", 0xFFA2}, {"-", 0xF2}, {"COS", 0xFF8C}, {"FILES", 0xB7}, {"LOCATE", 0xD8}, {"POINT", 0xED}, {"STOP", 0x90}, {"*", 0xF3}, {"CSAVE", 0x9A}, {"FIX", 0xFFA1}, {"LOF", 0xFFAD}, {"POKE", 0x98}, {"STR$", 0xFF93}, {"/", 0xF4}, {"CSNG", 0xFF9F}, {"FN", 0xDE}, {"LOG", 0xFF8A}, {"POS", 0xFF91}, {"STRIG", 0xFFA3}, {"^", 0xF5}, {"CSRLIN", 0xE8}, {"FOR", 0x82}, {"LPOS", 0xFF9C}, {"PRESET", 0xC3}, {"STRING$", 0xE3}, {"\\", 0xFC}, {"CVD", 0xFFAA}, {"FPOS", 0xFFA7}, {"LPRINT", 0x9D}, {"PRINT", 0x91}, {"SWAP", 0xA4}, {"ABS", 0xFF86}, {"CVI", 0xFFA8}, {"FRE", 0xFF8F}, {"LSET", 0xB8}, {"PSET", 0xC2}, {"TAB(", 0xDB}, {"AND", 0xF6}, {"CVS", 0xFFA9}, {"GET", 0xB2}, {"MAX", 0xCD}, {"PUT", 0xB3}, {"TAN", 0xFF8D}, {"ASC", 0xFF95}, {"DATA", 0x84}, {"GOSUB", 0x8D}, {"MERGE", 0xB6}, {"READ", 0x87}, {"THEN", 0xDA}, {"ATN", 0xFF8E}, {"DEF", 0x97}, {"GOTO", 0x89}, {"MID$", 0xFF83}, {"REM", 0x8F}, {"TIME", 0xCB}, {"ATTR$", 0xE9}, {"DEFDBL", 0xAE}, {"HEX$", 0xFF9B}, {"MKD$", 0xFFB0}, {"RENUM", 0xAA}, {"TO", 0xD9}, {"AUTO", 0xA9}, {"DEFINT", 0xAC}, {"IF", 0x8B}, {"MKI$", 0xFFAE}, {"RESTORE", 0x8C}, {"TROFF", 0xA3}, {"BASE", 0xC9}, {"DEFSNG", 0xAD}, {"IMP", 0xFA}, {"MKS$", 0xFFAF}, {"RESUME", 0xA7}, {"TRON", 0xA2}, {"BEEP", 0xC0}, {"DEFSTR", 0xAB}, {"INKEY$", 0xEC}, {"MOD", 0xFB}, {"RETURN", 0x8E}, {"USING", 0xE4}, {"BIN$", 0xFF9D}, {"DELETE", 0xA8}, {"INP", 0xFF90}, {"MOTOR", 0xCE}, {"RIGHT$", 0xFF82}, {"USR", 0xDD}, {"BLOAD", 0xCF}, {"DIM", 0x86}, {"INPUT", 0x85}, {"NAME", 0xD3}, {"RND", 0xFF88}, {"VAL", 0xFF94}, {"BSAVE", 0xD0}, {"DRAW", 0xBE}, {"INSTR", 0xE5}, {"NEW", 0x94}, {"RSET", 0xB9}, {"VARPTR", 0xE7}, {"CALL", 0xCA}, {"DSKF", 0xFFA6}, {"INT", 0xFF85}, {"NEXT", 0x83}, {"RUN", 0x8A}, {"VDP", 0xC8}, {"CDBL", 0xFFA0}, {"DSKI$", 0xEA}, {"IPL", 0xD5}, {"NOT", 0xE0}, {"SAVE", 0xBA}, {"VPEEK", 0xFF98}, {"CHR$", 0xFF96}, {"DSKO$", 0xD1}, {"KEY", 0xCC}, {"OCT$", 0xFF9A}, {"SCREEN", 0xC5}, {"VPOKE", 0xC6}, {"CINT", 0xFF9E}, {"ELSE", 0x3AA1}, {"KILL", 0xD4}, {"OFF", 0xEB}, {"SET", 0xD2}, {"WAIT", 0x96}, {"CIRCLE", 0xBC}, {"END", 0x81}, {"LEFT$", 0xFF81}, {"ON", 0x95}, {"SGN", 0xFF84}, {"WIDTH", 0xA0}, {"CLEAR", 0x92}, {"EOF", 0xFFAB}, {"LEN", 0xFF92}, {"OPEN", 0xB0}, {"SIN", 0xFF89}, {"XOR", 0xF8}, {"CLOAD", 0x9B}, {"EQV", 0xF9}, {"LET", 0x88}, {"OR", 0xF7}, {"SOUND", 0xC4}, {"CLOSE", 0xB4}, {"ERASE", 0xA5}, {"LFILES", 0xBB}, {"OUT", 0x9C}, {"SPACE$", 0xFF99}, {"CLS", 0x9F}, {"ERL", 0xE1}, {"LINE", 0xAF}, {"PAD", 0xFFA5}, {"SPC(", 0xDF}, {"'", 0x3A8FE6}, {"", 0}};

    bool isDouble(char previous, const char* str)
    {
        if ('#' == *str) return true;
        if (!isdigit(*str) && '.' != *str) return false;
        while (isdigit(*str) || '.' == *str) str++;
        if ('!' == *str || '%' == *str) return false;
        if ((previous & 0x80) || '#' == *str) return true;
        return false;
    }

    bool isFloat(const char* str)
    {
        if ('!' == *str) return true;
        if (!isdigit(*str) && '.' != *str) return false;
        while (isdigit(*str) || '.' == *str) str++;
        if ('!' == *str) return true;
        return false;
    }

    bool isDecimal(const char* str)
    {
        if ('%' == *str && isdigit(*(str + 1))) return true;
        if (!isdigit(*str)) return false;
        while (isdigit(*str)) str++;
        if ('%' == *str) return true;
        return false;
    }

    const char* bcdFloatS2String(const unsigned char* buf)
    {
        static char result[256];
        char fmt[256];
        if (0 == buf[0]) {
            strcpy(result, "0");
            return result;
        }
        char n[6];
        for (int i = 0; i < 3; i++) {
            n[i * 2] = '0' + ((buf[i + 1] & 0xF0) >> 4);
            n[i * 2 + 1] = '0' + (buf[i + 1] & 0x0F);
        }
        int pw = buf[0] & 0x7F;
        if (pw & 0x40) {
            pw &= 0x3F;
            switch (pw) {
                case 6: strcpy(fmt, "%c%c%c%c%c%c"); break;
                case 5: strcpy(fmt, "%c%c%c%c%c.%c"); break;
                case 4: strcpy(fmt, "%c%c%c%c.%c%c"); break;
                case 3: strcpy(fmt, "%c%c%c.%c%c%c"); break;
                case 2: strcpy(fmt, "%c%c.%c%c%c%c"); break;
                case 1: strcpy(fmt, "%c.%c%c%c%c%c"); break;
                case 0: strcpy(fmt, ".%c%c%c%c%c%c"); break;
                default: {
                    strcpy(fmt, "%c%c%c%c%c%c");
                    for (; 6 < pw; pw--) {
                        strcat(fmt, "0");
                    }
                }
            }
        } else {
            pw ^= 0x3F;
            strcpy(fmt, ".");
            for (; 0 <= pw; pw--) {
                strcat(fmt, "0");
            }
            strcat(fmt, "%c%c%c%c%c%c");
        }
        snprintf(result, sizeof(result), fmt, n[0], n[1], n[2], n[3], n[4], n[5]);
        trimnum(result);
        return result;
    }

    const char* bcdFloatD2String(const unsigned char* buf)
    {
        static char result[512];
        char fmt[512];
        if (0 == buf[0]) {
            strcpy(result, "0");
            return result;
        }
        char n[14];
        for (int i = 0; i < 7; i++) {
            n[i * 2] = '0' + ((buf[i + 1] & 0xF0) >> 4);
            n[i * 2 + 1] = '0' + (buf[i + 1] & 0x0F);
        }
        int pw = buf[0] & 0x7F;
        fmt[0] = 0;
        if (pw & 0x40) {
            pw &= 0x3F;
            switch (pw) {
                case 14: strcpy(fmt, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c"); break;
                case 13: strcpy(fmt, "%c%c%c%c%c%c%c%c%c%c%c%c%c.%c"); break;
                case 12: strcpy(fmt, "%c%c%c%c%c%c%c%c%c%c%c%c.%c%c"); break;
                case 11: strcpy(fmt, "%c%c%c%c%c%c%c%c%c%c%c.%c%c%c"); break;
                case 10: strcpy(fmt, "%c%c%c%c%c%c%c%c%c%c.%c%c%c%c"); break;
                case 9: strcpy(fmt, "%c%c%c%c%c%c%c%c%c.%c%c%c%c%c"); break;
                case 8: strcpy(fmt, "%c%c%c%c%c%c%c%c.%c%c%c%c%c%c"); break;
                case 7: strcpy(fmt, "%c%c%c%c%c%c%c.%c%c%c%c%c%c%c"); break;
                case 6: strcpy(fmt, "%c%c%c%c%c%c.%c%c%c%c%c%c%c%c"); break;
                case 5: strcpy(fmt, "%c%c%c%c%c.%c%c%c%c%c%c%c%c%c"); break;
                case 4: strcpy(fmt, "%c%c%c%c.%c%c%c%c%c%c%c%c%c%c"); break;
                case 3: strcpy(fmt, "%c%c%c.%c%c%c%c%c%c%c%c%c%c%c"); break;
                case 2: strcpy(fmt, "%c%c.%c%c%c%c%c%c%c%c%c%c%c%c"); break;
                case 1: strcpy(fmt, "%c.%c%c%c%c%c%c%c%c%c%c%c%c%c"); break;
                case 0: strcpy(fmt, ".%c%c%c%c%c%c%c%c%c%c%c%c%c%c"); break;
                default: {
                    strcpy(fmt, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c");
                    for (; 14 < pw; pw--) {
                        strcat(fmt, "0");
                    }
                }
            }
        } else {
            pw ^= 0x3F;
            strcpy(fmt, ".");
            for (; 0 <= pw; pw--) {
                strcat(fmt, "0");
            }
            strcat(fmt, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c");
        }
        snprintf(result, sizeof(result), fmt, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8], n[9], n[10], n[11], n[12], n[13]);
        trimnum(result);
        return result;
    }

    void makeBcdFloat(char* fstr, unsigned char* buf, bool isDouble)
    {
        trimnum(fstr);
        // fstr から小数点を削除した数字列 (mantissa) を作成
        char mantissa[512];
        strcpy(mantissa, fstr);
        char* dot = strchr(mantissa, '.');
        if (dot) {
            memmove(dot, dot + 1, strlen(dot + 1) + 1);
            // BCDを求める時のワーク用に 0 を付与
            strcat(mantissa, isDouble ? "00000000000000" : "000000");
        } else {
            // 上限桁数未満の場合はBCDを求める時のワーク用に 0 を付与
            for (int len = (int)strlen(mantissa); len < (isDouble ? 14 : 6); len++) {
                memmove(mantissa + 1, mantissa, strlen(mantissa) + 1);
                mantissa[0] = '0';
            }
        }
        // mantissaが0ならオール0を返す
        if (0L == atol(mantissa)) {
            for (int i = 0; i < (isDouble ? 8 : 4); i++) {
                buf[i] = 0;
            }
            return;
        }
        // 小数点の位置から指数部 (index) を算出
        dot = strchr(fstr, '.');
        if (!dot) {
            // 整数
            buf[0] = 0x40 | (isDouble ? 14 : 6);
        } else if (0 == atoi(fstr)) {
            // 1未満の実数
            buf[0] = 0x40;
            while ('0' == mantissa[0] && 0 != buf[0]) {
                memmove(&mantissa[0], &mantissa[1], strlen(&mantissa[1]) + 1);
                buf[0]--;
            }
        } else {
            // 1以上の実数
            int index = (int)((dot ? dot : fstr) - fstr);
            buf[0] = 0x40 | index;
            buf[0] &= 0x7F;
        }
        // mantissaをBCD形式でメモリに設定
        for (int i = 0; i < (isDouble ? 7 : 3); i++) {
            buf[1 + i] = (mantissa[i * 2] - '0') << 4;
            buf[1 + i] |= mantissa[i * 2 + 1] - '0';
        }
    }

    void trimnum(char* result)
    {
        // 先頭の0をトリム
        char* z = result;
        while (*z == '0') z++;
        if (z != result) {
            memmove(result, z, strlen(z) + 1);
        }
        // 小数点以下が0なら小数点から削除
        char* dot = strchr(result, '.');
        if (dot) {
            char* cp = dot;
            for (++cp; *cp == '0'; cp++) {
                ;
            }
            if (0 == *cp) {
                *dot = 0;
            }
        }
        // 小数点以下の末尾の0をトリム
        dot = strchr(result, '.');
        if (dot) {
            for (dot += strlen(dot) - 1; *dot != '.' && *dot == '0'; dot--) {
                *dot = 0;
            }
        }
    }

    void trimstring(char* src)
    {
        int i;
        for (i = 0; ' ' == src[i] || '\t' == src[i]; i++) {
            ;
        }
        if (i) {
            memmove(src, &src[i], strlen(&src[i]) + 1);
        }
        for (int len = (int)strlen(src) - 1; 0 <= len && (' ' == src[len] || '\t' == src[len]); len--) {
            src[len] = '\0';
        }
    }

    unsigned int oct2i(const char* srcStr, int* len)
    {
        unsigned int value = 0;
        int cc, cb;
        int i = 0;
        while (srcStr[i]) {
            cc = srcStr[i++];
            cb = -1;
            if (cc >= '0' & cc <= '7') { cb = cc - '0'; }
            if (cb == -1) { return value; }
            value = value << 3;
            value |= cb;
            *len = 1 + *len;
        }
        return value;
    }

    unsigned int hex2i(const char* srcStr, int* len)
    {
        unsigned int value = 0;
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
            *len = 1 + *len;
        }
        return value;
    }

    struct StatementRecord* getStatementFromCode(unsigned int code)
    {
        for (int i = 1; stbl[i].code; i++) {
            if (stbl[i].code == code) {
                return &stbl[i];
            }
        }
        return &stbl[0];
    }

    struct StatementRecord* getStatementFromWord(const char* src)
    {
        struct StatementRecord* result = &stbl[0];
        size_t rl = 0;
        for (int i = 1; stbl[i].code; i++) {
            size_t wl = strlen(stbl[i].word);
            if (0 == strncasecmp(src, stbl[i].word, wl)) {
                if (rl < wl) {
                    rl = wl;
                    result = &stbl[i];
                }
            }
        }
        return result;
    }
};