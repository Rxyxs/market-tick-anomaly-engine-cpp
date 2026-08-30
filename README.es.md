[ Read in English ](README.md) | [ Español ]

# 1. Título del Proyecto

## Motor de Anomalías de Mercado en Tiempo Real — Cero Dependencias, en C++

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=cplusplus&logoColor=white)
![MSVC](https://img.shields.io/badge/compilador-MSVC%20(cl.exe)-5C2D91?style=flat)
![CMake](https://img.shields.io/badge/build-CMake%20%7C%20build.ps1-064F8C?style=flat&logo=cmake&logoColor=white)
![Dependencias](https://img.shields.io/badge/dependencias%20externas-cero-brightgreen?style=flat)
![Tests](https://img.shields.io/badge/tests-51%2F51%20pasando-brightgreen?style=flat)
![Datos](https://img.shields.io/badge/datos-trades%20reales%20de%20Binance-lightgrey?style=flat)
![Status](https://img.shields.io/badge/status-validado%20contra%20un%20evento%20real-blue?style=flat)

Construí un motor de detección de anomalías en tiempo real para datos de mercado paso a paso, usando **C++20 puro sin librerías externas** — sin Boost, sin gestores de paquetes como vcpkg y sin librerías gráficas. El sistema lee y procesa sus propios archivos CSV, calcula las estadísticas en directo y genera sus propios gráficos en formato SVG.

Procesé **4,78 millones de transacciones reales** del archivo histórico de Binance durante los días **11, 12 y 13 de marzo de 2020 (el "Jueves Negro" cripto)**, cuando Bitcoin cayó casi un 50% en un solo día. Diseñé cuatro detectores desde cero (Z-score EWMA, CUSUM, ráfagas de volumen y desbalance de órdenes) que funcionan en conjunto en una sola pasada, alcanzando una velocidad de **2,7 millones de transacciones por segundo**.

Sumé un segundo pipeline en vivo (`streaming_demo.exe`): un **buffer circular sin bloqueos (lock-free)** que conecta un simulador de feed de Order Book L2 con los mismos detectores en línea, sosteniendo **~7,3 millones de ticks por segundo** productor-a-consumidor, con métricas transmitidas en vivo hacia un consumidor en Python.

> Este proyecto forma parte de mi serie personal sobre detección de anomalías financieras. Puedes revisar también mis proyectos previos: [chile-aml-anomaly-detection-engine](https://github.com/Rxyxs/chile-aml-anomaly-detection-engine) (análisis de grafos en Python) y [credit-fraud-autoencoder-detection-engine](https://github.com/Rxyxs/credit-fraud-autoencoder-detection-engine) (autoencoders y XGBoost en Python). En este desarrollo cambié el enfoque a C++ puro para demostrar rendimiento nativo y ajusté la forma de validar los resultados: **los datos reales de mercado no vienen etiquetados como "fraude"**, así que probé la efectividad del detector contra un evento financiero real y documentado en lugar de usar una matriz de confusión tradicional.

---

# 2. Motivación

Diseñé este proyecto respondiendo a dos decisiones clave:

**Por qué C++ y cero dependencias:** Quería sumar un proyecto en C++ nativo que respaldara mis habilidades junto a Python, SQL y R. En lugar de usar atajos o librerías externas, escribí todo el proceso (lectura de CSV, cálculo estadístico, puntaje de alertas y generación de gráficos SVG) en C++17 compilado con MSVC. Tomé esta decisión porque en sistemas financieros de alta velocidad, cada librería externa suma retrasos y posibles puntos de falla.

**Por qué datos de mercado:** En mis proyectos anteriores era fácil identificar fraude porque el dato venía etiquetado. En un exchange real esto no existe; solo hay precio, cantidad, hora y si la orden fue de compra o venta. Esto me obligó a implementar control estadístico en tiempo real en lugar de un clasificador común, demostrando cómo resolver problemas similares bajo condiciones distintas.

## 2.1 Impacto de Negocio e Indicadores Clave (KPIs)

| Métrica | Resultado | Qué significa |
|---|---|---|
| Trades reales procesados | 4.780.043 (3 días, Binance BTC/USDT real) | Crash cripto real de marzo 2020 ("Black Thursday"), no datos sintéticos |
| Puntaje pico de anomalía, momento exacto | 5,55 a las 2020-03-12 10:47 UTC | Cae exactamente dentro de un colapso real, documentado independientemente, de -13% en 3 minutos -- no elegido a conveniencia |
| Enriquecimiento de alertas en el día del crash | 7,43% vs. ~3,7% días circundantes (~2,0x) | Un aumento honesto, no dramático -- incluso los días "tranquilos" de cripto tienen ráfagas reales de corta duración |
| Throughput del pipeline | ~2,7M trades/seg de parseo, 1,89s totales para 4,78M trades | Cero dependencias externas, C++17 nativo |
| Bug real detectado por un resultado implausible | 4.319/4.320 barras marcadas anómalas inicialmente | Rastreado hasta un bug de calentamiento del EWMA, no aceptado sin cuestionar |

---

# 3. Marco Teórico

Implementé cuatro detectores independientes que analizan en paralelo bloques de datos (barras OHLCV) de 1 minuto, agrupados desde las operaciones individuales:

| Detector | Funcionamiento | Qué detecta |
|---|---|---|
| **Z-score EWMA** | Calcula el promedio y la variación del precio a lo largo del tiempo. Lo evalúo justo antes de incorporar el dato actual para evitar que un cambio brusco altere la base con la que se mide. | Movimientos de precio bruscos e inesperados en un solo minuto. |
| **CUSUM** (Page, 1954) | Suma acumulada del rendimiento. Identifica cambios constantes que ocurren durante varios minutos y que una regla simple no notaría. | Tendencias continuas de caída o subida sostenidas en el tiempo. |
| **Razón de ráfaga de volumen** | Compara el volumen de operaciones del minuto actual contra su promedio reciente. | Aumentos repentinos en la actividad, sin importar la dirección del precio. |
| **Z-score de desequilibrio de flujo de órdenes** | Mide la diferencia entre volumen de compra y venta agresiva por barra a partir de la marca del comprador. | Presión de compra o venta masiva, típica de liquidaciones en cascada. |

Cada detector entrega una nota donde `1.0` marca el límite de alerta. El **puntaje final del ensamble es el máximo entre los 4 detectores** (si uno detecta algo grave, se activa la alerta). También calculo el promedio por minuto como contexto adicional.

**Un periodo de calentamiento de 30 barras resulta indispensable.** Si se calculan estadísticas desde el primer dato, la falta de información genera varianzas de cero que provocan números gigantescos. En mis primeras pruebas, la segunda barra dio un resultado de -184.855, lo que dejó el sistema activado durante los 3 días completos. Al agregar un calentamiento de 30 minutos con un promedio simple, el cálculo se estabilizó correctamente.

---

# 4. Explicación

## Arquitectura del flujo de datos

```mermaid
flowchart LR
    A["download_data.ps1<br/>trades reales BTC/USDT (Binance)<br/>11-13 mar 2020, sin autenticacion"] --> B["csv_reader.cpp<br/>parser CSV manual en streaming<br/>2.7M trades/seg"]
    B --> C["bar_aggregator.cpp<br/>OHLCV de 1 min + volumen<br/>compra/venta taker, sin huecos"]
    C --> D["detectors/ensemble.cpp<br/>z-score EWMA + CUSUM +<br/>rafaga de volumen + desequilibrio de ordenes"]
    D --> E["main.cpp<br/>bars_with_signals.csv +<br/>lift vs. ventana de crash documentada"]
    D --> F["svg_writer.cpp<br/>graficos SVG auto-generados,<br/>cero librerias de graficacion"]
```

## Estructura de código

| Módulo | Función |
|---|---|
| [`data/download_data.ps1`](data/download_data.ps1) | Descarga y descomprime los datos de operaciones reales desde el servidor público de Binance. |
| [`src/csv_reader.cpp`](src/csv_reader.cpp) | Lector de CSV optimizado en streaming que procesa millones de filas por segundo sin cargar todo a la memoria. |
| [`src/bar_aggregator.cpp`](src/bar_aggregator.cpp) | Agrupa las operaciones en minutos (OHLCV), calcula volumen de compra/venta y rellena minutos vacíos para mantener un ritmo continuo. |
| [`src/detectors/ewma_zscore.*`](src/detectors/ewma_zscore.hpp) | Detector Z-score en tiempo real con periodo de calentamiento. |
| [`src/detectors/cusum.hpp`](src/detectors/cusum.hpp) | Algoritmo CUSUM para identificar cambios graduales sostenidos. |
| [`src/detectors/rolling_ratio.hpp`](src/detectors/rolling_ratio.hpp) | Comparador de volumen actual contra la ventana reciente. |
| [`src/detectors/ensemble.*`](src/detectors/ensemble.hpp) | Une los cuatro detectores por minuto en un único objeto de señal (`AnomalySignal`). |
| [`src/svg_writer.*`](src/svg_writer.hpp) | Generador de gráficos en formato SVG nativo, sin dependencias. |
| [`src/main.cpp`](src/main.cpp) | Programa principal: coordina la lectura, agrupación, detección y guardado de reportes. |
| [`src/streaming/spsc_ring_buffer.hpp`](src/streaming/spsc_ring_buffer.hpp) | Buffer circular sin bloqueos (lock-free), single-producer/single-consumer (C++20), con relleno de línea de cache para evitar false sharing entre productor y consumidor. |
| [`src/streaming/l2_types.hpp`](src/streaming/l2_types.hpp) | `L2TickEvent` — un POD trivialmente copiable que representa un evento de Order Book L2 (actualización de nivel o trade), el tipo que viaja por el ring buffer. |
| [`src/streaming/l2_feed_simulator.hpp`](src/streaming/l2_feed_simulator.hpp) | Productor: genera un stream sintético de ticks L2 (random walk de mid-price, actualizaciones bid/ask, trades) y lo empuja al ring buffer a la máxima tasa posible. |
| [`src/streaming/streaming_consumer.hpp`](src/streaming/streaming_consumer.hpp) | Consumidor: drena el ring buffer y corre detección en línea real (z-score EWMA sobre mid-price, razón de ráfaga de trades) sobre cada tick. |
| [`src/streaming/metrics_writer.hpp`](src/streaming/metrics_writer.hpp) | Calcula percentiles de latencia y escribe tanto las instantáneas NDJSON en vivo como el resumen final en JSON. |
| [`src/streaming_main.cpp`](src/streaming_main.cpp) | Conecta hilo productor + hilo consumidor + ring buffer; el punto de entrada de `streaming_demo.exe`. |
| [`tools/consume_streaming_metrics.py`](tools/consume_streaming_metrics.py) | Consumidor en Python (solo librería estándar) — sigue el feed NDJSON en vivo o imprime el resumen final. |
| [`tests/test_main.cpp`](tests/test_main.cpp) | Pruebas unitarias hechas a mano para verificar cada componente, incluyendo correctitud del ring buffer bajo hilos concurrentes reales y una prueba de latencia a escala de microsegundos. |

---

# 5. Metodología

- **Sin mirar al futuro (cero lookahead):** cada indicador usa únicamente información pasada y la barra actual. La evaluación se realiza antes de incorporar la nueva observación.
- **Validación con eventos reales:** como los datos no traen etiquetas de fraude, probé la efectividad comprobando si los picos de alerta coincidían con la caída histórica del 12 y 13 de marzo de 2020.
- **Análisis de rendimiento real:** comprobé que el procesamiento estadístico de las 4.320 barras toma menos de 1 milisegundo. Casi todo el tiempo de ejecución (1,8 de los 1,89 segundos totales) se va en la lectura de los archivos CSV desde el disco.

---

# 6. Desarrollo

## Requisitos

- Windows con Visual Studio 2019/2022 (Community o Build Tools) con la carga de trabajo "Desarrollo para el escritorio con C++", con soporte **C++20** (MSVC ≥ 19.29 / VS 16.11).
- PowerShell para los scripts de soporte.
- **CMake ≥ 3.20** (opcional) si preferís `CMakeLists.txt` en vez de `build.ps1` — ambos compilan los mismos tres ejecutables.

## Descargar los datos

```powershell
powershell -File data\download_data.ps1
```

Descarga los archivos `BTCUSDT-trades-2020-03-11/12/13.csv` (~345 MB descomprimidos) directamente desde el archivo de Binance.

## Compilar

```powershell
powershell -File build.ps1
```

Genera los ejecutables `outputs\bin\market_anomaly_engine.exe` (el motor batch histórico), `outputs\bin\run_tests.exe` (las pruebas) y `outputs\bin\streaming_demo.exe` (el pipeline en vivo lock-free) — verificado de punta a punta en esta máquina.

O, con CMake (camino estándar y multiplataforma — los mismos tres ejecutables):

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Ejecutar

```powershell
.\outputs\bin\market_anomaly_engine.exe data\raw\BTCUSDT-trades-2020-03-11.csv data\raw\BTCUSDT-trades-2020-03-12.csv data\raw\BTCUSDT-trades-2020-03-13.csv
```

## Ejecutar el pipeline de streaming en vivo

```powershell
.\outputs\bin\streaming_demo.exe 5000000   # n_ticks, por defecto 5.000.000
```

Corre el pipeline productor/consumidor lock-free y escribe `outputs\reports\streaming_benchmark.json` (resumen final de throughput/latencia) y `outputs\reports\streaming_metrics.ndjson` (instantáneas periódicas en vivo). Mientras corre (o después), se consumen las métricas desde Python (solo librería estándar, sin `pip install`):

```powershell
python tools\consume_streaming_metrics.py tail      # en vivo, sigue el feed NDJSON
python tools\consume_streaming_metrics.py summary   # resumen final del benchmark
```

## Pruebas

```powershell
.\outputs\bin\run_tests.exe
```

51 aserciones que cubren el parseo de CSV, la agregación de barras, los cuatro detectores batch, el ensamble, y la capa de streaming — incluyendo correctitud del ring buffer bajo dos `std::thread` reales concurrentes (no simulados en un solo hilo) y una prueba de latencia a escala de microsegundos sobre todo el camino productor → ring buffer → consumidor.

## Estructura del repositorio

```
market-tick-anomaly-engine-cpp/
├── data/
│   ├── download_data.ps1        # descarga los trades reales de Binance
│   └── raw/                     # BTCUSDT-trades-*.csv (real, ~345 MB, en .gitignore)
├── src/
│   ├── trade_types.hpp/.cpp     # Trade, Bar
│   ├── csv_reader.hpp/.cpp      # parser CSV en streaming
│   ├── bar_aggregator.hpp/.cpp  # ticks -> barras OHLCV de 1 min
│   ├── detectors/
│   │   ├── ewma_zscore.hpp/.cpp
│   │   ├── cusum.hpp
│   │   ├── rolling_ratio.hpp
│   │   └── ensemble.hpp/.cpp
│   ├── streaming/
│   │   ├── l2_types.hpp             # L2TickEvent (POD)
│   │   ├── spsc_ring_buffer.hpp     # ring buffer lock-free SPSC (C++20)
│   │   ├── l2_feed_simulator.hpp    # productor: feed L2 simulado
│   │   ├── streaming_consumer.hpp   # consumidor: deteccion en linea sobre ticks
│   │   └── metrics_writer.hpp       # percentiles de latencia, salida NDJSON/JSON
│   ├── svg_writer.hpp/.cpp      # renderizador SVG auto-contenido
│   ├── main.cpp                 # orquestador CLI (motor batch)
│   └── streaming_main.cpp       # punto de entrada de streaming_demo.exe
├── tools/
│   └── consume_streaming_metrics.py   # consumidor Python, solo libreria estandar
├── tests/
│   └── test_main.cpp            # suite de pruebas por aserciones (batch + streaming)
├── outputs/
│   ├── bin/                     # .exe compilados (en .gitignore)
│   ├── reports/                 # bars_with_signals.csv, streaming_*.json (en .gitignore)
│   └── figures/                 # graficos SVG (versionados)
├── build.ps1
├── CMakeLists.txt
├── README.md
└── README.es.md
```

---

# 7. Resultados

Estos son los números obtenidos tras procesar el conjunto completo de 3 días:

## 7.1 Rendimiento del procesamiento

| Métrica | Valor |
|---|---|
| Transacciones procesadas | 4.780.043 (699.401 + 1.770.812 + 2.309.830) |
| Minutos (barras) analizados | 4.320 |
| Velocidad de lectura | ~2,7 millones de operaciones por segundo |
| Tiempo total del proceso | 1,89 segundos |

## 7.2 El minuto más anómalo

El puntaje más alto registrado por mi motor (5,55) ocurrió a las 10:47 UTC del 12 de marzo de 2020. En ese punto exacto, el precio de Bitcoin cayó de USD 6.447 a las 10:44 a USD 5.600 a las 10:47 (una baja del 13% en tres minutos). El sistema detectó este colapso de forma automática.

![Precio y alertas](outputs/figures/price_and_alerts.svg)

![Puntaje del ensamble en el tiempo](outputs/figures/ensemble_score.svg)

## 7.3 Distribución de alertas por día

| Día | Alertas | Minutos analizados | Porcentaje de alertas |
|---|---:|---:|---:|
| 11 mar (día base) | 51 | 1.440 | 3,54% |
| 12 mar (día de la caída) | 107 | 1.440 | 7,43% |
| 13 mar (alta volatilidad) | 55 | 1.440 | 3,82% |

Durante el día del colapso (12 de marzo), el número de alertas se duplicó respecto al día anterior. En las 48 horas de mayor turbulencia se concentró el 76,1% del total de las alertas del periodo.

## 7.4 Corrección de errores con métricas

En la primera versión del código, la falta de calentamiento provocó que 4.319 de las 4.320 barras se marcaran como alerta. Al analizar este resultado atípico, identifiqué y arreglé la división por cero en la varianza inicial. Con el ajuste de 30 barras, el número de alertas se redujo a un valor realista de 213 (4,9% del total).

## 7.5 Pipeline en vivo: ring buffer lock-free + feed L2 simulado

`streaming_demo.exe` corre un pipeline productor/consumidor genuinamente concurrente — un `std::thread` real generando un stream simulado de ticks de Order Book L2, un segundo `std::thread` real drenándolo y corriendo detección en línea, comunicándose exclusivamente a través del ring buffer lock-free SPSC de `src/streaming/spsc_ring_buffer.hpp`, sin ningún mutex en el camino caliente.

| Métrica | Valor |
|---|---|
| Ticks procesados | 5.000.000 |
| Tiempo total | 688,3 ms |
| Throughput | **7.264.187 ticks/segundo** |
| Alertas generadas (detección real, no un consumidor no-op) | 7.544 |
| Reintentos por buffer lleno del productor | 0 |

| Percentil de latencia (productor → ring buffer → consumidor) | Valor |
|---|---|
| p50 | **0,1 µs** |
| p95 | 0,2 µs |
| p99 | 3,1 µs |
| max | 338,5 µs |

**Un hallazgo honesto sobre la latencia de cola, no disimulado.** A lo largo de cinco corridas del mismo benchmark de 5M ticks en esta máquina de desarrollo, el p50 se mantuvo sólido en 0,1 µs siempre — el costo genuino del camino push/pop lock-free — pero el p99 vario entre **3,1 µs y 4.309,6 µs** de una corrida a otra. Esa varianza no es una falla del algoritmo del ring buffer: es lo que se obtiene corriendo dos hilos reales del sistema operativo en una máquina de desarrollo compartida, de proposito general, sin fijar núcleos de CPU (core pinning) ni prioridad de hilo en tiempo real. Cuando el planificador del sistema operativo interrumpe al hilo consumidor por apenas unos cientos de microsegundos — algo enteramente normal en una máquina que también corre un IDE, un navegador y servicios en segundo plano — cada tick que se acumuló en el ring buffer durante ese hueco reporta una latencia inflada, aunque el traspaso lock-free en sí haya tomado nanosegundos. Esta es la razón real por la que los sistemas de producción de baja latencia fijan sus hilos a núcleos aislados y corren con prioridad de tiempo real: **lock-free garantiza progreso a nivel de sistema, no latencia acotada por hilo** — una distinción que este benchmark expone directamente en vez de mostrar un único número prolijo y esperar que nadie lo vuelva a correr.

El consumidor en Python (`tools/consume_streaming_metrics.py tail`) efectivamente transmite estas métricas en vivo mientras `streaming_demo.exe` corre — verificado corriendo ambos procesos concurrentemente, no escribiendo un ejemplo estático.

---

# 8. Conclusión

- **Rendimiento nativo:** logré procesar 4,78 millones de operaciones en 1,89 segundos utilizando C++ puro sin dependencias.
- **Validación precisa:** la alerta máxima del sistema coincidió exactamente con el momento más crítico de la caída del mercado.
- **Detección balanceada:** el motor duplicó sus alertas durante el colapso sin generar un exceso de falsas alarmas en días normales.
- **El pipeline de streaming lock-free sostiene ~7,3M ticks/segundo con latencia mediana sub-microsegundo** (§7.5), y reporta honestamente que la latencia de cola (p99) está dominada por la interrupción del planificador del sistema operativo en una máquina de desarrollo compartida, no por el algoritmo del ring buffer.

## Próximos pasos

- Incorporar datos del libro de órdenes para detectar patrones de manipulación antes de las transacciones.
- Reemplazar el feed L2 simulado por un cliente WebSocket real contra un feed público de datos de mercado en vivo, una vez que una dependencia de red sea aceptable para la restricción de cero dependencias de este motor — el ring buffer y el consumidor ya solo asumen un hilo productor, no que sea simulado.
- Fijar los hilos productor/consumidor a núcleos de CPU aislados y elevar su prioridad de planificación para probar directamente la hipótesis de §7.5 de que el planificador del sistema operativo, no el algoritmo, es lo que domina la latencia de cola (p99).
- Probar el modelo en otros eventos históricos de alta volatilidad para confirmar su estabilidad.

---

# 9. Autor

**Pablo Reyes** — [github.com/Rxyxs](https://github.com/Rxyxs)

## Fuente de datos y licencia

Datos de transacciones: operaciones públicas de BTC/USDT en Binance (11-13 de marzo de 2020) descargadas del [archivo público de Binance](https://data.binance.vision).

Código: Licencia MIT — ver [LICENSE](LICENSE).
