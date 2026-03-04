# ESP CAM Face Detection

Проект на ESP-IDF для ESP32-CAM с обработкой видеопотока и сетевыми компонентами.

## Версия ПО

- **ESP-IDF (требование проекта):** `>= 5.5` (см. `idf_component.yml`)
- **ESP-IDF (локальная сборка в этом репозитории):** `5.5.0` (см. `build/config.env`)
- **Цель сборки:** `esp32`
- **Зависимости компонентов:**
	- `esp_jpeg ^1.3.0`
	- `esp_http_client ^1.1.0`
	- `cJSON ^1.7.0`
	- `esp_websocket_client *`

## Перенос параметров ESP-IDF (`sdkconfig`)

Файл `sdkconfig` больше **не игнорируется** Git и хранится в репозитории для переноса параметров между машинами.

Рекомендуемый порядок:

1. На исходной машине настроить проект (`idf.py menuconfig` при необходимости).
2. Закоммитить изменения `sdkconfig`.
3. На новой машине выполнить:
	 - `idf.py fullclean`
	 - `idf.py reconfigure`
	 - `idf.py build`

Если после обновления ESP-IDF часть опций устарела, выполните `idf.py menuconfig` и сохраните новый `sdkconfig`.

## Сборка и прошивка

- Сборка: `build.ps1` или задача VS Code **ESP-IDF Build**
- Прошивка: `flash.ps1` или задача VS Code **ESP-IDF Flash (1-click)**
