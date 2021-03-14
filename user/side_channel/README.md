# Side Channel Demos

## Build side channel binary

``` shell
$ cd nvleak
$ make
```

## Format PMEM devices

1. Create fsdax and devdax pmem devices

``` shell
$ ./scripts/machine/optane.sh ndctl fsdevdax
```

2. Install or build e2fsprogs: the scripts uses the `fast_commit` feature from e2fsprogs, which is available since `v1.46.4`. If your Linux distro's `e2fsprogs` version is lower than `v1.46.4`, then build it as follows

``` shell
$ cd $YOUR_PATH_TO_BUILD
$ git clone git@github.com:tytso/e2fsprogs.git e2fsprogs-1.46.4
$ cd e2fsprogs-1.46.4
$ git checkout v1.46.4
$ mkdir build && cd build
$ ../configure
$ make -j $(nrpoc)
```

3. Format and mount DRAM and PMEM devices

``` shell
$ sudo -i su # the mount.sh script requires root privilege
$ cd user/side_channel
$ ./setup/mount.sh
```

## Notes

### Check file fragmentation

``` shell
$ filefrag -v
```
