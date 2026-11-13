#!/bin/sh

fetch_dependency() {
  url="$1"
  dir="$2"

  if [ -d "$dir/.git" ]; then
    echo "Updating $dir"
    git -C "$dir" pull
  else
    echo "Cloning $dir (from $url)"
    git clone "$url" "$dir" --depth=1
  fi
}

fetch_dependency https://github.com/asmjit/asmjit 3rdparty/asmjit
