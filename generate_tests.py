"""
Generira krizne testne vhode za algoritem ortogonalne CC-mnozice.
Vsak test pokriva drug scenarij, ki bi lahko razkril slabost algoritma.
"""

import random
import math
import os

BASE = "input"
os.makedirs(BASE, exist_ok=True)


def write_matrix(filename, mat):
    n = len(mat)
    with open(filename, "w") as f:
        f.write(f"{n}\n")
        for row in mat:
            f.write(" ".join(map(str, row)) + "\n")


def empty(n):
    return [[0] * n for _ in range(n)]


def make_path(n):
    mat = empty(n)
    for i in range(n - 1):
        mat[i][i + 1] = mat[i + 1][i] = 1
    return mat


def make_complete(n):
    mat = empty(n)
    for i in range(n):
        for j in range(n):
            if i != j:
                mat[i][j] = 1
    return mat


def make_star(n_leaves):
    n = n_leaves + 1
    mat = empty(n)
    for i in range(1, n):
        mat[0][i] = mat[i][0] = 1
    return mat


def make_disjoint_edges(n_pairs):
    n = n_pairs * 2
    mat = empty(n)
    for i in range(n_pairs):
        mat[2 * i][2 * i + 1] = mat[2 * i + 1][2 * i] = 1
    return mat


def make_random(n, p, seed):
    random.seed(seed)
    mat = empty(n)
    for i in range(n):
        for j in range(i + 1, n):
            if random.random() < p:
                mat[i][j] = mat[j][i] = 1
    return mat


def make_grid(rows, cols):
    n = rows * cols
    mat = empty(n)
    for r in range(rows):
        for c in range(cols):
            v = r * cols + c
            if c + 1 < cols:
                w = r * cols + (c + 1)
                mat[v][w] = mat[w][v] = 1
            if r + 1 < rows:
                w = (r + 1) * cols + c
                mat[v][w] = mat[w][v] = 1
    return mat


def make_petersen():
    # outer: 0-1-2-3-4-0, inner pentagram: 5-7-9-6-8-5, spokes: i - (i+5)
    n = 10
    mat = empty(n)
    outer = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 0)]
    inner = [(5, 7), (7, 9), (9, 6), (6, 8), (8, 5)]
    spokes = [(0, 5), (1, 6), (2, 7), (3, 8), (4, 9)]
    for u, v in outer + inner + spokes:
        mat[u][v] = mat[v][u] = 1
    return mat


def make_multiple_components():
    # path_20 + K_5 + 10_disjoint + star_10 + isolated_5
    # Skupaj 20+5+20+11+5 = 61 vozlisc
    segments = []
    segments.append(("path20", make_path(20)))
    segments.append(("k5", make_complete(5)))
    segments.append(("disjoint10", make_disjoint_edges(10)))
    segments.append(("star10", make_star(10)))
    segments.append(("isolated5", empty(5)))

    total = sum(len(s[1]) for s in segments)
    mat = empty(total)
    offset = 0
    for _, seg in segments:
        k = len(seg)
        for i in range(k):
            for j in range(k):
                mat[offset + i][offset + j] = seg[i][j]
        offset += k
    return mat


def make_cycle(n):
    mat = empty(n)
    for i in range(n):
        mat[i][(i + 1) % n] = mat[(i + 1) % n][i] = 1
    return mat


def make_bipartite_random(n_left, n_right, p, seed):
    random.seed(seed)
    n = n_left + n_right
    mat = empty(n)
    for i in range(n_left):
        for j in range(n_right):
            if random.random() < p:
                mat[i][n_left + j] = mat[n_left + j][i] = 1
    return mat


def make_disjoint_triangles(n_triangles):
    n = n_triangles * 3
    mat = empty(n)
    for t in range(n_triangles):
        a, b, c = 3 * t, 3 * t + 1, 3 * t + 2
        mat[a][b] = mat[b][a] = 1
        mat[b][c] = mat[c][b] = 1
        mat[a][c] = mat[c][a] = 1
    return mat


# Scenarij: pohlepna past -- rob z majhno skodo blokira veliko boljse resitve.
# Zgrajeno kot: eno centralno vozlisce (hub) z veliko listi (pasti),
# plus locena "idealna" struktura disjunktnih robov.
def make_greedy_trap():
    # Hub vozlisce 0 je povezano z vozlisci 1..20.
    # Posebej zanimivi robovi: (1,2), (3,4), ..., (19,20) -- ti so disjunktni.
    # Plus: (0,1), (0,2) -- manjsa skoda, a blokira vse pare.
    # Greedy bo morda izbral (0, x) ker ima nizko skodo, a s tem blokira vec parov.
    # Optimum: vzamemo 10 parov (1,2),(3,4),...,(19,20).
    n = 21
    mat = empty(n)
    # Hub
    for i in range(1, 21):
        mat[0][i] = mat[i][0] = 1
    # Pari (ne smejo biti med seboj povezani, da so kompatibilni)
    for i in range(1, 21, 2):
        mat[i][i + 1] = mat[i + 1][i] = 1
    return mat


tests = [
    ("test11_large_path_n500", make_path(500)),
    ("test12_complete_k50", make_complete(50)),
    ("test13_star_k1_200", make_star(200)),
    ("test14_disjoint_edges_n300", make_disjoint_edges(150)),
    ("test15_sparse_n200_p012", make_random(200, 0.12, 42)),
    ("test16_medium_n150_p028", make_random(150, 0.28, 42)),
    ("test17_grid_15x15", make_grid(15, 15)),
    ("test18_petersen", make_petersen()),
    ("test19_dense_n100_p070", make_random(100, 0.70, 42)),
    ("test20_large_sparse_n300_p010", make_random(300, 0.10, 42)),
    ("test21_multiple_components", make_multiple_components()),
    ("test22_adversarial_medium_n200_p030", make_random(200, 0.30, 42)),
    ("test23_cycle_n200", make_cycle(200)),
    ("test24_bipartite_random_n100", make_bipartite_random(50, 50, 0.3, 42)),
    ("test25_disjoint_triangles_n60", make_disjoint_triangles(20)),
    ("test26_greedy_trap", make_greedy_trap()),
    ("test27_large_path_n1000", make_path(1000)),
    ("test28_sparse_n400_p008", make_random(400, 0.08, 99)),
]

for name, mat in tests:
    path = os.path.join(BASE, name + ".txt")
    write_matrix(path, mat)
    print(f"Generated {path}  (n={len(mat)})")

print(f"\nSkupaj: {len(tests)} testov")
