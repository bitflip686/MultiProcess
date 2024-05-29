sudo mount -o loop dev_kernel_grub.img /mnt/floppy
sudo rm /mnt/floppy/kernel.elf
sudo cp kernel.elf /mnt/floppy/
sleep 1s
sudo umount /mnt/floppy/
