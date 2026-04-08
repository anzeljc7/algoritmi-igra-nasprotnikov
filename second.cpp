#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <queue>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <intrin.h>
#else
#include <sys/stat.h>
#endif

using namespace std;

// ---------------------------------------------------------
// NIZKONIVOJSKE BITNE FUNKCIJE
// ---------------------------------------------------------
#ifdef _MSC_VER
inline int popcount64(uint64_t x) {
    return static_cast<int>(__popcnt64(x));
}
inline int ctz64(uint64_t x) {
    unsigned long idx;
    if (_BitScanForward64(&idx, x)) {
        return static_cast<int>(idx);
    }
    return 64;
}
#else
inline int popcount64(uint64_t x) {
    return __builtin_popcountll(x);
}
inline int ctz64(uint64_t x) {
    return (x == 0) ? 64 : __builtin_ctzll(x);
}
#endif

// ---------------------------------------------------------
// PREPROST DINAMIČNI BITSET
// ---------------------------------------------------------
struct DynamicBitset {
    int nbits = 0;
    vector<uint64_t> words;

    DynamicBitset() = default;
    explicit DynamicBitset(int n, bool fill = false) : nbits(n), words((n + 63) / 64, fill ? ~0ULL : 0ULL) {
        trimLastWord();
    }

    void trimLastWord() {
        if (words.empty() || nbits % 64 == 0) {
            return;
        }
        words.back() &= ((1ULL << (nbits % 64)) - 1ULL);
    }

    void clearAll() {
        std::fill(words.begin(), words.end(), 0ULL);
    }

    void setAll() {
        std::fill(words.begin(), words.end(), ~0ULL);
        trimLastWord();
    }

    void set(int i) {
        words[i >> 6] |= (1ULL << (i & 63));
    }

    void reset(int i) {
        words[i >> 6] &= ~(1ULL << (i & 63));
    }

    bool test(int i) const {
        return (words[i >> 6] >> (i & 63)) & 1ULL;
    }

    bool any() const {
        for (uint64_t w : words) {
            if (w != 0ULL) {
                return true;
            }
        }
        return false;
    }

    int count() const {
        int total = 0;
        for (uint64_t w : words) {
            total += popcount64(w);
        }
        return total;
    }

    int first() const {
        for (int i = 0; i < static_cast<int>(words.size()); ++i) {
            if (words[i] != 0ULL) {
                return (i << 6) + ctz64(words[i]);
            }
        }
        return -1;
    }

    void andEq(const DynamicBitset& other) {
        for (size_t i = 0; i < words.size(); ++i) {
            words[i] &= other.words[i];
        }
    }

    void orEq(const DynamicBitset& other) {
        for (size_t i = 0; i < words.size(); ++i) {
            words[i] |= other.words[i];
        }
    }

    void andNotEq(const DynamicBitset& other) {
        for (size_t i = 0; i < words.size(); ++i) {
            words[i] &= ~other.words[i];
        }
        trimLastWord();
    }

    vector<int> toVector() const {
        vector<int> result;
        result.reserve(count());
        forEachSetBit([&](int bit) {
            result.push_back(bit);
        });
        return result;
    }

    template <typename Func>
    void forEachSetBit(Func&& f) const {
        for (int block = 0; block < static_cast<int>(words.size()); ++block) {
            uint64_t w = words[block];
            while (w != 0ULL) {
                int bit = ctz64(w);
                f((block << 6) + bit);
                w &= (w - 1ULL);
            }
        }
    }
};

struct Edge {
    int u = 0;
    int v = 0;
};

// ---------------------------------------------------------
// BRANJE / PISANJE
// ---------------------------------------------------------
vector<Edge> readGraph(const string& filename, int& n) {
    ifstream infile(filename);
    if (!infile.is_open()) {
        cerr << "Error: Could not open file '" << filename << "'!" << endl;
        n = 0;
        return {};
    }

    if (!(infile >> n)) {
        cerr << "Error: Failed to read number of nodes in '" << filename << "'." << endl;
        n = 0;
        return {};
    }

    vector<Edge> edges;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            int x;
            if (!(infile >> x)) {
                cerr << "Error: Invalid adjacency matrix in '" << filename << "'." << endl;
                n = 0;
                return {};
            }
            if (i < j && x == 1) {
                edges.push_back({i, j});
            }
        }
    }

    cout << "Read " << n << " nodes and found " << edges.size() << " edges." << endl;
    return edges;
}

