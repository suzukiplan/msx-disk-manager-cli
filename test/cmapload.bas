1 REM copied from https://github.com/gilbertfrancois/msx/blob/master/src/basic/cmapload.bas
10 ' Load character map
20 ' Gilbert Francois Duivesteijn
30 '
40 BLOAD"cyrmap.bin"
50 FOR I=0 TO 255
60 FOR J=0 TO 7
70 K=I*8+J
80 VPOKE BASE(2)+K,PEEK(&HC800+K)
90 NEXT J
100 NEXT I
110 PRINT "Ready..."
120 PRINT"---------------------------------------"
130 FOR I=192 TO 255 STEP 8
140 FOR J=0 TO 7
150 IF J=0 THEN PRINT I;" ";
160 PRINT USING "\\";CHR$(I+J);" ";
170 NEXT J
180 PRINT
190 NEXT I