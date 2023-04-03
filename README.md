# Usage

## Setup (first use only)

```sh
cd buildroot
make BR2_EXTERNAL="$(pwd)/../mwait_module" qemu_x86_64_defconfig
```

In ```.config``` set:
* BR2_PACKAGE_MWAIT_MODULE=y
* BR2_ROOTFS_OVERLAY="../overlay"
* BR2_PACKAGE_OVERRIDE_FILE="../buildroot_override"
* BR2_PACKAGE_HOST_LINUX_HEADERS_CUSTOM_6_0=y

```sh
make
```

## Compilation

```sh
rm -r output/build/mwait_module-0.1
make
```

## Starting Qemu

```sh
qemu-system-x86_64 -M pc -kernel output/images/bzImage -drive file=output/images/rootfs.ext2,if=virtio,format=raw \
    -append 'root=/dev/vda console=ttyS0' -net nic,model=virtio -nographic -serial mon:stdio -net user -smp $(nproc)
```