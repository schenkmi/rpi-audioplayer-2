# Build

## RPI

### Prepare
```
cd /opt/aarch64-buildroot-linux-gnu_sdk-buildroot
source environment-setup
```

### Full build
```
cd /work/rpi-audioplayer-2/ohupnpplayer/player
rm -rf .build; cmake -DMPD_RENDERER=ON -DDUMMY_RENDERER=OFF -B.build; make -j `nproc` -C .build/
```

### Just build and copy
```
make -j `nproc` -C .build/
scp -O .build/ohupnpplayer root@192.168.0.152:/usr/bin
```

## x86_64
```
cd /work/rpi-audioplayer-2/ohupnpplayer/player
rm -rf .build; cmake -DDUMMY_RENDERER=ON -B.build; make -j `nproc` -C .build/
```

# Misc

## Format source code
```
clang-format --style=file:.clang-format -i player/OHUPnPPlayer.cpp
clang-format --style=file:../.clang-format -i control/*.cpp
```

## Memory leak tracking
```
valgrind --leak-check=full ./.build/ohupnpplayer
```