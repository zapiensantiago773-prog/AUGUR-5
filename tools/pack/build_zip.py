"""Builds the distributable zip of an expansion pack with portable ('/') paths.
Usage: python tools/pack/build_zip.py            -> packs/dist/TONAL LAB - AUGUR-5 Anthology Vol.1 (500 presets).zip
"""
import pathlib
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
PACK = ROOT / "packs" / "AUGUR-5 Anthology Vol.1"
DIST = ROOT / "packs" / "dist"


def main():
    DIST.mkdir(parents=True, exist_ok=True)
    files = sorted(f for f in PACK.rglob("*") if f.is_file())
    presets = sum(1 for f in files if f.suffix == ".augur5")
    out = DIST / ("TONAL LAB - %s (%d presets).zip" % (PACK.name, presets))
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in files:
            z.write(f, (PACK.name + "/" + f.relative_to(PACK).as_posix()))
    print("%s: %d presets, %d KB" % (out.name, presets, out.stat().st_size // 1024))


if __name__ == "__main__":
    main()
