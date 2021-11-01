set -eu -o pipefail

make -j`nproc`

sudo sh make_disk.sh

sh qemu.sh
