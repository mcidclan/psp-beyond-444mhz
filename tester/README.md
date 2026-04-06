# Experimental PSP Overclock Stress Tester

This stress tester determines the maximum value, with a small margin, of the frequency supported by your PSP, and writes it to the `ms:/overconfig.txt` file.

## Usage

Before starting, make sure that no overclock plugin is enabled. It is best to start from a fresh reboot. Ideally, you should perform a reset by holding SELECT + START + △ (Triangle) + □ (Square) while powering on the device before and after using this program.

Copy the EBOOT and kcall.prx to the same folder in your `GAME` folder, then just run the program as any other homebrew. Press `Triangle` to start the process, wait for the status `RUNNING`, and let the frequency increase until the device freezes and shuts down. You can cancel the process by pressing `Triangle` at any time, then wait for the status `STOPPED`.

As the program could crash at any time after reaching a certain limit, you'll need to check the value in `ms:/overconfig.txt` to make sure it matches your observations during stress testing.

Note: Keep in mind that it still needs improvement to get a more precise value.

## Disclaimer

This project and code are provided as-is without warranty. Users assume full responsibility for any implementation or consequences. Use at your own discretion and risk

*m-c/d*
