#!/bin/bash
set -e

IMAGE_NAME="sebab90/dji_esdk_minimal"

docker buildx build \
  --file Dockerfile \
  --platform linux/amd64 \
  --memory=16g \
  --build-arg USERNAME=$(whoami) \
  --tag ${IMAGE_NAME}:latest \
  --load \
  .

echo ""
echo "✅ Build completata per ${IMAGE_NAME}:latest"
