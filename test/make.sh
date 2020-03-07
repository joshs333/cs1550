if [ -e build/ ]; then
    rm -rf build/
fi
mkdir build/

if [ ! -z "$1" ]; then
  gcc -m32 -o build/museumsim museumsim.c -lpthread -D USE_PTHREAD
else
  gcc -m32 -o build/museumsim museumsim.c 
fi
