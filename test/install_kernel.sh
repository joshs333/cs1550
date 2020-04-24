scp jjs231@thoth.cs.pitt.edu:/u/OSLab/original/bzImage .
scp jjs231@thoth.cs.pitt.edu:/u/OSLab/original/System.map .
cp bzImage /boot/bzImage-devel
cp System.map /boot/System.map-devel
lilo
reboot
