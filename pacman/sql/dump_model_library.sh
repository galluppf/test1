#!/bin/bash
MYDATE=$(date +%Y%m%d_%H%M)
MYBAKFILE=model_library.sql.$MYDATE
echo "--- backing up existing model library in $MYBAKFILE"
cp model_library.sql $MYBAKFILE

echo "--- dumping model_library.db in model_library.sql"
echo '.dump' | sqlite3 model_library.db > model_library.sql
