1 REM Test Code for bas2txt and txt2bas filter
100 LET F0=!123456
110 LET F1=!123456
120 LET F2=!123.456
130 LET F3=!.123456
140 LET F4=!.0123456
150 LET D1=#12345678901234
160 LET D2=#12345678901234
170 LET D3=#1234567.8901234
180 LET D4=#.12345678901234
190 LET D5=#.012345678901234
200 LET S0=#123456
210 LET S1=#123456.789
220 LET S2=#123456
230 LET S3=#123456
240 LET S4=#123456.789
250 LET S5=#123456
260 LET S6=!123456
270 LET S7=!123456
280 LET S8=!123456
1000 PRINT F0+F1+F2+F3+F4
1010 PRINT F0;F1;F2;F4;F4
1020 PRINT D0+D1+D2+D3+D4
1030 PRINT D0;D1;D2;D4;D4
