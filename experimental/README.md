# Experimental PSP Overclock Plugin

## Description
This is an experimental project for technical purposes only, intended for developers and advanced users. Stability is not guaranteed, use with caution.  

The current target frequency is set to 471 MHz (defined by `THEORETICAL_FREQUENCY`), but this is not guaranteed to be achieved on all PSPs. Some will comfortably exceed 407 MHz, others will be around that mark, and some may fall below it.

## Usage

### Prerequisites
Before using this plugin, make sure to:
- Install the pspsdk and related toolchain
- Disable all previous versions or similar plugins
- Remove any existing overclocking >333MHz code from your application

### Controls
Press **L_TRIGGER + R_TRIGGER + NOTE** (or alternatively **L_TRIGGER + R_TRIGGER + CIRCLE**) to toggle between 333MHz and the `THEORETICAL_FREQUENCY` target, or the frequency set in the ms0:/overconfig.txt file.

### Visual Feedback
- **333MHz (standard)**: White square on green background
- **Custom > 333MHz (overclocked)**: Red square on white background
The plugin auto-starts at 333MHz. In most cases, you should see the square a few seconds after the game/homebrew boots.

### ms0:/overconfig.txt
If the file doesn't exist, the plugin will target the `THEORETICAL_FREQUENCY` for the overclock frequency. So you must set a value between 333 and the `THEORETICAL_FREQUENCY` in that file.  

You can create that file manually, but it is **HIGHLY** recommended to use the overclock stress tester provided with this project, and let it create the file at the root of the memory stick for you with the maximum frequency supported by your PSP.  

In case you want to write the value by hand, start from 333 and increase in steps of multiples of 5.

## Compatibility and Testing

### Testing Methodology
After experiencing instability during testing, it is preferable to remove the battery and any other power source in order to start fresh with a clean test. Additionally, when restarting, you may perform a reset using SELECT + START + △ (Triangle) + □ (Square).

### Overclock Stress Tester
See the `tester` folder of this repository for more information.

| Model              | Status            |
|--------------------|-------------------|
| PSP 2000 and 3000  | Tested            |
| PSP 1000           | Tested            |
| PSP Go             | Tested            |
| PSP Street (E1000) | Not supported yet |
| ePSP (Vita)        | Not working       |

#### Special Note Related to PSP Street and Latest Models

PSP Street uses `0x01221100` as the default value for the PLL register and 5 as the ratio table index. On PSP Street and those very late PSP models, `6.2f` may be used as `PLL_BASE` and `4` as `PLL_DEN`, otherwise the ratio would need to be readjusted. Keep in mind that changing `PLL_BASE` within this context will break the logic of the code, as it was written assuming the PLL base frequency to be `37` on other models. The only way to achieve valid logic across every model is to understand what `0x01221100` actually means. Without that understanding, you would just be arbitrarily tweaking values with no real insight.

That said, here are some observations and  assumptions about the PSP Street and related models:

- The ratio table does not appear to have changed.
- The `0x00` value in the last 8 bits could be related to a disabled state of the multiplier.
- Sony may have introduced additional hardware registers to control the multiplier on the Street and related models.
- The first 16 bits may be related to a clock selector.

The last three points are assumptions and still need verification.

## Build
You can build the project using `./build.sh`. This will bundle all files into `./bin/build/` ready to be copied to the root of your Memory Stick.

To track which version you've built, use `./build.sh <version>` (e.g., `./build.sh v2.4`). This generates a `note.txt` file from the template with the specified version number.

The README.md files will automatically be included in their respective directories so you have the instructions available locally.

## Contribution Guidelines

### AI-assisted development

AI tools may be used as development aids. However, the following rules apply strictly:

* All commits must be authored by a human contributor (pseudonyms are perfectly acceptable).
* The commit history must not contain any AI attribution as author or co-author.
* Contributors must fully review, understand, and validate all submitted code before opening a pull request.
* Contributors are expected to be able to explain and justify their changes during reviews.
* The contributor is responsible for ensuring that their code or changes do not break existing functionality, integrations, dependencies, documentation consistency, APIs, build processes, tests, or the overall behavior and technical integrity of the project.
*In short: AI can assist, but humans must retain full ownership of the work.*

Pull requests that include AI attribution in commits, or that are not clearly understood and validated by the contributor, will be rejected.

### License compatibility

All code submitted to this repository must be compatible with the MIT License. Dependencies or code snippets under more restrictive licenses (e.g. GPL, LGPL, proprietary) are not accepted. Contributors are responsible for verifying that any third-party code they include is under a permissive license granting at least the same level of freedom as MIT.

## Disclaimer
This project and code are provided as-is without warranty. Users assume full responsibility for any implementation or consequences. Use at your own discretion and risk

## Special Mention
Thanks to re4thewin, z2442, Acid_Snake, st1x51 and skinny for testing, to febblin and koutsie for having brought back the overclock idea into the community, which sparked my curiosity and led me to investigate and experiment further on the subject, and of course to the pioneers and to crazyc from ps2dev, without whom I would not have had the key information regarding the existence of the registers involved, and ultimately the possibility to soft-overclock itself.

*m-c/d*
