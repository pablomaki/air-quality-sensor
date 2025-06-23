# Use official Python image as base
FROM python:3.9-slim

# Set working directory
WORKDIR /app

# Install dependencies
RUN apt-get update && apt-get install -y bluetooth bluez
RUN pip install paho-mqtt bleak

# Expose necessary ports (MQTT usually uses 1883)
EXPOSE 1883

# Set the default command to run the script
CMD ["python", "-u", "aqs_ble_client.py"]
