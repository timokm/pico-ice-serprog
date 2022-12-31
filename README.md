pico-ice-serprog
===
serprog protocol implementation for raspberry pico-ice
https://github.com/tinyvision-ai-inc/pico-ice

This works directly with
* flashrom [https://github.com/flashrom/flashrom]
* iceprog-serprog [https://github.com/Droid-MAX/iceprog-serprog]
    iceprog-serprog can be build for linux and windows

**usage with iceprog-serprog**
```
./iceprog rgb_blink.bin
```
or if there are more tty devices
```
./iceprog rgb_blink.bin -d /dev/ttyACM1
```



