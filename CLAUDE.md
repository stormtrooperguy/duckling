# duckling — Claude Code guidance

## Rules

- **Keep README.md in sync**: Any time code changes are made to `esp32wifiweb.ino`, update `README.md` to reflect those changes before considering the task complete. This includes new features, changed defaults, updated behavior, new configuration constants, and corrected tables.

- **Keep the web preview emulator in sync**: Any time changes to `esp32wifiweb.ino` affect the HTML, CSS, embedded JS, JSON status shape, HTTP routes, or the button arrays (emotes/actions/eye colors), update `tools/preview.py` to match. The emulator is the way to preview UI changes without flashing the device, so it must produce the same output as the firmware. After updating, smoke-test by running `python3 tools/preview.py` and verifying the page renders and `/status` / `/maestro/<x>` endpoints respond correctly.
