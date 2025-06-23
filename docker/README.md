# Docker

This directory contains the Docker configuration files for setting up the development environment for the air quality sensor project.

## Files

- **docker-compose.yml**: Defines the services and configurations for Docker Compose.
- **Dockerfile**: Specifies the instructions to build the Docker image.

## Usage

To build the docker image, run the folowing command

```bash
docker build -t aqs_ble_client:latest .
```

To run the docker container, ensure the variables in the docker-compose.yml are ok and run the following command:

```bash
docker compose up -d
```

To check that the script works, you can check the logs of the container:

```bash
docker compose logs -f aqs_ble_client
```