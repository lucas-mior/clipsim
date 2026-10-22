#!/usr/bin/env python3

import sys
import re
import subprocess
from pathlib import Path
from collections import defaultdict

# Increase recursion depth for deeply nested call graphs
sys.setrecursionlimit(200000)

def resolve_name(line, name_map):
    """Resolves Callgrind's string compression: e.g., 'fn=(1) main'"""
    val = line.split('=', 1)[1].strip()
    match = re.match(r'^\((\d+)\)(?:\s+(.*))?$', val)
    if match:
        id_part = match.group(1)
        name_part = match.group(2)
        if name_part:
            name_map[id_part] = name_part
            return name_part
        else:
            return name_map.get(id_part, f"unknown_fn_{id_part}")
    return val

def parse_callgrind(filepath):
    name_map = {}
    file_map = {}
    function_sources = defaultdict(set)
    self_costs = defaultdict(int)
    edge_costs = defaultdict(int)
    call_counts = defaultdict(int)

    num_events = 1
    ir_offset = -1

    current_fn = None
    current_file = None
    current_cfn = None
    current_callee_file = None
    is_call = False
    pending_call_count = 0

    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            if line.startswith('events:'):
                events = line.split()[1:]
                num_events = len(events)
                idx = 0
                for i, ev in enumerate(events):
                    if ev.lower() == 'ir':
                        idx = i
                        break
                ir_offset = -num_events + idx
                continue

            if line.startswith('fl='):
                current_file = resolve_name(line, file_map)
                continue

            if line.startswith('cfl='):
                current_callee_file = resolve_name(line, file_map)
                continue

            if line.startswith('fn='):
                current_fn = resolve_name(line, name_map)
                if current_file:
                    function_sources[current_fn].add(current_file)
                current_cfn = None
                current_callee_file = None
                is_call = False
                continue

            if line.startswith('cfn='):
                current_cfn = resolve_name(line, name_map)
                if current_callee_file:
                    function_sources[current_cfn].add(current_callee_file)
                continue

            if line.startswith('calls='):
                is_call = True
                match = re.match(r'^calls=(\d+)', line)
                if match:
                    pending_call_count = int(match.group(1))
                else:
                    pending_call_count = 0
                continue

            # Standard cost line starts with a digit or position modifier (+, -, *)
            if line[0].isdigit() or line[0] in '-+*':
                tokens = line.split()
                try:
                    # Extract the primary metric (usually Ir) based on its offset
                    if len(tokens) >= num_events:
                        cost = int(tokens[ir_offset])
                    else:
                        cost = int(tokens[-1])
                except ValueError:
                    cost = 0

                if is_call:
                    if current_fn and current_cfn:
                        edge_costs[(current_fn, current_cfn)] += cost
                        call_counts[current_cfn] += pending_call_count
                    is_call = False
                    pending_call_count = 0
                else:
                    if current_fn:
                        self_costs[current_fn] += cost

    return self_costs, edge_costs, call_counts, function_sources

def find_sccs(nodes, adj):
    """Tarjan's algorithm to find Strongly Connected Components (cycles)"""
    index = 0
    indices = {}
    lowlinks = {}
    on_stack = set()
    stack = []
    sccs = []

    def strongconnect(v):
        nonlocal index
        indices[v] = index
        lowlinks[v] = index
        index += 1
        stack.append(v)
        on_stack.add(v)

        for w in adj[v]:
            if w not in indices:
                strongconnect(w)
                lowlinks[v] = min(lowlinks[v], lowlinks[w])
            elif w in on_stack:
                lowlinks[v] = min(lowlinks[v], indices[w])

        if lowlinks[v] == indices[v]:
            scc = []
            while True:
                w = stack.pop()
                on_stack.remove(w)
                scc.append(w)
                if w == v:
                    break
            sccs.append(scc)

    for v in nodes:
        if v not in indices:
            strongconnect(v)

    return sccs

