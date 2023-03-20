all:
	../dskmgr ./wmsx.dsk get hello.bas
	../dskmgr ./wmsx.dsk get hoge.bas
	../dskmgr ./image.dsk create hello.bas hoge.bas text.bas
	../dskmgr ./image.dsk info
	../dskmgr ./image.dsk ls
	../dskmgr ./image.dsk get hello.bas
	../dskmgr ./image.dsk get hoge.bas
	../dskmgr ./image.dsk cat hello.bas
	../dskmgr ./image.dsk cat hoge.bas
	../dskmgr ./image.dsk cat text.bas >text.bas
