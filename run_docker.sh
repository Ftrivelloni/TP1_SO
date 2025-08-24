#!/bin/bash

# Script to work with the required Docker image for TP1_SO

# Default command to run
CMD=${@:-/bin/bash}

# Run Docker with the current directory mounted
docker run --rm -it \
  -v "$(pwd):/home/student/TP1_SO" \
  -w /home/student/TP1_SO \
  --ipc=host \
  --cap-add SYS_PTRACE \
  --security-opt seccomp=unconfined \
  agodio/itba-so-multi-platform:3.0 \
  $CMD
