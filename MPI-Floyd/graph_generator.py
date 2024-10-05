import random
import argparse

def generate_graph(V, E):
    edges = set()
    while len(edges) < E:
        a = random.randint(1, V - 1)
        b = random.randint(a + 1, V)
        weight = random.randint(1, 100)
        edges.add((a, b, weight))
    return edges

def main():
    parser = argparse.ArgumentParser(description="Generate a graph for Floyd's algorithm.")
    parser.add_argument('-v', '--vertices', type=int, help='Number of vertices in the graph', required=False)
    parser.add_argument('-e', '--edges', type=int, help='Number of edges in the graph', required=False)
    parser.add_argument('-s', '--seed', type=int, help='Seed for random number generator', default=None)
    args = parser.parse_args()
    max_V = 5000
    max_E = 1000000

    if args.seed is not None:
        random.seed(args.seed)

    V = args.vertices if args.vertices and args.vertices >= max_V else max_V
    E = args.edges if args.edges and args.edges >= max_E and args.edges <= V**2 // 2 else random.randint(max_E, V**2 // 2)

    edges = generate_graph(V, E)

    with open('Floyd.input', 'w') as f:
        f.write(f"{V}\n")
        for edge in edges:
            f.write(f"{edge[0]} {edge[1]} {edge[2]}\n")

    print(f"Generated graph with {V} vertices and {len(edges)} edges in 'Floyd.input'.")

if __name__ == "__main__":
    main()
