qemu-system-x86_64 \
-m 128 \
-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
-drive file=/mnt/osdev_ramdisk/disk.img,format=raw,index=0,media=disk \
-boot menu=off \
-serial stdio

# -drive if=pflash,format=raw,file=./OVMF_VARS.fd \
# -drive if=pflash,format=raw,readonly=on,file=/usr/share/qemu/edk2-x86_64-code.fd \