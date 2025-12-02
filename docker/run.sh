#!/bin/bash
set -e

CONTAINER_NAME="dji_esdk_container"
IMAGE_NAME="sebab90/dji_esdk_minimal:latest"
USER_NAME=$(whoami)
WORKSPACE_DIR="$(realpath ../Edge-SDK)"

# Se il container è già in esecuzione
if docker ps --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "⚠️ Il container '${CONTAINER_NAME}' è già in esecuzione."
    echo "👉 Usa './exec.sh' per entrarci oppure './stop.sh' per fermarlo."
    exit 0
fi

# Se il container esiste ma è fermo
if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "⚠️ Il container '${CONTAINER_NAME}' esiste già (fermo)."
    read -p "Vuoi eliminarlo e ricrearlo da zero? [y/N]: " RESP
    if [[ "$RESP" =~ ^[Yy]$ ]]; then
        echo "🧹 Rimuovo il container esistente..."
        docker rm ${CONTAINER_NAME} >/dev/null
    else
        echo "❌ Operazione annullata."
        exit 0
    fi
fi

# Abilita X11
echo "🎨 Abilito X11 per GUI..."
xhost +local:root >/dev/null 2>&1 || true

# Crea e avvia container
echo "🚀 Creo e avvio container '${CONTAINER_NAME}'..."
docker run -it \
  --name ${CONTAINER_NAME} \
  --hostname ${CONTAINER_NAME} \
  --network=host \
  --gpus all \
  --privileged \
  --cpuset-cpus="0-15" \
  --memory=12gb \
  -e DISPLAY=$DISPLAY \
  -e QT_X11_NO_MITSHM=1 \
  -e XDG_RUNTIME_DIR=/tmp/runtime-$USER \
  -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
  -v ${WORKSPACE_DIR}:/home/${USER_NAME}/Edge-SDK:rw \
  -v $(pwd):/home/${USER_NAME}/docker:rw \
  -v /dev:/dev \
  -v /dev/shm:/dev/shm:rw \
  -v /run/udev:/run/udev:ro \
  --user ${USER_NAME} \
  --cap-add=NET_ADMIN \
  --cap-add=SYS_RESOURCE \
  --cap-add=SYS_NICE \
  "${IMAGE_NAME}" /bin/bash
