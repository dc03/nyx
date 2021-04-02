#!/usr/bin/env bash

WIS=$(find ../ -name wis | head -n 1)

for i in $(find ./ -type f); do
  if ! [[ ${i} =~ RunTests.sh ]]; then
    echo "Running ${i}:"
    ${WIS} --main ${i}
  fi
done