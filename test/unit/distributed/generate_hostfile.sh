#!/bin/bash
# Generate hostfile for Docker cluster
# This creates a hostfile with the IP addresses of all containers

# Output file
HOSTFILE="hostfile"

# Generate hostfile
cat > "$HOSTFILE" << 'EOF'
172.25.0.10
172.25.0.11
172.25.0.12
172.25.0.13
EOF

echo "Generated hostfile:"
cat "$HOSTFILE"
