# Deployment Dockerfile for IOWarp Runtime
# Inherits from the build container and runs chimaera_start_runtime
FROM iowarp/iowarp-runtime-build:latest

# Create configuration directory
RUN mkdir -p /etc/chimaera

# Copy default configuration
COPY config/chimaera_default.yaml /etc/chimaera/chimaera_config.yaml

# Create entrypoint script
RUN echo '#!/bin/bash\n\
set -e\n\
\n\
# Default configuration path\n\
CONFIG_FILE=${CHI_CONFIG_FILE:-/etc/chimaera/chimaera_config.yaml}\n\
\n\
# Generate configuration from environment variables if not provided\n\
if [ ! -f "$CONFIG_FILE" ]; then\n\
  echo "Generating configuration from environment variables..."\n\
  cat > "$CONFIG_FILE" <<EOF\n\
# Chimaera Runtime Configuration\n\
workers:\n\
  sched_threads: ${CHI_SCHED_WORKERS:-8}\n\
  process_reaper_threads: ${CHI_PROCESS_REAPER_THREADS:-1}\n\
\n\
memory:\n\
  main_segment_size: ${CHI_MAIN_SEGMENT_SIZE:-1073741824}\n\
  client_data_segment_size: ${CHI_CLIENT_DATA_SEGMENT_SIZE:-536870912}\n\
  runtime_data_segment_size: ${CHI_RUNTIME_DATA_SEGMENT_SIZE:-536870912}\n\
\n\
networking:\n\
  port: ${CHI_ZMQ_PORT:-5555}\n\
  neighborhood_size: ${CHI_NEIGHBORHOOD_SIZE:-32}\n\
\n\
logging:\n\
  level: \"${CHI_LOG_LEVEL:-info}\"\n\
  file: \"${CHI_LOG_FILE:-/tmp/chimaera.log}\"\n\
\n\
performance:\n\
  stack_size: ${CHI_STACK_SIZE:-65536}\n\
  queue_depth: ${CHI_QUEUE_DEPTH:-10000}\n\
  lane_map_policy: \"${CHI_LANE_MAP_POLICY:-round_robin}\"\n\
\n\
runtime:\n\
  heartbeat_interval: ${CHI_HEARTBEAT_INTERVAL:-1000}\n\
  task_timeout: ${CHI_TASK_TIMEOUT:-30000}\n\
EOF\n\
fi\n\
\n\
echo "Starting Chimaera runtime with configuration: $CONFIG_FILE"\n\
cat "$CONFIG_FILE"\n\
\n\
# Execute chimaera_start_runtime with the configuration\n\
exec chimaera_start_runtime "$CONFIG_FILE"\n\
' > /usr/local/bin/entrypoint.sh && chmod +x /usr/local/bin/entrypoint.sh

# Expose default ZeroMQ port
EXPOSE 5555

# Set entrypoint
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
