# rpi-audioplayer-2
Buildroot based UPnP/Openhome MediaPlayer for Raspberry Pi 64Bit using mpd and upmpdcli.

# Building

## RPI3
```
cd buildroot
make PLATFORM=RPI3_64
```

## RPI4
```
cd buildroot
make PLATFORM=RPI4_64
```

## Flashing
Please change /dev/sdX with the correct mounting point of your sd card.
```
cd /work/rpi-audioplayer-2
sudo umount /dev/sdX?*
sudo dd if=RPI3_64-sdcard.img of=/dev/sdX bs=1M conv=fdatasync status=progress
```

# Releases

## 2025.12.19 

Buildroot: 2025.11
upmpdcli:  1.9.7
mpd:       0.24.6

- 64Bit builds only for RPI3 and RPI4
- Encode IP address into renderers name

# Infromations

## Console log
```
sudo picocom -b 115200 /dev/ttyUSB0
```

## Tag release
```
git tag -a release_20251219 -m "Release 2025.12.19 Buildroot 2025.11"
git push --tags
```

## Tips & Tricks

### WPA password as hash
```
wpa_passphrase
```
/etc/wpa_supplicant.conf
```
country=CH
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1
network={
        ssid="SNK-2.4G"
        psk=YOURPSK
        key_mgmt=WPA-PSK
}
```

### Mount boot partion
```
mkdir /tmp/boot
mount /dev/mmcblk0p1 /tmp/boot/
```


