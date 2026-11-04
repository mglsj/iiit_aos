import argparse
import time

from pyspark.sql import SparkSession


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Deadlock detection on an HPC RAG log using PySpark")
    parser.add_argument("log_path", help="path to the hpc_rag log file")
    parser.add_argument("no_of_cores", type=int, help="number of cores for the Spark job")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    spark = (
        SparkSession.builder.master(f"local[{args.no_of_cores}]")
        .appName("AOS_Assignment4")
        .getOrCreate()
    )
    sc = spark.sparkContext

    start = time.time()
    lines = sc.textFile(args.log_path)

    # Q1: RAG construction and graph metrics
    # Q2: cycle detection (MapReduce path propagation, max cycle length 25)
    # Q3: deadlock formation timeline and timing
    # Q4: in-degree / out-degree hotspots
    _ = lines

    elapsed = time.time() - start
    print(f"# Total execution time (in seconds) ... {elapsed:.2f}")

    spark.stop()


if __name__ == "__main__":
    main()
