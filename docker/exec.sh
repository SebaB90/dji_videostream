#!/bin/bash
set -e

CONTAINER_NAME="dji_esdk_container"

if docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "🔗 Entrando nel container '${CONTAINER_NAME}'..."
    docker exec -it ${CONTAINER_NAME} /bin/bash
else
    if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
        echo "⚠️ Il container '${CONTAINER_NAME}' esiste ma non è in esecuzione."
        echo "👉 Avvialo con: docker start ${CONTAINER_NAME}"
    else
        echo "❌ Nessun container chiamato '${CONTAINER_NAME}' trovato."
        echo "👉 Crealo e avvialo con: ./run.sh"
    fi
fi