def annotate_top_sources(filepath, rankings, function_sources):
    project_root = Path(__file__).resolve().parent
    output_dir = Path.cwd()
    seen_sources = set()
    output_sources = {}

    for ranking in rankings:
        for name, _ in ranking[:5]:
            for source in sorted(function_sources.get(name, ())):
                if not source.endswith('.c'):
                    continue

                path = Path(source)
                if not path.is_absolute():
                    path = project_root / path
                path = path.resolve()
                if not path.is_file():
                    continue
                try:
                    relative = path.relative_to(project_root)
                except ValueError:
                    # Skip libc and any other external source trees.
                    continue

                if path in seen_sources:
                    continue
                seen_sources.add(path)

                output_name = f"profile-{path.name}.c.txt"
                if output_name in output_sources:
                    print(
                        f"Skipping {relative}: {output_name} already belongs to "
                        f"{output_sources[output_name]}",
                        file=sys.stderr,
                    )
                    continue
                output_sources[output_name] = relative

                with (output_dir / output_name).open('w') as output:
                    subprocess.run(
                        ['callgrind_annotate', str(Path(filepath).resolve()),
                         source],
                        cwd=project_root,
                        stdout=output,
                        check=True,
                    )


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <callgrind.out.file>")
        sys.exit(1)

    filepath = sys.argv[1]
    self_costs, edge_costs, call_counts, function_sources = parse_callgrind(filepath)

    # 1. Build the call graph
    nodes = set(self_costs.keys())
    for u, v in edge_costs.keys():
        nodes.add(u)
        nodes.add(v)

    adj = defaultdict(list)
    for u, v in edge_costs.keys():
        adj[u].append(v)

    # 2. Detect cycles (SCCs)
    sccs = find_sccs(nodes, adj)

    # 3. Calculate Cycle-Aware Inclusive Costs (The KCachegrind method)
    # The inclusive cost of an SCC is the sum of its internal self-costs 
    # PLUS the call costs of all edges leaving the SCC.
    incl_costs = {}
    for scc in sccs:
        scc_set = set(scc)
        cycle_self_cost = sum(self_costs.get(u, 0) for u in scc)
        cycle_out_cost = 0

        for u in scc:
            for v in adj[u]:
                if v not in scc_set:
                    cycle_out_cost += edge_costs.get((u, v), 0)

        scc_incl_cost = cycle_self_cost + cycle_out_cost

        # Distribute the total cycle cost to all functions within the cycle
        for u in scc:
            incl_costs[u] = scc_incl_cost

    # 4. Sort and Print Results
    total_time = sum(self_costs.values())
    if total_time == 0: total_time = 1  # Avoid division by zero

    sorted_incl = sorted(incl_costs.items(), key=lambda x: x[1], reverse=True)
    sorted_excl = sorted(self_costs.items(), key=lambda x: x[1], reverse=True)
    sorted_calls = sorted(
        ((name, self_costs.get(name, 0)) for name in nodes
         if (self_costs.get(name, 0) / total_time) * 100 >= 1.0),
        key=lambda x: call_counts.get(x[0], 0),
        reverse=True
    )

    def print_top(items, title, min_pct):
        print(f"\n{'='*70}")
        print(f"{title}")
        print(f"{'='*70}")
        print(f"{'Cost (Ir)':>15}  {'%':>6}  {'Calls':>12}  Function")
        print(f"{'-'*15}  {'-'*6}  {'-'*12}  {'-'*45}")
        for name, cost in items:
            pct = (cost / total_time) * 100
            if min_pct is not None and pct < min_pct:
                break
            calls = call_counts.get(name, 0)
            if min_pct is None and calls == 0:
                break
            # Truncate extremely long C++ signatures for terminal readability
            if len(name) > 65: name = name[:62] + "..."
            print(f"{cost:15,d}  {pct:5.2f}%  {calls:12,d}  {name}")

    print_top(sorted_incl, "TOP FUNCTIONS BY INCLUSIVE TIME (Cycle-Aware)", 1.0)
    print_top(sorted_excl, "TOP FUNCTIONS BY EXCLUSIVE TIME (Self Cost)", 1.0)
    print_top(sorted_calls, "TOP FUNCTIONS BY NUMBER OF CALLS", None)

    # Use exactly the entries eligible for the three displayed rankings.
    top_calls = [(name, cost) for name, cost in sorted_calls
                 if call_counts.get(name, 0) > 0][:5]
    top_excl = [(name, cost) for name, cost in sorted_excl
                if (cost / total_time) * 100 >= 1.0][:5]
    top_incl = [(name, cost) for name, cost in sorted_incl
                if (cost / total_time) * 100 >= 1.0][:5]
    annotate_top_sources(
        filepath, (top_calls, top_excl, top_incl), function_sources
    )

if __name__ == "__main__":
    main()
