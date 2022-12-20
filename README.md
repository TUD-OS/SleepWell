# Usage

## Setup (first use only)

```sh
cd buildroot
make BR2_EXTERNAL="$(pwd)/../mwait_module" qemu_x86_64_defconfig
```

In ```.config``` set:
* BR2_PACKAGE_MWAIT_MODULE=y
* BR2_ROOTFS_OVERLAY="../mwait_module/overlay"

```sh
make BR2_JLEVEL="$(nproc)" all
```

## Compilation

```sh
rm -r output/build/mwait_module-0.1
make BR2_JLEVEL="$(nproc)" all
```

## Starting Qemu

```sh
qemu-system-x86_64 -M pc -cpu host -kernel output/images/bzImage -drive file=output/images/rootfs.ext2,if=virtio,format=raw \
    -append 'root=/dev/vda console=ttyS0' -net nic,model=virtio -nographic -serial mon:stdio -net user -smp $(nproc) -enable-kvm
```