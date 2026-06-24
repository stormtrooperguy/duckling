#!/usr/bin/env python3
"""Renumber audio files in a folder to a contiguous 0001..NNNN sequence.

After running tools/audition.py and trashing bad clips, the source folder
has gaps in its numbering (e.g. 0001, 0003, 0004, 0007...). This script
walks the folder in sorted order and renames each file to fill the gaps:

    0001.wav, 0003.wav, 0004.wav, 0007.wav
      -> 0001.wav, 0002.wav, 0003.wav, 0004.wav

The numbering scheme matches what the YX5200 / DFPlayer expects in /mp3/,
and is also the natural sequence for the DFPlayer Pro's onboard storage.

Each file's extension is preserved (you can mix .wav and .mp3 in the same
folder and it'll renumber them in sorted order).

Usage:
    python3 tools/renumber.py /path/to/sounds/             # do it
    python3 tools/renumber.py /path/to/sounds/ --dry-run   # show plan only
    python3 tools/renumber.py /path/to/sounds/ --width 5   # 00001.wav padding

Safety:
- Renames are performed in sorted order; each new index is <= the old
  index, so we never collide with an unrenamed file.
- --dry-run prints the rename plan without touching anything.
- Skips files whose new name equals the old name (no-op).
"""

import argparse
import sys
from pathlib import Path

AUDIO_EXTS = {".wav", ".mp3", ".m4a", ".flac", ".aac", ".aiff", ".aif"}


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("folder", help="Folder of audio files to renumber")
    parser.add_argument(
        "--width",
        type=int,
        default=4,
        help="Number of digits in the new filenames (default 4, so 0001..9999)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the rename plan without modifying anything",
    )
    args = parser.parse_args()

    folder = Path(args.folder).resolve()
    if not folder.is_dir():
        sys.exit(f"Not a directory: {folder}")

    files = sorted(
        f
        for f in folder.iterdir()
        if f.is_file()
        and not f.name.startswith(".")
        and f.suffix.lower() in AUDIO_EXTS
    )
    if not files:
        sys.exit(f"No audio files found in {folder}")

    if 10**args.width <= len(files):
        sys.exit(
            f"--width {args.width} can't represent {len(files)} files; "
            f"try --width {len(str(len(files)))}"
        )

    plan = []
    for i, f in enumerate(files, start=1):
        new_name = f"{i:0{args.width}d}{f.suffix.lower()}"
        plan.append((f, folder / new_name))

    # Pretty-print the plan
    changes = [(old, new) for old, new in plan if old.name != new.name]
    print(f"Found {len(files)} audio files in {folder}")
    print(f"Renames needed: {len(changes)}")
    print(f"No-ops (already correct): {len(files) - len(changes)}")

    if not changes:
        print("\nNothing to do — files are already contiguously numbered.")
        return

    print("\nPlan (first 10 and last 5):")
    for old, new in changes[:10]:
        print(f"  {old.name}  ->  {new.name}")
    if len(changes) > 15:
        print(f"  ... ({len(changes) - 15} more) ...")
    for old, new in changes[-5:]:
        if (old, new) not in changes[:10]:
            print(f"  {old.name}  ->  {new.name}")

    if args.dry_run:
        print("\n--dry-run: not modifying anything.")
        return

    print(f"\nExecuting {len(changes)} renames...")
    for old, new in plan:
        if old.name == new.name:
            continue
        # Should never collide because new index <= old index for each file
        # in sorted order, and we've already renamed earlier files.
        if new.exists():
            sys.exit(
                f"Unexpected collision: {new.name} already exists. "
                "Aborting to avoid data loss. Investigate manually."
            )
        old.rename(new)
    print("Done.")


if __name__ == "__main__":
    main()
