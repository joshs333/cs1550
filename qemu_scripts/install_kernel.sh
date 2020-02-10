scp jjs231@thoth.cs.pitt.edu:/u/OSLab/jjs231/linux-2.6.23.1/System.map .
scp jjs231@thoth.cs.pitt.edu:/u/OSLab/jjs231/linux-2.6.23.1/arch/i386/boot/bzImage .
cp bzImage /boot/bzImage-devel
cp System.map /boot/System.map-devel
lilo
reboot
