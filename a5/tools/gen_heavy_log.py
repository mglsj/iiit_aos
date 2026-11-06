"""Generate a synthetic hpc_rag log whose cycle search is compute-heavy.

Structure (all within the assignment's rules):
  * every resource has a single instance (at most one HOLD per resource)
  * every node is on at most one deadlock cycle
  * at most `--lines` records (default 50,000)

Layout, in increasing ID order (IDs are zero-padded, so string order = numeric):
  entry cycles -> layered DAG (depth D, width W, fan-out K) -> exit cycles -> noise
Entry-cycle processes wait on layer-0 resources; the last DAG layer waits on
exit-cycle resources. Every DAG node therefore has in- and out-edges and
survives dead-end trimming, but the DAG has no cycle, so the path search has
to enumerate ~W * K**D simple paths without finding anything new there.

Usage: python3 tools/gen_heavy_log.py OUT [--depth 9] [--width 30] [--fanout 3]
"""

import argparse
import datetime
import random


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--depth", type=int, default=9, help="DAG layers")
    ap.add_argument("--width", type=int, default=30, help="processes per layer")
    ap.add_argument("--fanout", type=int, default=3, help="waits per DAG process")
    ap.add_argument("--lines", type=int, default=50_000)
    ap.add_argument("--noise-procs", type=int, default=150)
    ap.add_argument("--seed", type=int, default=7)
    a = ap.parse_args()
    rnd = random.Random(a.seed)

    counter = {"P": 0, "R": 0}

    def new(kind):
        counter[kind] += 1
        return f"{kind}{counter[kind]:04d}"

    base = datetime.datetime(2025, 11, 1)
    events = []  # (datetime, level, process, resource)

    def ev(level, p, r, day_lo=0, day_hi=7):
        t = base + datetime.timedelta(
            seconds=rnd.randint(day_lo * 86400, day_hi * 86400 - 1)
        )
        events.append((t, level, p, r))

    def cycle(length, day):
        # all edges of this cycle appear by the end of `day`, so it forms then
        ps = [new("P") for _ in range(length)]
        rs = [new("R") for _ in range(length)]
        for i in range(length):
            ev("HOLD", ps[i], rs[i], 0, day + 1)
            ev("WAIT", ps[i], rs[(i + 1) % length], 0, day + 1)
        return ps, rs

    entry = [cycle(3, 1), cycle(4, 3)]

    layers = []  # each layer: list of (process, held resource)
    for _ in range(a.depth):
        layer = []
        for _ in range(a.width):
            p, r = new("P"), new("R")
            ev("HOLD", p, r)
            layer.append((p, r))
        layers.append(layer)

    exit_ = [cycle(5, 3), cycle(6, 5)]
    isolated = cycle(8, 6)  # unrelated deadlock, for a varied report

    # entry processes feed layer 0
    for ps, _ in entry:
        for p in ps:
            for _, r in rnd.sample(layers[0], a.fanout):
                ev("WAIT", p, r)
    # DAG edges only go forward one layer
    for j, layer in enumerate(layers):
        targets = (
            [r for _, r in layers[j + 1]]
            if j + 1 < len(layers)
            else [r for _, rs in exit_ for r in rs]
        )
        for p, _ in layer:
            for r in rnd.sample(targets, a.fanout):
                ev("WAIT", p, r)

    # Noise: processes that hold nothing (in-degree 0, so never on a cycle)
    # waiting on arbitrary resources, plus waits on never-held resources.
    all_r = [f"R{i:04d}" for i in range(1, counter["R"] + 1)]
    free_r = [new("R") for _ in range(40)]
    noise_p = [new("P") for _ in range(a.noise_procs)]
    for p in noise_p:
        for r in rnd.sample(all_r + free_r, rnd.randint(5, 25)):
            ev("WAIT", p, r)

    # Pad with repeated WAITs at later times (first appearance is unchanged).
    waits = [e for e in events if e[1] == "WAIT"]
    while len(events) < a.lines:
        t, _, p, r = rnd.choice(waits)
        events.append((t + datetime.timedelta(minutes=rnd.randint(1, 600)), "WAIT", p, r))
    if len(events) > a.lines:
        raise SystemExit(
            f"graph needs {len(events)} lines but --lines is {a.lines}; "
            "lower --width/--depth/--fanout/--noise-procs or raise --lines"
        )

    events.sort()
    with open(a.out, "w") as f:
        for t, level, p, r in events:
            f.write(f"{t:%Y-%m-%d %H:%M:%S}\t{level}\t{p}\t{r}\n")

    paths = a.width * a.fanout**a.depth
    print(f"wrote {len(events)} lines to {a.out}; ~{paths:,} paths in the deepest round")


if __name__ == "__main__":
    main()
