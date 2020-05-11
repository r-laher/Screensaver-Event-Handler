# Screensaver-Event-Handler
Linux Kernel Module - displays Boston CITGO screensaver after 15 seconds of no keyboard/mouse input to framebuffer. <br />
Utilized QEMU & VNC framebuffer. <br />
Modified driver functions from keyboard.c and evdev.c as explicitely stated in comments of myscreensaver.c file. <br />

Developers:<br />
Rebecca Laher - rlaher@bu.edu<br />
Elise DeCarli - decarli@bu.edu<br />
<br />
To run:<br />
Use 'make' to generate .ko files<br />
echo 0 > /sys/class/vtconsole/vtcon1/bind<br />
mknod /dev/myscreensaver c 61 0 <br />
insmod /root/myscreensaver.ko<br />
rmmod myscreensaver to remove<br />
<br />
Bugs:<br />
May experience page domain fault if trying to remove module while screensaver is actively drawing. Move mouse on framebuffer to produce black screen before running rmmod to prevent fault.
