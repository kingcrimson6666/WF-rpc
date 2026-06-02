#!/bin/bash

CERT_DIR="${1:-./conf}"
DAYS="${2:-365}"

mkdir -p "$CERT_DIR"

openssl req -newkey rsa:2048 -nodes -keyout "$CERT_DIR/server.key" \
    -x509 -days "$DAYS" -out "$CERT_DIR/server.crt" \
    -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

echo "Certificate generated:"
echo "  - $CERT_DIR/server.crt"
echo "  - $CERT_DIR/server.key"