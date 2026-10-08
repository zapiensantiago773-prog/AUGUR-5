"""Builds the distributable zip of an expansion pack with portable ('/') paths.
Usage: python tools/pack/build_zip.py ["<pack folder name>"]
       (default: AUGUR-5 Anthology Vol.1)   -> packs/dist/TONAL LAB - <pack> (<n> presets).zip
"""
import pathlib
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
DIST = ROOT / "packs" / "dist"


def main():
    pack = ROOT / "packs" / (sys.argv[1] if len(sys.argv) > 1 else "AUGUR-5 Anthology Vol.1")
    DIST.mkdir(parents=True, exist_ok=True)
    files = sorted(f for f in pack.rglob("*") if f.is_file())
    presets = sum(1 for f in files if f.suffix == ".augur5")
    title = pack.name if pack.name.startswith("AUGUR-5") else "AUGUR-5 " + pack.name
    out = DIST / ("TONAL LAB - %s (%d presets).zip" % (title, presets))
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in files:
            z.write(f, (pack.name + "/" + f.relative_to(pack).as_posix()))
    print("%s: %d presets, %d KB" % (out.name, presets, out.stat().st_size // 1024))


if __name__ == "__main__":
    main()