void writeResults(const string& filename, const vector<Edge>& solution) {
    ofstream outfile(filename);
    if (!outfile.is_open()) {
        cerr << "Error: Could not create file '" << filename << "'." << endl;
        return;
    }

    outfile << solution.size() << "\n";
    for (const auto& e : solution) {
        // V izhodu morajo biti vozlišča oštevilčena od 1 naprej.
        outfile << (e.u + 1) << ' ' << (e.v + 1) << "\n";
    }

    cout << "Solution successfully saved to '" << filename << "'." << endl;
}

// ---------------------------------------------------------
// GRADNJA KONFLIKTNEGA GRAFA
// ---------------------------------------------------------
struct ConflictGraphData {
    vector<DynamicBitset> conflict;   // conflict[i] = vsi robovi, ki so v konfliktu z robom i
    vector<int> conflictDegree;       // stopnja v konfliktnem grafu
};

ConflictGraphData buildConflictGraph(int n, const vector<Edge>& edges) {
    const int E = static_cast<int>(edges.size());

    // closedNbr[v] = {v} U N(v)
    vector<DynamicBitset> closedNbr(n, DynamicBitset(n));
    for (int v = 0; v < n; ++v) {
        closedNbr[v].set(v);
    }

    for (const auto& e : edges) {
        closedNbr[e.u].set(e.v);
        closedNbr[e.v].set(e.u);
    }

    // incidentEdges[v] = vsi robovi, ki se stikajo z vozliščem v.
    vector<DynamicBitset> incidentEdges(n, DynamicBitset(E));
    for (int i = 0; i < E; ++i) {
        incidentEdges[edges[i].u].set(i);
        incidentEdges[edges[i].v].set(i);
    }

    vector<DynamicBitset> conflict(E, DynamicBitset(E));
    vector<int> conflictDegree(E, 0);

    // Za rob e=(u,v) so konfliktni vsi robovi, ki imajo vsaj eno krajišče v množici:
    // {u, v} U N(u) U N(v)
    // To izkoristimo tako, da za ta prepovedana vozlišča OR-amo njihove incidentne robove.
    for (int i = 0; i < E; ++i) {
        const Edge& e = edges[i];

        DynamicBitset forbiddenVertices = closedNbr[e.u];
        forbiddenVertices.orEq(closedNbr[e.v]);

        forbiddenVertices.forEachSetBit([&](int x) {
            conflict[i].orEq(incidentEdges[x]);
        });

        // Rob ni konflikt sam s sabo.
        conflict[i].reset(i);
        conflictDegree[i] = conflict[i].count();
    }

    return {std::move(conflict), std::move(conflictDegree)};
}

vector<vector<int>> buildConflictComponents(const vector<DynamicBitset>& conflict) {
    const int E = static_cast<int>(conflict.size());
    vector<char> seen(E, 0);
    vector<vector<int>> components;

    for (int start = 0; start < E; ++start) {
        if (seen[start]) {
            continue;
        }

        vector<int> component;
        queue<int> q;
        q.push(start);
        seen[start] = 1;

        while (!q.empty()) {
            int v = q.front();
            q.pop();
            component.push_back(v);

            conflict[v].forEachSetBit([&](int u) {
                if (!seen[u]) {
                    seen[u] = 1;
                    q.push(u);
                }
            });
        }

        components.push_back(std::move(component));
    }

    return components;
}

// ---------------------------------------------------------
// HEVRISTIKA + EKSAKTNI MAX-CLIQUE NA KOMPATIBILNEM GRAFU
// ---------------------------------------------------------
class MaxCliqueSolver {
public:
    explicit MaxCliqueSolver(vector<DynamicBitset> compatibility)
        : compat(std::move(compatibility)), n(static_cast<int>(compat.size())), degree(n, 0) {
        for (int i = 0; i < n; ++i) {
            degree[i] = compat[i].count();
        }
    }

    vector<int> solve() {
        if (n == 0) {
            return {};
        }

        buildInitialSolution();

        vector<int> root(n);
        iota(root.begin(), root.end(), 0);
        sort(root.begin(), root.end(), [&](int a, int b) {
            if (degree[a] != degree[b]) {
                return degree[a] > degree[b];
            }
            return a < b;
        });

        expand(root);
        return best;
    }

private:
    const vector<DynamicBitset> compat;
    const int n;
    vector<int> degree;
    vector<int> current;
    vector<int> best;

