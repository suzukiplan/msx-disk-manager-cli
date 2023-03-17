all:
	clang++ -o dskmgr src/dskmgr.cpp

test: all
	@echo ========================================
	cd test && ../dskmgr ./wmsx.dsk info
	@echo ========================================
	cd test && ../dskmgr ./wmsx.dsk ls
	@echo ========================================
	cd test && ../dskmgr ./wmsx.dsk cp hello.bas
	cd test && ../dskmgr ./wmsx.dsk cp hoge.bas
