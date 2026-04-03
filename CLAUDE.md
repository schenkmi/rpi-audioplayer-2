# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Buildroot-based UPnP/Openhome MediaPlayer for Raspberry Pi 64-bit using mpd and upmpdcli. The project produces bootable SD card images for RPI3 and RPI4.

**Core packages:**
- mpd 0.24.8 (Music Player Daemon)
- upmpdcli 1.9.15 (UPnP renderer backend)
- Custom ohupnpplayer (OpenHome/UPnP/DLNA player with Platinum SDK)

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

### Partial Rebuilds
```bash
# Clean and rebuild specific packages
make O=../toolchain-rpi3_64 BR2_EXTERNAL=../buildroot-external upmpdcli-dirclean
make O=../toolchain-rpi3_64 BR2_EXTERNAL=../buildroot-external upmpdcli-rebuild
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

# x86_64 native build (for testing)
rm -rf .build; cmake -DDUMMY_RENDERER=ON -B.build
make -j `nproc` -C .build/

# Format code
clang-format --style=file:.clang-format -i <file>

# Memory leak tracking (x86_64)
valgrind --leak-check=full ./.build/ohupnpplayer
```

## Code Architecture

### Repository Structure

**buildroot/** - Main build system
- `Makefile` - Orchestrates buildroot extraction, patching, and building (NPROC jobs)
- `buildroot-2026.02.tar.xz` - Buildroot source tarball
- `buildroot-external/` - External buildroot tree (name: AP2)
  - `configs/` - defconfig files (rpi3_64_defconfig, rpi4_64_defconfig)
  - `board/rpi3-64/` and `board/rpi4-64/` - Board-specific:
    - `configs/linux_defconfig` - Kernel defconfig
    - `genimage.cfg.in` - Disk image template
    - `post-image.sh` - Final image generation (adds device tree overlays for HiFiBerry Digi+ Pro)
    - `rootfs-overlay/etc/` - Runtime configuration files
- `patches/` - Buildroot package patches (upmpdcli)

**ohupnpplayer/** - Custom C++ UPnP/Openhome player application
- `player/` - Main application
  - `OHUPnPPlayer.cpp` - Entry point, UPnP device hosting, config management (XML-based)
  - `control/` - UPnP device implementations
    - `MyUPnPRenderer.{cpp,h}` - DMR (Digital Media Renderer) with AVTransport, RenderingControl
    - `MyOHPlaylist.{cpp,h}` - OpenHome Playlist device with playlist/volume control
  - `common/` - Shared utilities (Dispatcher, MediaItem, MetaData, MyLogger, UPnPUtils)
  - `platinum/` - Platinum UPnP SDK (third-party)
- CMake options: `MPD_RENDERER`, `DUMMY_RENDERER`, `FFMPEGD_RENDERER`

**docker/** - Docker environment for reproducible builds (debian:trixie base)

**doc/** - Additional documentation and tips

### Build Flow

1. `config` target: Extracts buildroot tarball, applies patches from `patches/`
2. Patches update upmpdcli to 1.9.7 (meson-based build) and libnpupnp/libupnpp
3. defconfig loaded from `buildroot-external/configs/`
4. Buildroot builds:
   - AArch64 toolchain with C++ support
   - Linux kernel 6.12 (Raspberry Pi bcm2711/bcm2710)
   - Root filesystem with mpd, upmpdcli, alsa-utils, dropbear, wpa_supplicant
5. `post-image.sh`:
   - Configures device tree overlays (hifiberry-digi-pro, I2S)
   - Generates sdcard.img via genimage (boot + rootfs partitions)

### Configuration Files (rootfs-overlay)

**mpd.conf:**
- ALSA output to hw:0,0 (My DAC)
- soxr samplerate converter
- 24 MB audio buffer
- Unix socket at /var/lib/mpd/socket

**Init Scripts:**
- `S90upmpdcli-ip` - Dynamically sets friendlyname/ohproductroom in upmpdcli.conf using IP address

### UPnP/OpenHome Architecture

The ohupnpplayer creates two UPnP devices:

1. **DMR (Digital Media Renderer)** - `MyUPnPRenderer`
   - AVTransport service (Play, Pause, Stop, Next, Previous, Seek, SetAVTransportURI)
   - RenderingControl service (Volume, Mute)

2. **OpenHome Playlist** - `MyOHPlaylist`
   - Playlist service (Insert, Delete, Read, Seek)
   - Transport control (Play, Pause, Stop, Next, Previous)
   - Volume control (SetVolume, Mute, VolumeInc/Dec)

Both devices share a common renderer interface (`IRenderer`) with pluggable backends:
- `MPDRenderer` - MPD client backend
- `DummyRenderer` - Testing/stub backend

Configuration is persisted in `~/.ohupnpplayer/ohupnpcfg.xml` (UUIDs, friendly names).

### Hardware Support

**RPI3_64:**
- Kernel: bcm2711 defconfig
- Device trees: bcm2710-rpi-3-b, bcm2710-rpi-3-b-plus, bcm2710-rpi-cm3

**RPI4_64:**
- Kernel: bcm2711 defconfig
- Device trees for RPI4 variants

**Audio HATs (via device tree overlays):**
- HiFiBerry Digi+ Pro (default, SPDIF)
- HiFiBerry DAC+
- Configured in `post-image.sh`

## Flashing

```bash
sudo umount /dev/sdX?*
sudo dd if=RPI3_64-sdcard.img of=/dev/sdX bs=1M conv=fdatasync status=progress
```

First boot: Ethernet (eth0) configured via DHCP. WiFi via wpa_supplicant.conf.

## Debugging

### Serial Console
```bash
picocom -b 115200 /dev/ttyUSB0
```

### Remote Access
- dropbear SSH server (root/root)
- Access via Ethernet or WiFi

### MPD Control
```bash
mpc clear
mpc add http://<server>:<port>/track.flac
mpc play
watch -n1 mpc
```

### Modify Friendly Name
```bash
vi /etc/upmpdcli.conf
# Edit: friendlyname, ohproductroom
```

### Mount Boot Partition (on device)
```bash
mkdir /tmp/boot
mount /dev/mmcblk0p1 /tmp/boot/
vi /tmp/boot/config.txt  # Edit device tree overlays
```

## Release Tagging

```bash
git tag -a release_20260403 -m "Release 2026.04.03 Buildroot 2026.02"
git push --tags
```
