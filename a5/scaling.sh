#!/bin/bash
#SBATCH -A cs3304
#SBATCH --qos=cs3304
#SBATCH -p u22
#SBATCH -n 1
#SBATCH -c 8
#SBATCH --mem=23G
#SBATCH -t 01:00:00
#SBATCH -o scaling_%j.out
# Q3.2: time Q2 (cycle detection) for 1, 2, 4 and 8 cores.
# Usage: [DRIVER_MEM=12g] [CORES="1 2 4 8"] sbatch scaling.sh [log_path relative to src/]
# Full program output for each run goes to scaling_<jobid>_c<cores>.txt.
LOG=${1:-../test_files/sample_hpc_rag.log}
MEM=${DRIVER_MEM:-4g}
OUT=${SLURM_SUBMIT_DIR:-$PWD}

source ~/a5venv/bin/activate
cd ~/a5/src
echo "log=$LOG driver_mem=$MEM"
for c in ${CORES:-1 2 4 8}; do
  f="$OUT/scaling_${SLURM_JOB_ID}_c${c}.txt"
  spark-submit --driver-memory "$MEM" main.py "$LOG" "$c" >"$f" 2>/dev/null
  echo "cores=$c exit=$? time=$(sed -n 4p "$f")"
done
