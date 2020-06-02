cool-renderer

### Linux build
```shell
sudo apt install clang-7 libglu1-mesa-dev libc++-7-dev libc++abi-7-dev ninja-build libxi-dev

cd deps/protobuf
./autogen.sh
CC=clang CXX=clang++ ./configure CXX_FOR_BUILD=clang++ --prefix=/usr/local/ --with-pic
make
sudo make install

mkdir build
cd build
CC=clang-7 CXX=clang++-7 cmake .. -G Ninja -DCMAKE_CXX_COMPILER="clang++-7" -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -stdlib=libc++ -lc++abi -Wno-unused-command-line-argument" -DCMAKE_POSITION_INDEPENDENT_CODE=1 
ninja
```
if you get a ``builtins-generated/bytecodes not found`` error do 
```
cd build/deps/v8-cmake
./bytecode_builtins_list_generator generated/builtins-generated/bytecodes-builtins-list.h
```