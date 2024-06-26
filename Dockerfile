# Use an official Python runtime as a parent image
FROM espressif/idf:latest

# Set the working directory
WORKDIR /esp

# Expose the USB port for the ESP32 connection
EXPOSE 3333

# Install additional dependencies if needed
RUN apt-get update && apt-get install -y \
    vim \
    git \
    wget \
    make \
    && rm -rf /var/lib/apt/lists/*

# Set environment variables for ESP-IDF
ENV IDF_PATH=/opt/esp/idf
ENV PATH=$IDF_PATH/tools:$PATH

# Set default command to run when starting the container
CMD ["/bin/bash"]

