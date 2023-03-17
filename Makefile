all:
	clang++ --std=c++14 -o dskmgr src/dskmgr.cpp src/bas2txt.cpp
	cd test && make

format:
	clang-format -style=file < ./src/dskmgr.cpp > ./src/dskmgr.cpp.bak
	cat ./src/dskmgr.cpp.bak > ./src/dskmgr.cpp
	rm ./src/dskmgr.cpp.bak
	clang-format -style=file < ./src/bas2txt.cpp > ./src/bas2txt.cpp.bak
	cat ./src/bas2txt.cpp.bak > ./src/bas2txt.cpp
	rm ./src/bas2txt.cpp.bak
