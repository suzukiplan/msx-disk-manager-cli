all:
	clang++ -o dskmgr src/dskmgr.cpp

test: all
	@echo ========================================
	./dskmgr test/wmsx.dsk info
	@echo ========================================
	./dskmgr test/wmsx.dsk ls
