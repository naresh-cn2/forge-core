#!/bin/bash
# Master System Generator v1.0

if [ -z "$1" ]; then
    echo "Usage: ./sysgen.sh <System-Name>"
    exit 1
fi

SYS_NAME=$1
mkdir -p $SYS_NAME/{src,benchmarks,logs,scripts,tests,docs,datasets,configs,assets}
touch $SYS_NAME/README.md $SYS_NAME/ROADMAP.md $SYS_NAME/FORGE_LOG.md
chmod +x $SYS_NAME

echo "--- Ecosystem Initialized: $SYS_NAME ---"
echo "Structure: 9 Folders | 3 Docs | 1 Entrypoint"
