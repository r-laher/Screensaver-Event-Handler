# Screensaver-Event-Handler
Linux Kernel Module - displays Boston CITGO screensaver after 15 seconds of no keyboard/mouse input to framebuffer.
Utilized QEMU & VNC framebuffer

Developers:
Rebecca Laher - rlaher@bu.edu
Elise DeCarli - decarli@bu.edu

To run:
Use 'make' to generate .ko files
echo 0 > /sys/class/vtconsole/vtcon1/bind
mknod /dev/myscreensaver c 61 0 
insmod /root/myscreensaver.ko
rmmod myscreensaver to remove

Bugs:
May experience page domain fault if trying to remove module while screensaver is actively drawing. Move mouse on framebuffer to produce black screen before running rmmod to prevent fault.
