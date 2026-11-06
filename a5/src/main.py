import argparse
import time

from pyspark.sql import SparkSession


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Deadlock detection on an HPC RAG log using PySpark"
    )
    parser.add_argument("log_path", help="path to the hpc_rag log file")
    parser.add_argument(
        "no_of_cores", type=int, help="number of cores for the Spark job"
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    spark = (
        SparkSession.builder.master(f"local[{args.no_of_cores}]")
        .appName("2026201063 Assignment 5")
        .getOrCreate()
    )
    sc = spark.sparkContext
    sc.setLogLevel("ERROR")

    lines = sc.textFile(args.log_path)

    # Q1: RAG construction and graph metrics
    def to_edge(line):
        try:
            timestamp, level, p, r = line.strip().split("\t")
            if level == "WAIT":
                return [((p, r), timestamp)]
            elif level == "HOLD":
                return [((r, p), timestamp)]
        except ValueError:
            pass
        return []

    timed_edges = lines.flatMap(to_edge).reduceByKey(lambda x, y: min(x, y)).cache()
    edges = timed_edges.map(lambda e: e[0]).cache()

    procs = edges.map(lambda e: e[0] if e[0].startswith("P") else e[1]).distinct()
    res = edges.map(lambda e: e[1] if e[1].startswith("R") else e[0]).distinct()

    print(procs.count())
    print(res.count())
    print(edges.count())

    # Q2: cycle detection (MapReduce path propagation, max cycle length 25)
    start = time.time()

    paths = procs.map(lambda proc: (proc,))
    cycles = sc.parallelize([])

    edges_keyed = edges.map(lambda e: (e[0], e[1]))

    for _ in range(25):
        paths_keyed = paths.map(lambda path: (path[-1], path))
        joined = paths_keyed.join(edges_keyed)
        paths = joined.map(lambda item: item[1][0] + (item[1][1],))

        detected_cycles = paths.filter(lambda path: path[0] == path[-1])
        cycles = cycles.union(detected_cycles)

        paths = paths.filter(lambda path: path[-1] not in path[:-1]).cache()
        if paths.isEmpty():
            break

    def canonical(path):
        path = path[:-1]
        min_ele = min(path)
        i = path.index(min_ele)
        return path[i:] + path[:i]

    cycles = cycles.map(canonical).distinct().cache()
    sorted_cycles = sorted(cycles.collect(), key=lambda c: (len(c), c))

    procs_in_cycle = set()

    for cycle in sorted_cycles:
        for ele in cycle:
            if ele.startswith("P"):
                procs_in_cycle.add(ele)

    elapsed = time.time() - start
    print(f"{elapsed:.2f}")

    print(len(sorted_cycles))
    for cycle in sorted_cycles:
        print(*cycle)

    print(len(procs_in_cycle))
    print(*sorted(procs_in_cycle))

    # Q3: deadlock formation timeline and timing
    def cycle_edges(cycle):
        return list(zip(cycle, cycle[1:] + cycle[:1]))

    cycle_edge_pairs = cycles.flatMap(lambda c: [(e, c) for e in cycle_edges(c)])
    cycle_edge_times = cycle_edge_pairs.join(timed_edges)
    cycle_times = cycle_edge_times.map(lambda item: item[1]).reduceByKey(
        lambda x, y: max(x, y)
    )
    day_count = cycle_times.map(lambda item: (item[1].split()[0], 1)).reduceByKey(
        lambda x, y: x + y
    )

    for pair in sorted(day_count.collect()):
        print(*pair)

    # Q4: in-degree / out-degree hotspots
    in_degree = edges.map(lambda e: (e[1], 1)).reduceByKey(lambda x, y: x + y)
    in_res = in_degree.filter(lambda n: n[0].startswith("R"))
    for pair in in_res.takeOrdered(5, key=lambda n: (-n[1], n[0])):
        print(*pair)

    out_degree = edges.map(lambda e: (e[0], 1)).reduceByKey(lambda x, y: x + y)
    out_procs = out_degree.filter(lambda n: n[0].startswith("P"))
    for pair in out_procs.takeOrdered(5, key=lambda n: (-n[1], n[0])):
        print(*pair)

    spark.stop()


if __name__ == "__main__":
    main()
