# MegaLEDController
IR and Bluetooth SPP controlled LED strip driver for ATmega328 MCU.

This is intended as a drop-in replacement for those cheap "analogue" 12V LED strip drivers with IR remote.
It is also intendend to be a companion for my branch of [shairport-sync](https://github.com/kwasmich/shairport-sync).


Connect via Bluetooth Serial (SPP)
----------------------------------
You can connect to the Bluetooth SPP device and issue commands via terminal.

First pair your computer with the device. On linux it goes like this:

```
sudo bluetoothctl -a
    > scan on
    > devices
    > pair 01:23:45:67:89:AB
    > trust 01:23:45:67:89:AB
    > quit
```

Where `01:23:45:67:89:AB` needs to be replaced with the actual Bluetooth MAC-address of your device.
Then start a terminal session by creating a virtual serial device and then connecting against it.

```
sudo rfcomm bind /dev/rfcomm1 01:23:45:67:89:AB 1
minicom -c on -b 9600 -D /dev/rfcomm1
```

Here you can issue commands to the device. The format is exactly the same as for colors in the web or inkscape. As it is
capable to control up to four colors the format is:

```
#RRGGBBWW
```

`RR`, `GG`, `BB` and `WW` are is the hex value of the corresponding color component in the range [0, 255]. So total red
would be `#ff000000` where total blue would be `#0000ff00` and so on.



Inner workings
--------------
This program utilizes the following features of the ATmega328 (making it impossible to implement on an Arduino):

* Timer 1 and Interrupt controlled NEC IR remote receiver and decoder
* Hardware USART for communication with the Bluetooth SPP module
* Timer 0 and Timer 2 for phase correct 8-bit PWM for 4 channels
* EEPROM for storing custom colors

