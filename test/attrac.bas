1 REM Copied from https://github.com/gilbertfrancois/msx/blob/master/src/basic/attrac.bas
10 ' Attractors
20 ' Gilbert Francois Duivesteijn
30 '
40 ' attractor parameters
50 '
100 A=-2.8225522543017#
110 B=-2.650397043732#
120 C=.11832952398997#
130 D=-.19495159236333#
140 XS=-.92582235975328#
150 YS=.28866756121724#
160 X0=-1.2301309138714#
170 X1=1.2301242870841#
180 Y0=-1.3143731122071#
190 Y1=1.3144328064812#
200 WM=192    ' screen width
210 HM=192    ' screen height
220 DE=50000!
230 '
240 ' pixel transformation
250 '
260 DEF FN TU(X)=32+CINT((X-X0)/(X1-X0)*WM+.4)
270 DEF FN TV(Y)=CINT((Y1-Y)/(Y1-Y0)*HM+.4)
280 '
290 ' main
300 '
310 SCREEN 2
320 X=XS
330 Y=YS
340 COLOR 1,15,14
350 FOR I=0 TO DE
360 GOSUB 430
370 XP=FN TU(X)
380 YP=FN TV(Y)
390 PSET(XP,YP)
400 NEXT I
410 COLOR 1,15,15
420 GOTO 420
430 '
440 ' attractor function
450 ' in : x, y, a, b, c, d
460 ' out: x, y
470 '
480 XN=SIN(Y*B)+C*SIN(X*B)
490 YN=SIN(X*A)+D*SIN(Y*A)
500 X=XN
510 Y=YN
520 RETURN
