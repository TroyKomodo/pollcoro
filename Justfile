opt := "Debug"
address_sanitizer := "OFF"

configure:
    cmake -B build -GNinja -DCMAKE_BUILD_TYPE:STRING={{opt}} -DADDRESS_SANITIZER:BOOL={{address_sanitizer}} -Wno-dev

build: configure
    cmake --build build --config {{opt}}

alias fmt := format

format:
    git ls-files --cached --others --exclude-standard '*.cc' '*.h' '*.hpp' '*.cpp' '*.cppm' | while IFS= read -r f; do [ -f "$f" ] && printf '%s\0' "$f"; done | xargs -0 -r clang-format -i

lint:
    run-clang-tidy -p build -quiet -header-filter='.*' '^(?!.*/build/_deps/).*$'

clean:
    rm -rf build
