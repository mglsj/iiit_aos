import argparse
import time

from pyspark.sql import SparkSession

MAX_CYCLE_LEN = 25


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Deadlock detection on an HPC RAG log using PySpark"
    )
    parser.add_argument("log_path", help="path to the hpc_rag log file")
    parser.add_argument(
        "no_of_cores", type=int, help="number of cores for the Spark job"
    )
    return parser.parse_args()


def to_edge(line):
    parts = line.split()
    if len(parts) < 5:
        return []
    timestamp = parts[0] + " " + parts[1]
    level, p, r = parts[2].upper(), parts[3], parts[4]
    if level == "WAIT":
        return [((p, r), timestamp)]
    if level == "HOLD":
        return [((r, p), timestamp)]
    return []


def main() -> None:
    args = parse_args()
    cores = args.no_of_cores
    spark = (
        SparkSession.builder.master(f"local[{cores}]")
        .appName("2026201063 Assignment 5")
        .config("spark.default.parallelism", str(cores))
        .getOrCreate()
    )
    sc = spark.sparkContext
    sc.setLogLevel("ERROR")

    lines = sc.textFile(args.log_path, minPartitions=cores)

    # Q1: RAG construction and graph metrics
    timed_edges = lines.flatMap(to_edge).reduceByKey(min).cache()
    edges = timed_edges.keys().cache()

    nodes = edges.flatMap(lambda e: e).distinct().cache()
    print(nodes.filter(lambda n: n.startswith("P")).count())
    print(nodes.filter(lambda n: n.startswith("R")).count())
    print(edges.count())

    # Q2: cycle detection
    start = time.time()

    # Trim edges touching nodes with no in-edge or no out-edge: such nodes
    # cannot lie on a cycle. Repeat until the edge set stops shrinking.
    live = edges
    n_live = live.count()
    while True:
        has_out = live.map(lambda e: (e[0], None)).distinct()
        has_in = live.map(lambda e: (e[1], None)).distinct()
        keep = sc.broadcast(set(has_out.join(has_in).keys().collect()))
        live = live.filter(lambda e: e[0] in keep.value and e[1] in keep.value).cache()
        n = live.count()
        if n == n_live:
            break
        n_live = n

    edges_keyed = live.partitionBy(cores).cache()

    # A path is only extended to nodes greater than its start node, so each
    # cycle is grown exactly once, from its smallest node. Since "P" < "R"
    # that node is always a process, and the cycle is already canonical.
    paths = (
        live.keys().filter(lambda n: n.startswith("P")).distinct().map(lambda n: (n,))
    )
    found = []
    for _ in range(MAX_CYCLE_LEN):
        joined = paths.map(lambda path: (path[-1], path)).join(edges_keyed)
        extended = (
            joined.map(lambda kv: kv[1][0] + (kv[1][1],))
            .filter(
                lambda path: (
                    path[-1] == path[0]
                    or (path[-1] > path[0] and path[-1] not in path[1:-1])
                )
            )
            .cache()
        )
        found.extend(extended.filter(lambda path: path[-1] == path[0]).collect())
        nxt = extended.filter(lambda path: path[-1] != path[0]).cache()
        empty = nxt.count() == 0  # materialise before dropping the parents
        paths.unpersist()
        extended.unpersist()
        paths = nxt
        if empty:
            break

    sorted_cycles = sorted((c[:-1] for c in found), key=lambda c: (len(c), c))
    procs_in_cycle = sorted(
        {n for cycle in sorted_cycles for n in cycle if n.startswith("P")}
    )

    elapsed = time.time() - start
    print(f"{elapsed:.2f}")

    print(len(sorted_cycles))
    for cycle in sorted_cycles:
        print(*cycle)

    print(len(procs_in_cycle))
    print(*procs_in_cycle)

    # Q3.1: deadlock formation timeline
    def cycle_edges(cycle):
        return list(zip(cycle, cycle[1:] + cycle[:1]))

    cycles = sc.parallelize(sorted_cycles)
    day_count = (
        cycles.flatMap(lambda c: [(e, c) for e in cycle_edges(c)])
        .join(timed_edges)
        .values()
        .reduceByKey(max)
        .map(lambda item: (item[1].split()[0], 1))
        .reduceByKey(lambda x, y: x + y)
    )
    for pair in sorted(day_count.collect()):
        print(*pair)

    # Q4: in-degree / out-degree hotspots
    def top5(pairs):
        return pairs.takeOrdered(5, key=lambda n: (-n[1], n[0]))

    in_res = (
        edges.filter(lambda e: e[1].startswith("R"))
        .map(lambda e: (e[1], 1))
        .reduceByKey(lambda x, y: x + y)
    )
    for pair in top5(in_res):
        print(*pair)

    out_procs = (
        edges.filter(lambda e: e[0].startswith("P"))
        .map(lambda e: (e[0], 1))
        .reduceByKey(lambda x, y: x + y)
    )
    for pair in top5(out_procs):
        print(*pair)

    spark.stop()


if __name__ == "__main__":
    main()