    vector<int> buildGreedyClique(const vector<int>& order, int forcedStart = -1) const {
        DynamicBitset allowed(n, true);
        vector<int> clique;

        if (forcedStart != -1) {
            clique.push_back(forcedStart);
            allowed.andEq(compat[forcedStart]);
            allowed.reset(forcedStart);
        }

        for (int v : order) {
            if (v == forcedStart) {
                continue;
            }
            if (allowed.test(v)) {
                clique.push_back(v);
                allowed.andEq(compat[v]);
                allowed.reset(v);
            }
        }

        return clique;
    }

    void greedyAugment(vector<int>& clique, const vector<int>& order) const {
        DynamicBitset allowed(n, true);
        for (int v : clique) {
            allowed.andEq(compat[v]);
        }
        for (int v : clique) {
            allowed.reset(v);
        }

        for (int v : order) {
            if (allowed.test(v)) {
                clique.push_back(v);
                allowed.andEq(compat[v]);
                allowed.reset(v);
            }
        }
    }

    bool improveByOneTwoSwap(vector<int>& clique, const vector<int>& order) const {
        if (clique.empty()) {
            return false;
        }

        DynamicBitset inClique(n);
        for (int v : clique) {
            inClique.set(v);
        }

        for (int removePos = 0; removePos < static_cast<int>(clique.size()); ++removePos) {
            DynamicBitset allowed(n, true);
            for (int i = 0; i < static_cast<int>(clique.size()); ++i) {
                if (i == removePos) {
                    continue;
                }
                allowed.andEq(compat[clique[i]]);
            }

            // Ne želimo ponovno uporabiti že izbranih vozlišč iz trenutne klike.
            allowed.andNotEq(inClique);

            vector<int> candidates;
            candidates.reserve(allowed.count());
            for (int v : order) {
                if (allowed.test(v)) {
                    candidates.push_back(v);
                }
            }

            for (int x : candidates) {
                DynamicBitset pairCandidates = allowed;
                pairCandidates.andEq(compat[x]);
                pairCandidates.reset(x);
                int y = pairCandidates.first();
                if (y != -1) {
                    int removed = clique[removePos];
                    clique.erase(clique.begin() + removePos);
                    clique.push_back(x);
                    clique.push_back(y);
                    (void)removed;
                    return true;
                }
            }
        }

        return false;
    }

    void buildInitialSolution() {
        vector<int> descOrder(n);
        iota(descOrder.begin(), descOrder.end(), 0);
        sort(descOrder.begin(), descOrder.end(), [&](int a, int b) {
            if (degree[a] != degree[b]) {
                return degree[a] > degree[b];
            }
            return a < b;
        });

        vector<int> ascOrder = descOrder;
        reverse(ascOrder.begin(), ascOrder.end());

        auto tryCandidate = [&](vector<int> cand, const vector<int>& augmentOrder) {
            greedyAugment(cand, augmentOrder);
            bool changed = true;
            while (changed) {
                changed = improveByOneTwoSwap(cand, augmentOrder);
                if (changed) {
                    greedyAugment(cand, augmentOrder);
                }
            }
            if (cand.size() > best.size()) {
                best = std::move(cand);
            }
        };

        tryCandidate(buildGreedyClique(descOrder), descOrder);
        tryCandidate(buildGreedyClique(ascOrder), descOrder);

        const int SEED_TRIES = min(n, 24);
        for (int i = 0; i < SEED_TRIES; ++i) {
            tryCandidate(buildGreedyClique(descOrder, descOrder[i]), descOrder);
        }
        for (int i = 0; i < SEED_TRIES; ++i) {
            tryCandidate(buildGreedyClique(ascOrder, ascOrder[i]), descOrder);
        }
    }

    // Greedy barvanje kompatibilnega grafa.
    // Ker iščemo kliko, nam število uporabljenih barv poda zgornjo mejo.
    void colorSort(const vector<int>& candidates, vector<int>& ordered, vector<int>& colorBound) const {
        vector<int> uncolored = candidates;
        sort(uncolored.begin(), uncolored.end(), [&](int a, int b) {
            if (degree[a] != degree[b]) {
                return degree[a] > degree[b];
            }
            return a < b;
        });

        ordered.clear();
        colorBound.clear();
        ordered.reserve(candidates.size());
        colorBound.reserve(candidates.size());

        int color = 0;
        while (!uncolored.empty()) {
            ++color;
            vector<int> rest;
            DynamicBitset blocked(n);

            for (int v : uncolored) {
                if (!blocked.test(v)) {
                    ordered.push_back(v);
                    colorBound.push_back(color);
                    blocked.orEq(compat[v]);
                } else {
                    rest.push_back(v);
                }
            }

            uncolored.swap(rest);
        }
    }

