#!/bin/bash
# Preparar el entorno de la clase en Linux.
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/scripts/install_env_unix.sh"
