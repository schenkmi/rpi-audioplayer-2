# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Buildroot-based UPnP/Openhome MediaPlayer for Raspberry Pi 64-bit using mpd and upmpdcli. The project produces bootable SD card images for RPI3 and RPI4.

## Build Commands

### Full Build (RPI3 or RPI4)
```bash
cd buildroot
make PLATFORM=RPI3_64    # or PLATFORM=RPI4_64
```

### Build Targets
- `make PLATFORM=RPI3_64 config` - Extract and configure buildroot
- `make PLATFORM=RPI3_64 build` - Build the system
- `make PLATFORM=RPI3_64 sdk` - Generate SDK
- `make PLATFORM=RPI3_64 install` - Copy sdcard.img and toolchain to project root
- `make PLATFORM=RPI3_64 menuconfig` - Buildroot menuconfig
- `make PLATFORM=RPI3_64 linux-menuconfig` - Kernel menuconfig
- `make PLATFORM=RPI3_64 clean` - Remove buildroot sources

### Reconfigure
```bash
make PLATFORM=RPI3_64 re-config      # Full reconfigure
make PLATFORM=RPI3_64 clean-config   # Clean and reconfigure
```

### Docker Build Environment
```bash
cd rpi-audioplayer-2
docker build -t br-docker -f docker/Dockerfile .
docker create -it --name br-docker --mount type=bind,source="$(pwd)",destination=/home/br-user/br-docker br-docker
docker start -ia br-docker
```

### ohupnpplayer (C++ UPnP Player)
```bash
# RPI cross-compile (after sourcing SDK environment)
cd ohupnpplayer/player
rm -rf .build; cmake -DMPD_RENDERER=ON -DDUMMY_RENDERER=OFF -B.build
make -j `nproc` -C .build/

# x86_64 native build
rm -rf .build; cmake -DDUMMY_RENDERER=ON -B.build
make -j `nproc` -C .build/

# Format code
clang-format --style=file:.clang-format -i <file>
```

## Code Architecture

### Repository Structure

**buildroot/** - Main build system
- `Makefile` - Orchestrates buildroot extraction, patching, and building
- `buildroot-external/` - External buildroot tree with custom configs
  - `configs/` - defconfig files for RPI3_64 and RPI4_64
  - `board/rpi3-64/` and `board/rpi4-64/` - Board-specific configs, rootfs overlay, post-image scripts
  - Rootfs overlay contains: mpd.conf, upmpdcli.conf, network interfaces, wpa_supplicant.conf, init scripts

**ohupnpplayer/** - Custom C++ UPnP/Openhome player application
- `player/` - Main application using Platinum UPnP SDK
- Uses CMake with options: `MPD_RENDERER`, `DUMMY_RENDERER`

**docker/** - Docker environment for reproducible builds

**patches/** - Buildroot package patches for libnpupnp, libupnpp, upmpdcli

### Build Flow

1. Buildroot tarball is extracted automatically
2. Patches are applied to buildroot packages
3. defconfig is loaded from buildroot-external/configs/
4. Buildroot builds toolchain, kernel, rootfs
5. post-image.sh generates final sdcard.img

### Key Components

- **mpd** (0.24.6) - Music Player Daemon
- **upmpdcli** (1.9.7) - UPnP renderer backend for mpd
- **libnpupnp/libupnpp** - UPnP stack (patched versions)
- Custom init script `S90upmpdcli-ip` encodes IP address into renderer name

## Flashing

```bash
sudo umount /dev/sdX?*
sudo dd if=RPI3_64-sdcard.img of=/dev/sdX bs=1M conv=fdatasync status=progress
```

## Serial Console

```bash
picocom -b 115200 /dev/ttyUSB0
```
