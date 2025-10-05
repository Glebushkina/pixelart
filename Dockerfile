FROM python:3.9-slim

WORKDIR /app

# Установка системных зависимостей для OpenCV
RUN apt-get update && apt-get install -y \
    libglib2.0-0 \
    libsm6 \
    libxext6 \
    libxrender-dev \
    libgl1 \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

# Копирование requirements и установка Python зависимостей
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Копирование исходного кода
COPY . .

# Создание директории для загрузок
RUN mkdir -p uploads static

# Копирование статических файлов
COPY static/ static/

# Открытие порта
EXPOSE 5000

# Запуск приложения
CMD ["python", "app.py"]