#!/bin/bash

# Forcefully re-creates the docker container

docker compose down lichess chess
docker compose build --no-cache --progress=plain lichess chess
docker compose up -d --force-recreate lichess chess