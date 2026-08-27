"""Consumidor Python de las metricas que produce streaming_demo.exe.

El motor de streaming (C++) escribe dos artefactos, sin ninguna
dependencia de red ni de un broker de mensajes -- el mismo espiritu de
cero dependencias externas del resto del repo, en el lado Python tambien
(solo libreria estandar):

  - outputs/reports/streaming_metrics.ndjson: una linea JSON por
    instantanea periodica, escrita en append durante la corrida. Este
    script puede seguirla en vivo ("tail -f") mientras streaming_demo.exe
    esta corriendo en otra terminal.
  - outputs/reports/streaming_benchmark.json: el resumen final (throughput,
    percentiles de latencia p50/p95/p99 en microsegundos), escrito una
    sola vez al terminar la corrida.

Uso:
    python tools/consume_streaming_metrics.py summary
    python tools/consume_streaming_metrics.py tail
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
NDJSON_PATH = BASE / "outputs" / "reports" / "streaming_metrics.ndjson"
SUMMARY_PATH = BASE / "outputs" / "reports" / "streaming_benchmark.json"


def print_summary() -> None:
    if not SUMMARY_PATH.exists():
        print(f"No existe {SUMMARY_PATH} todavia -- corre outputs/bin/streaming_demo.exe primero.")
        sys.exit(1)

    data = json.loads(SUMMARY_PATH.read_text())
    latency = data["ring_to_consumer_latency_us"]

    print("=== Resumen del pipeline de streaming (consumido desde Python) ===")
    print(f"Ticks procesados:        {data['n_ticks']:,}")
    print(f"Tiempo total:            {data['elapsed_ms']:.1f} ms")
    print(f"Throughput:              {data['throughput_ticks_per_sec']:,.0f} ticks/seg")
    print(f"Alertas generadas:       {data['alerts_total']:,}")
    print(f"Reintentos buffer lleno: {data['producer_full_buffer_retries']:,}")
    print("\nLatencia productor -> consumidor (ring buffer, microsegundos):")
    print(f"  p50: {latency['p50']:.2f} us")
    print(f"  p95: {latency['p95']:.2f} us")
    print(f"  p99: {latency['p99']:.2f} us")
    print(f"  max: {latency['max']:.2f} us")


def tail_ndjson(poll_interval_s: float = 0.2) -> None:
    # flush=True en cada print: stdout se buferiza por bloque (no por linea)
    # cuando no es un terminal interactivo -- sin flush explicito, un
    # consumidor real leyendo esta salida por una tuberia (o un usuario
    # matando el proceso antes de que el buffer se llene) no veria nada.
    print(f"Siguiendo {NDJSON_PATH} (Ctrl+C para salir)...", flush=True)
    last_size = 0
    try:
        while True:
            if NDJSON_PATH.exists():
                text = NDJSON_PATH.read_text()
                if len(text) > last_size:
                    new_lines = text[last_size:].splitlines()
                    for line in new_lines:
                        if not line.strip():
                            continue
                        snapshot = json.loads(line)
                        print(
                            f"[{snapshot['elapsed_ms']:>9.1f} ms] "
                            f"ticks={snapshot['ticks_processed']:>10,} "
                            f"throughput={snapshot['throughput_ticks_per_sec']:>12,.0f}/seg "
                            f"alertas={snapshot['alerts_total']:>6,} "
                            f"ring_buffer~={snapshot['ring_buffer_size_approx']:>6,}",
                            flush=True,
                        )
                    last_size = len(text)
            time.sleep(poll_interval_s)
    except KeyboardInterrupt:
        print("\nDetenido.", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["summary", "tail"], help="'summary' imprime el resumen final; 'tail' sigue las instantaneas en vivo")
    args = parser.parse_args()

    if args.mode == "summary":
        print_summary()
    else:
        tail_ndjson()


if __name__ == "__main__":
    main()
