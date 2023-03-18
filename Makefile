all:
	clang++ --std=c++14 -o dskmgr src/dskmgr.cpp
	cd test && make

format:
	clang-format -style=file < ./src/dskmgr.cpp > ./src/dskmgr.cpp.bak
	cat ./src/dskmgr.cpp.bak > ./src/dskmgr.cpp
	rm ./src/dskmgr.cpp.bak
	clang-format -style=file < ./src/basic.hpp > ./src/basic.hpp.bak
	cat ./src/basic.hpp.bak > ./src/basic.hpp
	rm ./src/basic.hpp.bak
