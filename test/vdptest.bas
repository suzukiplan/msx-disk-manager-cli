10CLEAR10,&HC8FF:BLOAD"vdptest.bin"
20DEFFNS(A)=PEEK(A)+256*(PEEK(A)>127)
30DEFFNW(A)=PEEK(A)+256*PEEK(A+1)
40DEFUSR=&HC900
50A=USR(0)
60B=FNW(-1675)+65536*PEEK(-1673)
70SCREEN0
80?"Cyc,Err"B;A
90?"ACK "FNS(-1672)FNS(-1671)FNS(-1670)FNS(-1669)
