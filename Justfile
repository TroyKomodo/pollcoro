opt := "Debug"

configure:
    cmake -GNinja -B build -S . -DCMAKE_BUILD_TYPE={{opt}} -DCMAKE_PREFIX_PATH=./install

build: configure 
    cmake --build build --config {{opt}}

install: build
    cmake --install build --prefix ./install

clean:
    rm -rf build install

fmt:
    clang-format -i $(git ls-files '*.cc' '*.h' '*.hpp' '*.cpp')
