if [ -e build/ ]; then
    rm -rf build/
fi
mkdir build/

gcc -m32 -o build/museumsim museumsim.c
