
Be sure to put the following in: `/etc/udev/rules.d/99-magfest.rules` and execute `sudo udevadm control --reload-rules && sudo udevadm trigger`

```
# MAGFest Swadge 2022
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", ATTR{idProduct}=="4004", GROUP="users", MODE="0666"
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", MODE="0664", GROUP="plugdev"
KERNEL=="ttyUSB[0-9]*", TAG+="udev-acl", TAG+="uaccess"
```



