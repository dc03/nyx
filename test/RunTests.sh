#!/usr/bin/env bash

NYX=$(find ../ -name nyx | head -n 1)

for i in $(find ./ -type f); do
  if ! [[ ${i} =~ RunTests.sh ]]; then
    echo "Running ${i}"
    ${NYX} --main ${i}
  fi
done