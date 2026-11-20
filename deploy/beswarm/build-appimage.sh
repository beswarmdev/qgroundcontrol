#!/usr/bin/env bash
# Builds the BeSwarm Vehicle GS AppImage inside WSL from a commit of the Windows repository.
# Called only by dist-linux.ps1 (paths already in /mnt/<drive>/... form); see deploy/beswarm/README.md.
set -euo pipefail

usage() {
    echo "Usage: build-appimage.sh --origin <dir> --sha <commit> --out <file> [--src <dir>] [--jobs <n>] [--skip-build]"
}

die() {
    echo "build-appimage: $*" >&2
    exit 1
}

step() {
    echo "==> $*"
}

ORIGIN=""
SHA=""
OUT=""
SRC="$HOME/src/qgroundcontrol"
JOBS=$(($(nproc) - 2))
if ((JOBS < 1)); then
    JOBS=1
fi
SKIP_BUILD=0

while (($# > 0)); do
    case "$1" in
        --origin)
            ORIGIN="${2:?--origin needs a value}"
            shift 2
            ;;
        --sha)
            SHA="${2:?--sha needs a value}"
            shift 2
            ;;
        --out)
            OUT="${2:?--out needs a value}"
            shift 2
            ;;
        --src)
            SRC="${2:?--src needs a value}"
            shift 2
            ;;
        --jobs)
            JOBS="${2:?--jobs needs a value}"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        *)
            usage >&2
            die "unknown argument: $1"
            ;;
    esac
done

if [[ -z "$ORIGIN" || -z "$SHA" || -z "$OUT" ]]; then
    usage >&2
    die "--origin, --sha and --out are required"
fi
if [[ ! "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
    die "--jobs must be a positive integer, got: $JOBS"
fi

step "Syncing the Linux clone ($SRC) to $SHA"
if ! git config --global --get-all safe.directory | grep -xF -e "$ORIGIN" >/dev/null; then
    git config --global --add safe.directory "$ORIGIN"
fi
if [[ ! -d "$SRC/.git" ]]; then
    mkdir -p "$(dirname "$SRC")"
    git clone "$ORIGIN" "$SRC"
fi
git -C "$SRC" fetch --force --tags origin '+refs/heads/*:refs/remotes/origin/*'
if ! git -C "$SRC" cat-file -e "$SHA^{commit}"; then
    die "commit $SHA is not in any branch of the Windows repository"
fi
git -C "$SRC" checkout --force --detach "$SHA"

export QT_ROOT_DIR="$HOME/Qt/6.12.0/gcc_64"
export CPM_SOURCE_CACHE="$HOME/.cache/qgc-cpm"
# WSL does not always mount FUSE; CreateAppImage.cmake unsets this where it needs the real mount.
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$HOME/.local/bin:$PATH"
if [[ ! -f "$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake" ]]; then
    die "Qt gcc_64 not found at $QT_ROOT_DIR; run the one-time WSL setup from deploy/beswarm/README.md"
fi

BUILD_DIR="$SRC/build/Linux"
BINARY="$BUILD_DIR/Release/QGroundControl"
APPIMAGE="$BUILD_DIR/QGroundControl-x86_64.AppImage"

if ((SKIP_BUILD == 0)); then
    step "Building with the Linux preset ($JOBS jobs)"
    if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        (cd "$SRC" && cmake --preset Linux)
    fi
    (cd "$SRC" && cmake --build --preset Linux --parallel "$JOBS")
fi

step "Boot test of the build tree binary"
if [[ ! -x "$BINARY" ]]; then
    die "binary not found: $BINARY"
fi
QT_QPA_PLATFORM=offscreen timeout 600 "$BINARY" --simple-boot-test --allow-multiple

step "Creating the AppImage"
rm -f "$APPIMAGE"
cmake --install "$BUILD_DIR" --config Release
if [[ ! -f "$APPIMAGE" ]]; then
    die "AppImage was not created: $APPIMAGE"
fi

step "Boot test of the AppImage"
chmod +x "$APPIMAGE"
QT_QPA_PLATFORM=offscreen timeout 600 "$APPIMAGE" --simple-boot-test --allow-multiple

step "Copying the AppImage to $OUT"
mkdir -p "$(dirname "$OUT")"
cp -f "$APPIMAGE" "$OUT"
echo "commit:   $SHA"
echo "appimage: $OUT"
# No explicit "exit 0" here: under "bash -l" the Ubuntu ~/.bash_logout runs with errexit and turns it into exit 1.
