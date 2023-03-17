all:
	clang++ -o dskmgr src/dskmgr.cpp src/bas2txt.cpp
	cd test && make
