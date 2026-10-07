# Use official Python image as base
FROM python:3.12-slim

# Set working directory
WORKDIR /app

# Install dependencies
RUN pip install --no-cache-dir aiohttp paho-mqtt prometheus-client

# Prometheus metrics endpoint
EXPOSE 8000

# Set the default command to run the script
CMD ["python", "-u", "aqs_matter_client.py"]
