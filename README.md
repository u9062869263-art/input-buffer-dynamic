# Input Buffer Dynamic (Geode mod, Windows)

Press **Tab** in game to open the settings.

## Build without installing anything (GitHub Actions)
1. Create a new GitHub repository and upload everything in this folder (keep the `.github` folder).
2. Open the **Actions** tab, run **Build Geode Mod**.
3. Download the artifact "Build Output" and unzip it. It contains the `.geode` file.
4. Put the `.geode` into `Geometry Dash/geode/mods/` (or Geode menu -> Mods -> install from file).

## Build locally
Install the Geode CLI, run `geode sdk install`, then `geode build` in this folder.

## Before building
- In `mod.json` set `"geode"` to the Geode version you have installed (shown in the Geode menu), and change `id` and `developer`.
- If the build fails, send the error text back; the likeliest spots are the `dispatchKeyboardMSG` signature and `openSettingsPopup` in `src/main.cpp`.

## How it works
- Cube/Ball/Robot/Spider: your input stays held for the buffer time after you let go.
- Ship/UFO/Swing: taps shorter than the min hold are stretched.
- Wave: min hold + min gap between inputs.
- Grid snap rounds all delays up to whole physics ticks.
- Not compatible with Click Between Frames; disable one of them.
