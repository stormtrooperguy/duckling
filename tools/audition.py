#!/usr/bin/env python3
"""Audition audio files interactively; keep, replay, or trash each one.

Walks a folder of audio files (.wav / .mp3 / .m4a / .flac / .aac), plays each
through afplay (macOS built-in), and prompts you for a verdict. Trashed files
are MOVED into a sibling _trash/ folder rather than deleted, so you can review
your decisions or restore mistakes by moving files back.

Usage:
    python3 tools/audition.py /path/to/sounds/
    python3 tools/audition.py /path/to/sounds/ --trash /custom/trash/dir
    python3 tools/audition.py /path/to/sounds/ --start 100        # resume at file 100
    python3 tools/audition.py /path/to/sounds/ --pattern '*.wav'  # filter

Controls (per file):
    Enter, k   keep this file (default)
    d          move to trash folder
    r          replay the current file
    s          skip — no change, no verdict (same as keep for now)
    q          quit immediately (already-trashed files stay trashed)
    ?          show this help
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

AUDIO_EXTS = {".wav", ".mp3", ".m4a", ".flac", ".aac", ".aiff", ".aif"}


def play(path: Path) -> None:
    """Play a file with afplay. Blocks until the clip ends."""
    subprocess.run(["afplay", str(path)], check=False)


def confirm_quit(progress: str) -> bool:
    return input(f"Quit at {progress}? [y/N]: ").strip().lower() in ("y", "yes")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("folder", help="Folder of audio files to audition")
    parser.add_argument(
        "--trash",
        default=None,
        help="Trash folder (defaults to <folder_parent>/<folder_name>_trash/)",
    )
    parser.add_argument(
        "--start",
        type=int,
        default=1,
        help="1-based index of the file to start at (handy for resuming)",
    )
    parser.add_argument(
        "--pattern",
        default=None,
        help="Glob pattern to filter (e.g. '*.wav'); default = all audio extensions",
    )
    args = parser.parse_args()

    folder = Path(args.folder).resolve()
    if not folder.is_dir():
        sys.exit(f"Not a directory: {folder}")

    if args.trash:
        trash = Path(args.trash).resolve()
    else:
        trash = folder.parent / f"{folder.name}_trash"
    trash.mkdir(parents=True, exist_ok=True)

    if args.pattern:
        files = sorted(folder.glob(args.pattern))
    else:
        files = sorted(
            f
            for f in folder.iterdir()
            if f.is_file()
            and not f.name.startswith(".")
            and f.suffix.lower() in AUDIO_EXTS
        )

    if not files:
        sys.exit(f"No audio files found in {folder}")

    if args.start < 1 or args.start > len(files):
        sys.exit(f"--start {args.start} out of range (have {len(files)} files)")

    print(f"\nAuditioning {len(files)} files from {folder}")
    print(f"Trash folder:  {trash}")
    print(f"Starting at:   #{args.start}\n")
    print("Controls: Enter=keep  d=delete  r=replay  s=skip  q=quit  ?=help\n")

    kept = trashed = skipped = 0
    last_index = args.start

    try:
        for i, f in enumerate(files[args.start - 1 :], start=args.start):
            last_index = i
            progress = f"[{i}/{len(files)}]"
            size_kb = f.stat().st_size / 1024
            print(f"{progress} {f.name}  ({size_kb:.1f} KB)")

            replay_count = 0
            while True:
                play(f)
                if replay_count == 0:
                    prompt = "  verdict [K/d/r/s/q]: "
                else:
                    prompt = f"  (replay #{replay_count}) verdict [K/d/r/s/q]: "
                choice = input(prompt).strip().lower()

                if choice in ("", "k", "keep"):
                    kept += 1
                    print("  -> KEEP\n")
                    break
                if choice in ("d", "del", "delete", "trash"):
                    target = trash / f.name
                    if target.exists():
                        # Name collision — append a numeric suffix
                        n = 1
                        while (trash / f"{f.stem}_{n}{f.suffix}").exists():
                            n += 1
                        target = trash / f"{f.stem}_{n}{f.suffix}"
                    shutil.move(str(f), str(target))
                    trashed += 1
                    print(f"  -> TRASHED to {target.name}\n")
                    break
                if choice in ("r", "replay"):
                    replay_count += 1
                    continue
                if choice in ("s", "skip"):
                    skipped += 1
                    print("  -> SKIP (no change)\n")
                    break
                if choice in ("q", "quit"):
                    if confirm_quit(progress):
                        raise SystemExit(0)
                    continue
                if choice in ("?", "h", "help"):
                    print("    Enter, k   keep this file (default)")
                    print("    d          move to trash folder")
                    print("    r          replay the current file")
                    print("    s          skip — same as keep, just doesn't decide")
                    print("    q          quit (trashed files stay trashed)")
                    continue
                print(f"  unknown command: {choice!r} — type ? for help")

    except KeyboardInterrupt:
        print(f"\n\nInterrupted at {last_index}/{len(files)}")
    except SystemExit:
        print(f"\nStopped at {last_index}/{len(files)}")
        raise
    finally:
        print(f"\nSummary: kept={kept}  trashed={trashed}  skipped={skipped}")
        if trashed:
            print(f"Trashed files are in {trash}")
            print("Restore any mistakes by moving them back into the source folder.")
        if last_index < len(files):
            next_start = last_index + (1 if last_index == args.start else 0)
            print(f"To resume: --start {next_start}")


if __name__ == "__main__":
    main()
