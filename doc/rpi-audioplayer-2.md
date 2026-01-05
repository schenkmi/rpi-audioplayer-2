


# Serial
```
picocom -b 115200 /dev/ttyUSB0
```

## Build

### RPI3
```
cd /work/rpi-audioplayer-2/buildroot
make PLATFORM=RPI3
```

### RPI4
```
cd /work/rpi-audioplayer-2/buildroot
make PLATFORM=RPI4
```

## Reconfig
```
make PLATFORM=RPI4 re-config
```



## Flashing
Please change /dev/sdX with the correct mounting point of your sd card.
```
cd /work/rpi-audioplayer-2
sudo umount /dev/sdX?*
sudo dd if=buildroot/toolchain-rpi3/images/sdcard.img of=/dev/sdX bs=1M conv=fdatasync status=progress
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
        psk=d4faa7fbcc7410b21b0e507823c1b0ff5cba171b151af4f26cc1a248485b025c
        key_mgmt=WPA-PSK
}
```

### Mount boot partion
```
mkdir /tmp/boot
mount /dev/mmcblk0p1 /tmp/boot/
```
vi /tmp/boot/config.txt


dtparam=i2s=on
dtoverlay=hifiberry-digi-pro
audio=off

### Change name
```
vi /etc/upmpdcli.conf
```
```
friendlyname = RPI3-Schlafzimmer
ohproductroom = RPI3-Schlafzimmer
```

alsa

aplay -l



/etc/mpd.conf
audio_output {
type "alsa"
name "alsa"
device "hw:1,0"
dop "no"
}

Because I wanted to control volume from my MPD Client.
The only two choices of mixer for Digi+ Pro were None and Software.
(I think it is because Hardware mixer does not make much sense to Digi+ as it is not a DAC).
Only Software allows MPD Clients to control volume.

Yes, I saved settings before checking mpd.conf.

Original autido_output section of mpd.conf. (Mixer Type was “None”)
audio_output {
type “alsa”
name “alsa”
device “hw:1,0”
dop “no”
}

After setting Mixer Type” to “Software” and “MPD Clients Volume Control” to On.
audio_output {
type “alsa”
name “alsa”
device “softvolume”
dop “no”
mixer_device “SoftMaster”
mixer_control “SoftMaster”
mixer_type “hardware”
}

https://mpd.readthedocs.io/en/latest/mpd.conf.5.html

mpc clear
mpc add http://192.168.0.42:26125/content/c2/b16/f44100/d1286470904272590256-co05FA7A75DFB6345F.flac
mpc play
mpc current
mpc stats

watch -n1 mpc

https://github.com/mariam-elshakafi/MP3-RaspberryPi-Buildroot
https://www.jackcampbellsounds.com/2020/09/13/developingembeddedlinuxaudioapplications15.html

upmpdcli

https://patchwork-proxy.ozlabs.org/project/buildroot/patch/20251122222327.1913289-1-bernd@kuhls.net/
https://patchwork-proxy.ozlabs.org/project/buildroot/patch/20251122222327.1913289-2-bernd@kuhls.net/
https://patchwork-proxy.ozlabs.org/project/buildroot/patch/20251122222327.1913289-3-bernd@kuhls.net/


diff -Naur buildroot-2025.02.9-org/package/libnpupnp buildroot-2025.02.9/package/libnpupnp > buildroot-external/patches/0001-package-libnpupnp-6.2.3.patch
diff -Naur buildroot-2025.02.9-org/package/libupnpp buildroot-2025.02.9/package/libupnpp > buildroot-external/patches/0001-package-libupnpp-1.0.3.patch
diff -Naur buildroot-2025.02.9-org/package/upmpdcli buildroot-2025.02.9/package/upmpdcli > buildroot-external/patches/0001-package-upmpdcli-1.9.7.patch


https://stackoverflow.com/questions/64233932/applying-patch-kept-in-br2-external-to-a-buildroot-package

Buildroot

make O=../toolchain-rpi3 BR2_EXTERNAL=../buildroot-external upmpdcli-dirclean
make O=../toolchain-rpi3 BR2_EXTERNAL=../buildroot-external make upmpdcli-rebuild

make O=../toolchain-rpi3 BR2_EXTERNAL=../buildroot-external libnpupnp-dirclean libupnpp-dirclean upmpdcli-dirclean
make O=../toolchain-rpi3 BR2_EXTERNAL=../buildroot-external



make O=../toolchain-rpi3_64 BR2_EXTERNAL=../buildroot-external upmpdcli-dirclean

