#!/bin/bash

# Prefijo (vacío si no hay argumento)
prefix="${1:-}"

# Construimos la expresión de búsqueda
#   - en el directorio actual (.)
#   - sin descender en subdirectorios (-maxdepth 1)
#   - solo ficheros regulares (-type f)
#   - cuyo nombre comience por "$prefix"
count=$(find . -maxdepth 1 -type f -name "${prefix}*" | wc -l)

# Espera de 2 segundos para monitorización
sleep 2

# Mensaje de salida
echo "Número de ficheros encontrados: $count"

# Código de retorno: 0 si hay ≥1 fichero, 1 si no hay ninguno
if [ "$count" -gt 0 ]; then
  exit 0
else
  exit 1
fi
