FROM python:3.12-slim
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY server.py analisar_ia.py index.html ecotech_co2_sensor.c ./
ENV HOST=0.0.0.0
EXPOSE 3000
CMD ["python3", "server.py"]
