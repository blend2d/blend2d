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

# 2D rendering context for benchmarking (bl_bench_context):
fetch_dependency https://github.com/blend2d/blend2d-bench-agg 3rdparty/agg
fetch_dependency https://github.com/juce-framework/JUCE 3rdparty/JUCE

# Compression benchmarking (bl_bench_compression):

