mkdir -p sub
# Copy OSLab files to repo
cp /u/OSLab/jjs231/linux-2.6.23.1/kernel/sys.c linux-2.6.23.1/kernel/sys.c
cp linux-2.6.23.1/arch/i386/kernel/syscall_table.S sub/syscall_table.S
cp linux-2.6.23.1/include/asm/unistd.h sub/unistd.h

# Copy Repo files to the submission
cp linux-2.6.23.1/kernel/sys.c sub/sys.c
cp linux-2.6.23.1/arch/i386/kernel/syscall_table.S sub/syscall_table.S
cp linux-2.6.23.1/include/asm/unistd.h sub/unistd.h
cp test/sem.h sub/sem.h
cp project1.pdf sub/project1.pdf

# Create Zip Fil
cd sub
zip ../submittal.zip *

# Clean up
cd ..
rm -rf sub