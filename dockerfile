# Użyj oficjalnego, lekkiego obrazu Pythona
FROM python:3.11-slim

# Ustaw zmienne środowiskowe (optymalizacja)
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

# Ustaw katalog roboczy
WORKDIR /app

# Najpierw skopiuj plik zależności – to pozwala wykorzystać cache Dockera
COPY requirements.txt .

# Zainstaluj zależności
RUN pip install --no-cache-dir -r requirements.txt

# Teraz skopiuj resztę plików
COPY . .

# Utwórz zwykłego użytkownika dla bezpieczeństwa (opcjonalnie)
RUN adduser --disabled-password --gecos '' appuser && \
    chown -R appuser:appuser /app
USER appuser

# aby był healty
RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*

# Otwórz port
EXPOSE 5000

# Uruchom aplikację przez gunicorn (produkcyjnie)
CMD ["gunicorn", "--bind", "0.0.0.0:5000", "--workers", "4", "app:app"]
