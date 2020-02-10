if [ -e build/ ]; then
    rm -rm build/
fi
mkdir -p build/
gcc -m32 -o build/trafficsim -I /u/OSLab/jjs231/linux-2.6.23.1/include/ trafficsim.c -lm
gcc -m32 -o build/trafficsim-mutex -I /u/OSLab/jjs231/linux-2.6.23.1/include/ trafficsim-mutex.c -lm
gcc -m32 -o build/trafficsim-strict-order -I /u/OSLab/jjs231/linux-2.6.23.1/include/ trafficsim-strict-order.c -lm
