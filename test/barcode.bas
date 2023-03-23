1 REM copied from https://github.com/gilbertfrancois/msx/blob/master/src/basic/barcode.bas
10 ' Barcode
20 ' Gilbert Francois Duivesteijn
30 '
100 COLOR 1,15,15
110 SCREEN 2: CLS
120 A=(RND(TIME)-1)*.6
125 W=0
130 FOR Y0=-30 TO 296 STEP 10
140 FOR X0=0 TO 256
150 YV=Y0/96*18*COS(-A+X0/256*9)*SIN(Y0/192*7)
160 Y1=Y0+YV+X0*A
170 PSET(X0,Y1),15
180 PR = RND(10)
190 IF PR > .5 THEN C1=1  ELSE C1=15
200 LINE(X0,Y1+1)-(X0,192),C1
210 NEXT X0
220 NEXT Y0
230 GOTO 230