    void expand(const vector<int>& candidates) {
        if (candidates.empty()) {
            if (current.size() > best.size()) {
                best = current;
            }
            return;
        }

        // Šibkejša, a zelo poceni meja, ki včasih odreže vejo še pred barvanjem.
        if (current.size() + candidates.size() <= best.size()) {
            return;
        }

        vector<int> ordered;
        vector<int> colorBound;
        colorSort(candidates, ordered, colorBound);

        for (int i = static_cast<int>(ordered.size()) - 1; i >= 0; --i) {
            if (current.size() + colorBound[i] <= best.size()) {
                return;
            }

            int v = ordered[i];
            current.push_back(v);

            vector<int> nextCandidates;
            nextCandidates.reserve(i);
            for (int j = 0; j < i; ++j) {
                int u = ordered[j];
                if (compat[v].test(u)) {
                    nextCandidates.push_back(u);
                }
            }

            if (nextCandidates.empty()) {
                if (current.size() > best.size()) {
                    best = current;
                }
            } else {
                expand(nextCandidates);
            }

            current.pop_back();
        }
    }
};

vector<DynamicBitset> buildCompatibilitySubgraph(const vector<int>& component,
                                                 const vector<DynamicBitset>& conflict) {
    const int m = static_cast<int>(component.size());
    vector<DynamicBitset> compat(m, DynamicBitset(m));

    for (int i = 0; i < m; ++i) {
        for (int j = i + 1; j < m; ++j) {
            if (!conflict[component[i]].test(component[j])) {
                compat[i].set(j);
                compat[j].set(i);
            }
        }
    }

    return compat;
}

// ---------------------------------------------------------
// GLAVNI ALGORITEM
// ---------------------------------------------------------
vector<Edge> solveOrthogonalCCSet(int n, const vector<Edge>& edges) {
    if (edges.empty()) {
        return {};
    }

    ConflictGraphData cg = buildConflictGraph(n, edges);
    vector<vector<int>> components = buildConflictComponents(cg.conflict);

    // Večje komponente rešujemo najprej, ker so najtežje in tam najbolj koristi dober incumbent.
    sort(components.begin(), components.end(), [](const vector<int>& a, const vector<int>& b) {
        return a.size() > b.size();
    });

    vector<Edge> answer;
    answer.reserve(edges.size());

    for (const auto& component : components) {
        if (component.size() == 1) {
            answer.push_back(edges[component[0]]);
            continue;
        }

        vector<DynamicBitset> compat = buildCompatibilitySubgraph(component, cg.conflict);
        MaxCliqueSolver solver(std::move(compat));
        vector<int> bestLocal = solver.solve();

        for (int localIdx : bestLocal) {
            answer.push_back(edges[component[localIdx]]);
        }
    }

    cout << "Najdena je ortogonalna CC-mnozica velikosti: " << answer.size() << endl;
    return answer;
}

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
int main(int argc, char* argv[]) {
    string inputDir = "input";
    string outputDir = "output";
    string targetFileName = "primer.txt";

    // Omogoči preprosto uporabo iz ukazne vrstice:
    // ./cc_mnozica 05.txt
    if (argc >= 2) {
        targetFileName = argv[1];
    }

#ifdef _WIN32
    _mkdir(outputDir.c_str());
#else
    mkdir(outputDir.c_str(), 0777);
#endif

    string inputFile = inputDir + "/" + targetFileName;

    size_t dotPos = targetFileName.find_last_of('.');
    string stem = (dotPos == string::npos) ? targetFileName : targetFileName.substr(0, dotPos);
    string ext = (dotPos == string::npos) ? "" : targetFileName.substr(dotPos);
    string outputFile = outputDir + "/" + stem + "-out" + ext;

    cout << "\n---------------------------------------------------------" << endl;
    cout << "Processing: " << inputFile << endl;

    int n = 0;
    vector<Edge> edges = readGraph(inputFile, n);

    if (n != 0) {
        // Merimo samo čas glavnega algoritma, brez branja vhodne datoteke
        // in brez pisanja rezultata v izhodno datoteko.
        auto start = chrono::high_resolution_clock::now();
        vector<Edge> solution = solveOrthogonalCCSet(n, edges);
        auto end = chrono::high_resolution_clock::now();

        chrono::duration<double, std::milli> elapsedMs = end - start;
        writeResults(outputFile, solution);

        cout << "Algorithm execution time: " << elapsedMs.count() << " ms" << endl;
    } else {
        cerr << "Skipping algorithm due to read error. (Make sure input file exists.)" << endl;
    }

    cout << "Finished processing." << endl;
    return 0;
}
