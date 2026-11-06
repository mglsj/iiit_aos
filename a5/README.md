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

## Running on ADA

One-time setup on the cluster:

```bash
python3 -m venv ~/a5venv && source ~/a5venv/bin/activate && pip install pyspark==3.4.1
```

Copy the project up (from the local machine):

```bash
rsync -av --exclude .devenv --exclude .venv --exclude .ipynb_checkpoints ./ <user>@ada-gw1.iiit.ac.in:~/a5/
```

Interactive run on a compute node (not the gateway):

```bash
sint3304 -c 8
source ~/a5venv/bin/activate && cd ~/a5/src && spark-submit main.py ../test_files/sample_hpc_rag.log 8
```

Scalability timings for Q3.2 (1, 2, 4, 8 cores), written to `scaling_<jobid>.out`:

```bash
sbatch scaling.sh ../test_files/sample_hpc_rag.log
```

## Jupyter

JupyterLab and the IPython kernel are dev dependencies. Start the server with:

```bash
uv run jupyter lab
```

Notebooks use the `python3` kernel from the project environment, which runs Python 3.10 with PySpark 3.4.1.
