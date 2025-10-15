# Chimaera Runtime Docker Container
# This container runs the Chimaera distributed task execution runtime as a daemon
FROM iowarp/iowarp-deps-spack:latest

# Source spack environment
SHELL ["/bin/bash", "-c"]

# Install iowarp using spack
RUN source /opt/spack/share/spack/setup-env.sh && \
    spack install -y iowarp@ai && \
    spack load iowarp@ai

# Create runtime directories
RUN mkdir -p /etc/chimaera /var/log/chimaera

# Add spack environment to PATH
ENV SPACK_ROOT=/opt/spack
RUN echo "source /opt/spack/share/spack/setup-env.sh" >> /etc/profile.d/spack.sh && \
    echo "spack load iowarp@ai" >> /etc/profile.d/spack.sh && \
    chmod +x /etc/profile.d/spack.sh

# Update library cache
RUN ldconfig

# Copy entrypoint script
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

# Default configuration values (can be overridden via environment variables or config file)
ENV CHI_SCHED_WORKERS=8 \
    CHI_PROCESS_REAPER_WORKERS=1 \
    CHI_MAIN_SEGMENT_SIZE=1073741824 \
    CHI_CLIENT_DATA_SEGMENT_SIZE=536870912 \
    CHI_RUNTIME_DATA_SEGMENT_SIZE=536870912 \
    CHI_ZMQ_PORT=5555 \
    CHI_LOG_LEVEL=info \
    CHI_STACK_SIZE=65536 \
    CHI_QUEUE_DEPTH=10000 \
    CHI_HEARTBEAT_INTERVAL=1000 \
    CHI_TASK_TIMEOUT=30000 \
    CHI_SHM_SIZE=2147483648

# Set CHI_SERVER_CONF to default location (can be overridden)
ENV CHI_SERVER_CONF=/etc/chimaera/chimaera_config.yaml

# Expose ZeroMQ port (default 5555, can be overridden)
EXPOSE 5555

# Set shared memory size requirement
# This must match CHI_SHM_SIZE environment variable
# Use --shm-size flag when running container

# Use entrypoint script to generate config and start runtime
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

# Default command (can be overridden)
CMD ["chimaera_start_runtime"]
