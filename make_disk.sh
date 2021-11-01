mkdir -p /mnt/osdev_ramdisk
mkdir -p /mnt/osdev_install
mkdir -p /mnt/osdev_efi

umount /mnt/osdev_efi
umount /mnt/osdev_install
losetup -d /dev/loop0
umount /mnt/osdev_ramdisk

set -eu -o pipefail


mount -t tmpfs -o size=119m tmpfs /mnt/osdev_ramdisk

dd if=/dev/zero of=/mnt/osdev_ramdisk/disk.img bs=118M count=1
chmod 777 /mnt/osdev_ramdisk/disk.img

parted /mnt/osdev_ramdisk/disk.img mktable gpt
parted /mnt/osdev_ramdisk/disk.img mkpart efi fat32 1MiB 101MiB 
parted /mnt/osdev_ramdisk/disk.img mkpart os fat16 101MiB 117MiB 

losetup -o 1048576 --sizelimit 104857600 /dev/loop0 /mnt/osdev_ramdisk/disk.img
mkdosfs -F32 /dev/loop0
mount /dev/loop0 /mnt/osdev_efi
cp -r disk_contents/efi/* /mnt/osdev_efi


umount /mnt/osdev_efi
losetup -d /dev/loop0

losetup -o 105906176 --sizelimit 16777216 /dev/loop0 /mnt/osdev_ramdisk/disk.img
mke2fs -b1024 /dev/loop0
mount /dev/loop0 /mnt/osdev_install
cp -r disk_contents/install/* /mnt/osdev_install

umount /mnt/osdev_install
losetup -d /dev/loop0

echo disk setup complete
