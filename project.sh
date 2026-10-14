#!/bin/bash
# Export project variables
export PRJ=cloud-video-editor
export SRC=src
export SRC_C=$SRC/main/c
export SRC_CPP=$SRC/main/cpp
export BUILD=target
export BUILD_DEBUG=$BUILD/exe/debug
# Variable name LIB reserved for ./configure
export REQUIRED_LIBS="gtk+-3.0 webkit2gtk-4.1 glib-2.0 gobject-2.0 gio-2.0 sqlite3 lua5.3"

# Array of prerequisite packages
REQUIRED_PACKAGES="build-essential libwebkit2gtk-4.1-dev libsqlite3-dev liblua5.3-dev lua5.3 libmpv-dev"

# Array of avoided packages
AVOIDED_PACKAGES=""

apt_install() {
  RESULT=$(apt list --installed 2>&1 | grep "$2")
  if [ -z "$RESULT" ]; then 
    if [ -z "$1" ]; then
      sudo apt update
    fi
    sudo apt install -y -qq "$2"
    echo "1"
  else 
    echo "$1"
  fi 
}

apt_uninstall() {
  if apt list --installed 2>&1 | grep -q "$1"; then 
    sudo apt purge --autoremove -y -qq "$1"
  fi 
}

# libmpv-dev is headers only. Do not link -lmpv.
detect_pm() {
  if command -v apt-get >/dev/null 2>&1; then
    echo apt
  elif command -v pacman >/dev/null 2>&1; then
    echo pacman
  elif command -v dnf >/dev/null 2>&1; then
    echo dnf
  elif command -v yum >/dev/null 2>&1; then
    echo yum
  elif command -v zypper >/dev/null 2>&1; then
    echo zypper
  elif command -v apk >/dev/null 2>&1; then
    echo apk
  elif command -v xbps-install >/dev/null 2>&1; then
    echo xbps
  elif command -v brew >/dev/null 2>&1; then
    echo brew
  else
    echo unknown
  fi
}

native_pkg() {
  deb=$1
  pm=$2
  if [ "$deb" != "libmpv-dev" ]; then
    printf '%s\n' "$deb"
    return
  fi
  case "$pm" in
    apt) printf '%s\n' libmpv-dev ;;
    pacman|brew) printf '%s\n' mpv ;;
    dnf|yum|zypper|xbps) printf '%s\n' mpv-devel ;;
    apk) printf '%s\n' mpv-dev ;;
    *) printf '%s\n' libmpv-dev ;;
  esac
}

pm_install() {
  upd=$1
  deb=$2
  pm=$(detect_pm)
  case "$pm" in
    apt)
      apt_install "$upd" "$deb"
      ;;
    *)
      pkg=$(native_pkg "$deb" "$pm")
      if [ "$pkg" = "$deb" ]; then
        if [ "$deb" = "libmpv-dev" ]; then
          echo "[ERROR] mpv/client.h was not found. Install libmpv-dev or set MPV_INCLUDE_DIR. Do not link -lmpv." >&2
        else
          echo "[WARN] No $pm package name for $deb. Skipping." >&2
        fi
        echo "$upd"
        return
      fi
      case "$pm" in
        pacman)
          if ! pacman -Q "$pkg" >/dev/null 2>&1; then
            sudo pacman -S --noconfirm --needed "$pkg"
          fi
          ;;
        dnf)
          if ! rpm -q "$pkg" >/dev/null 2>&1; then
            sudo dnf install -y "$pkg"
          fi
          ;;
        yum)
          if ! rpm -q "$pkg" >/dev/null 2>&1; then
            sudo yum install -y "$pkg"
          fi
          ;;
        zypper)
          if ! rpm -q "$pkg" >/dev/null 2>&1; then
            sudo zypper --non-interactive install "$pkg"
          fi
          ;;
        apk)
          if ! apk info -e "$pkg" >/dev/null 2>&1; then
            sudo apk add "$pkg"
          fi
          ;;
        xbps)
          if ! xbps-query "$pkg" >/dev/null 2>&1; then
            sudo xbps-install -y "$pkg"
          fi
          ;;
        brew)
          if ! brew list --formula "$pkg" >/dev/null 2>&1; then
            brew install "$pkg"
          fi
          ;;
        *)
          echo "[ERROR] mpv/client.h was not found. Install libmpv-dev or set MPV_INCLUDE_DIR. Do not link -lmpv." >&2
          ;;
      esac
      echo "$upd"
      ;;
  esac
}

install() {
  UPD=0
  echo "$REQUIRED_PACKAGES" | tr ' ' '\n' | while read -r package; do
    if [ ! -z "$package" ]; then
      UPD=$(pm_install $UPD "$package")
    fi
  done

  echo "$AVOIDED_PACKAGES" | tr ' ' '\n' | while read -r package; do
    if [ ! -z "$package" ]; then
      apt_uninstall "$package"
    fi
  done
}

install
