# forge-core v0.1 Alpha

## High-Performance CSV Validation & Ingestion Engine

`forge-core` is a low-level systems engineering prototype focused on high-throughput CSV ingestion, structural validation, and bounded-memory processing.

The project is designed to explore efficient file-scanning pipelines using memory-mapped I/O, lightweight validation logic, and forensic observability tooling under constrained memory environments.

---

# Objectives

* Process large datasets with minimal memory overhead
* Validate CSV structure at high throughput
* Detect malformed rows and corruption patterns
* Maintain stable performance under bounded RAM conditions
* Build a modular ingestion architecture for future expansion

---

# Current Capabilities

## Structural Validation

* CSV row validation
* Column-count integrity checks
* Malformed row detection
* Sequential ingestion pipeline

## Observability

* Forensic logging
* Corruption manifest generation
* Byte-offset reporting
* Validation summaries

## Performance Engineering

* Memory-mapped file access (`mmap`)
* Bounded memory execution
* Warm vs cold cache benchmarking
* Linux-focused throughput optimization

---

# Benchmark Snapshot

Environment:

* Acer Nitro 16
* Ubuntu (WSL2)
* C-based ingestion pipeline

Results from current prototype testing:

| Metric                  | Result                              |
| ----------------------- | ----------------------------------- |
| Dataset Size            | 20GB                                |
| Peak Cold Throughput    | ~306 MB/s                           |
| Peak Warm Throughput    | ~867 MB/s                           |
| Rows Processed          | 83.9M+                              |
| Malformed Rows Detected | 73.4M+                              |
| Memory Constraint       | <512MB target during benchmark runs |

Note:
Warm-cache benchmarks reflect Linux page-cache acceleration after repeated reads.

---

# Architecture Overview

## Layer 1 — Metal

Low-level ingestion engine using memory-mapped file access for sequential scanning.

## Layer 2 — Shield

Integrity validation layer responsible for corruption detection and structural verification.

## Layer 3 — Scribe

Observability and forensic logging layer for audit visibility.

## Layer 4 — Sentinel

Schema-aware CSV validation engine for delimiter and column analysis.

---

# Repository Structure

```text
forge-core/
├── src/
├── scripts/
├── benchmarks/
├── logs/
├── tests/
├── docs/
├── README.md
├── ROADMAP.md
└── FORGE_LOG.md
```

---

# Usage

## Run Validation

```bash
./forge-core validate
```

## Run Benchmarks

```bash
./forge-core bench
```

---

# Technical Stack

| Component   | Technology                |
| ----------- | ------------------------- |
| Core Engine | C                         |
| Automation  | Shell Scripts             |
| Environment | Ubuntu / WSL2             |
| File Access | mmap                      |
| Logging     | Custom forensic manifests |

---

# Development Roadmap

## Planned Improvements

* Dynamic schema configuration
* Real-world CSV dataset testing
* Multi-threaded ingestion
* CLI argument expansion
* API integration layer
* Streaming ingestion support
* Benchmark comparison suite
* Structured test coverage

---

# Project Status

Current Status: `Prototype / Experimental Infrastructure`

This repository is an ongoing systems-engineering exploration focused on ingestion performance, structural validation, and scalable data-processing architecture.

---

# License

MIT License
