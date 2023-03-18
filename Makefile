all:
	clang++ --std=c++14 -o dskmgr src/dskmgr.cpp
	cd test && make

format:
	make execute-format FILENAME=dskmgr.cpp
	make execute-format FILENAME=basic.hpp

execute-format:
	clang-format -style=file < ./src/${FILENAME} > ./src/${FILENAME}.bak
	cat ./src/${FILENAME}.bak > ./src/${FILENAME}
	rm ./src/${FILENAME}.bak
