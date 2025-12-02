#!/bin/bash
set -e

CONTAINER_NAME="dji_esdk_container"

echo "🛑 Arresto container '${CONTAINER_NAME}'..."
if docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    docker stop ${CONTAINER_NAME} >/dev/null
    echo "✅ Container '${CONTAINER_NAME}' fermato."
else
    echo "ℹ️ Nessun container '${CONTAINER_NAME}' in esecuzione."
fi
