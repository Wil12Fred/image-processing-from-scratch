#!/bin/sh
# Runs the two real programs in scratch directories.
#  1. procimg with no input images must exit 1 with a message (it used to
#     abort inside libpng).
#  2. procimg on the four generated 16x16 images in tests/data/ must write a
#     sumlena.png with the same pixels as tests/expected/sumlena.png.
#  3. procimg-gui must start and keep running for 5 s, always on a private
#     xvfb-run display (never the caller's DISPLAY); exit 124 = still running.
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/empty" "$work/batch" "$work/gui"

cd "$work/empty"
set +e
"$here/procimg" > out.txt 2> err.txt
rc=$?
set -e
[ "$rc" -eq 1 ] || { echo "FAIL: procimg without inputs exited $rc, expected 1"; cat err.txt; exit 1; }
grep -q 'cannot read camahorizontal.png' err.txt || { echo "FAIL: no clear message"; cat err.txt; exit 1; }
echo "ok: procimg without input images exits 1 with: $(grep procimg: err.txt | cut -c1-60)..."

cd "$work/batch"
cp "$here"/tests/data/*.png .
"$here/procimg"
"$here/tests/test_filters" --same sumlena.png "$here/tests/expected/sumlena.png" \
  || { echo "FAIL: sumlena.png differs from tests/expected/sumlena.png"; exit 1; }
echo "ok: procimg sums the four generated images into the expected sumlena.png"

if [ -x "$here/build/procimg-gui" ]; then
  command -v xvfb-run >/dev/null || { echo "FAIL: xvfb-run is required (package xvfb)"; exit 1; }
  cd "$work/gui"
  cp "$here/tests/data/camahorizontal.png" lena.png   # the GUI's startup image
  set +e
  env -u DISPLAY -u WAYLAND_DISPLAY xvfb-run -a timeout 5 "$here/build/procimg-gui"
  rc=$?
  set -e
  [ "$rc" -eq 124 ] || { echo "FAIL: procimg-gui exited $rc within 5 s"; exit 1; }
  echo "ok: procimg-gui starts and keeps running under xvfb"
else
  echo "skip: build/procimg-gui not built (make gui)"
fi
