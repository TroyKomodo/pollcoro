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
    git ls-files --cached --others --exclude-standard '*.cc' '*.h' '*.hpp' '*.cpp' '*.cppm' | while IFS= read -r f; do [ -f "$f" ] && printf '%s\0' "$f"; done | xargs -0 -r clang-format -i
