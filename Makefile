all:
	clang++ -o dskmgr src/dskmgr.cpp
	cd test && make
