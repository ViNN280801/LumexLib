# CodeQL Security анализатор для LumexLib

Локальный анализатор безопасности C++ кода.

## Быстрый старт

```bash
# 1. Перейти в папку docker
cd docker

# 2. Собрать контейнер
docker compose up -d --build

# 3. Выполнить полный анализ (15-30 минут)
docker compose exec codeql full-scan.sh /workspace/lumexlib
```

### Полный анализ (15-30 минут)

```bash
# Полный набор security + quality проверок
docker compose exec codeql full-scan.sh /workspace/lumexlib

# С дополнительными параметрами
docker compose exec codeql full-scan.sh /workspace/lumexlib \
  --threads 8 \
  --memory 8192 \
  --clean
```

## Доступные команды

| Команда                                                       | Описание                   | Время выполнения |
| ------------------------------------------------------------- | -------------------------- | ---------------- |
| `docker compose exec codeql full-scan.sh /workspace/lumexlib` | Полный анализ безопасности | 15-30 минут      |
| `docker compose exec codeql bash`                             | Интерактивная оболочка     | -                |

## Анализируемые уязвимости

- **Memory Management**: use-after-free, double-free, memory leaks
- **Buffer Operations**: buffer overflow, stack/heap overflow
- **Input Validation**: SQL injection, command injection, format strings
- **Cryptography**: weak algorithms, insufficient key sizes
- **Type Safety**: unsafe casts, integer overflow
- **Custom Rules**: специфичные для LumexLib проверки

## Структура результатов

```console
docker/results/
├── LumexLib_20240115_143022.sarif    # SARIF для IDE/GitHub
├── LumexLib_20240115_143022.csv      # CSV для анализа в Excel
├── latest.sarif                      # Последний результат
├── latest.csv                        # Последний CSV
├── summary.txt                       # Краткая сводка
└── complete-*/                       # Полные анализы
    ├── complete-analysis.sarif
    ├── complete-analysis.csv
    ├── analyzed-files.txt
    └── summary.txt
```

### Интеграция с IDE

```bash
# Копирование SARIF в проект для VSCode/Visual Studio
cp docker/results/latest.sarif .vscode/codeql-results.sarif
```

## Требования

- **Docker** 20.10+
- **Docker Compose** 2.0+
- **RAM**: 4GB минимум (рекомендуется 8GB для полного анализа)
- **CPU**: 2+ ядра (больше = быстрее)
- **Диск**: 10GB свободного места

## Кастомизация

### Изменить настройки анализа

```bash
# Редактировать конфигурацию CodeQL
nano ../.github/codeql/codeql-config.yml

# Добавить собственные правила
nano ../.github/codeql/custom-queries.ql

# Перезапустить контейнер для применения изменений
docker compose restart codeql
```

### Изменить ресурсы контейнера

```yaml
# В docker-compose.yml
deploy:
  resources:
    limits:
      memory: 8G # Увеличить память
      cpus: "6.0" # Больше ядер CPU
```

### Настройки анализа

```bash
# Переменные окружения в docker-compose.yml
environment:
  - CODEQL_THREADS=8        # Количество потоков
  - CODEQL_RAM=8192         # Память в MB
  - CPP_STANDARD=17         # Стандарт C++
  - EXCLUDE_TESTS=true      # Исключить тесты
```

## Troubleshooting

### Контейнер не запускается

```bash
# Посмотреть логи
docker compose logs codeql

# Пересобрать образ
docker compose build --no-cache codeql
```

### Не находит исходный код

```bash
# Проверить mount в docker-compose.yml
volumes:
  - "../:/workspace/lumexlib"

# Проверить, что контейнер видит файлы
docker compose exec codeql ls -la /workspace/lumexlib/lumex/
```

### Пустые результаты

```bash
# Проверить что анализ нашел файлы
docker compose exec codeql full-scan.sh /workspace/lumexlib --help

# Посмотреть какие файлы были проанализированы
cat docker/results/latest.sarif | jq '.runs[].results | length'
```

## Интеграция с CI/CD

Файлы `.github/workflows/` содержат готовые конфигурации для:

- **GitHub Actions** - автоматический анализ при push/PR
- **Security Reports** - отчеты о найденных уязвимостях
- **Branch Protection** - блокировка merge при критических находках

---
