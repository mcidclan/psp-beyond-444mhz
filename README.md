# Experimental Overclock targeting 555MHz

## Description
This is an experimental project for technical purposes only, intended for developers and advanced users. Stability is not guaranteed, use with caution.

## Usage

### Prerequisites
Before using this plugin, make sure to:
- Disable all previous versions or similar plugins
- Remove any existing overclocking >333MHz code from your application

### Controls
Press **L_TRIGGER + R_TRIGGER + NOTE** (or alternatively **L_TRIGGER + R_TRIGGER + CIRCLE**) to toggle between 333MHz and 555MHz, or the frequency set in the ms0:/overconfig.txt file.

### Visual Feedback
- **333MHz (standard)**: White square on green background
- **Custom > 333MHz (overclocked)**: Red square on white background
The plugin auto-starts at 333MHz. In most cases, you should see the square a few seconds after the game/homebrew boots.

### ms0:/overconfig.txt
If the file doesn't exist, the plugin will target 555MHz for the overclock frequency. So you must set a value between 333 and 555 in that file.  

You can create that file manually, or use the overclock stress tester provided with this project, and let it create the file at the root of the memory stick for you with the maximum frequency supported by your PSP.

## Compatibility and Testing

### Testing Methodology
After experiencing instability during testing, it is preferable to remove the battery and any other power source in order to start fresh with a clean test. Additionally, when restarting, you may perform a reset using SELECT + START + △ (Triangle) + □ (Square).

### Overclock Stress Tester
See the `tester` folder of this repository for more information.

| Model | Status |
|---|---|
| PSP 2000 and 3000 | Tested |
| PSP 1000 | Tested |
| PSP Go | Not tested |
| PSP Street (E1000) | Not supported yet |

## Build
You can build the project using `./build.sh`. This will bundle all files into `./bin/build/` ready to be copied to the root of your Memory Stick.

To track which version you've built, use `./build.sh <version>` (e.g., `./build.sh v2.4`). This generates a `note.txt` file from the template with the specified version number.

The README.md files will automatically be included in their respective directories so you have the instructions available locally.

## Disclaimer
This project and code are provided as-is without warranty. Users assume full responsibility for any implementation or consequences. Use at your own discretion and risk

## Special Mention
Thanks to re4thewin, z2442, st1x51 and skinny for testing, and to koutsie whose claims about overclocking possibilities motivated me to investigate and experiment on the subject.

*m-c/d*
