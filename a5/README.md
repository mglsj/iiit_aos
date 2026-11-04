# Deadlock Detection using MapReduce (PySpark)

CS3.304 Advanced Operating Systems, Assignment 5.

## Dependencies

- Python 3.10 (`.python-version`)
- PySpark 3.4.1 (`pyproject.toml`), managed with uv
- Java 11 (OpenJDK)

The environment is provided by devenv (`devenv.nix`), which supplies uv, Python 3.10 and JDK 11.

## Setup

```bash
devenv shell
```

On entering the shell, `uv sync` installs the project dependencies into devenv's venv at `.devenv/state/venv`.

## Running

Local test run:

```bash
uv run python src/main.py test_files/sample_hpc_rag.log 4
```

Cluster submission (final evaluation):

```bash
spark-submit src/main.py ../test_files/log_file_name no_of_cores
```

The first argument is the log file path and the second is the number of cores.
