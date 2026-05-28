#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iostream>
#include <new>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

using Matrix = vector<vector<unsigned char>>;

// n=20000 => ~400MB za matriko (unsigned char) -- fizicna zgornja meja,
// ne arbitrarna; naloga ne specificira limite, ampak vecje vrednosti
// zahtevajo vec pomnilnika kot je tipicno na voljo.
static constexpr int MAX_VERTEX_COUNT = 20000;

// Casovna meja za celoten solve_graph klic.
// B&B jo sposta in vrne najboljso kliko, ki jo je do tedaj nasel.
// Greedy fallback jo sposta med restartom zanko.
static constexpr double SOLVE_TIME_LIMIT_SECONDS = 8.0;

// Stevilo pohlepnih ponovitev z razlicnimi ureditvami robov.
// Vec ponovitev => boljsa resitev na redkih grafih, a vec casa.
static constexpr int GREEDY_RESTART_COUNT = 10;

// Pragovi za preklop na pohlepni fallback namesto eksaktnega B&B.
// Redki grafi z veliko robovi imajo gost kompatibilnostni graf =>
// B&B je pocasen, pohlepni pa hiter.
static constexpr int MAX_EXACT_EDGE_COUNT_FOR_SPARSE_COMPONENT = 1200;
static constexpr int MAX_EXACT_EDGE_COUNT_OVERALL = 15000;
static constexpr double SPARSE_COMPONENT_DENSITY_LIMIT = 0.25;

// Globalna casovna meja; nastavimo jo pred solve_graph in jo beremo
// v B&B in greedy zanki.
static chrono::steady_clock::time_point g_solve_deadline;

// ------------------------------------------------------------
// Problem:
// Iz grafa zelimo izbrati najvecjo mnozico robov tako, da:
// 1) se izbrani robovi med seboj ne dotikajo,
// 2) med krajisci razlicnih izbranih robov ni nobene povezave.
//
// Takšna mnozica robov je najvecja ortogonalna CC-mnozica
// (maksimalni inducirani matching).
//
// Eksaktna resitev (za manjse/gostejse komponente):
//   Vsak rob originalnega grafa = vozlisce v pomocnem grafu.
//   Dve vozlisci sta povezani ce sta robova kompatibilna.
//   Maksimalna klika v pomocnem grafu = iskana resitev.
//
// Pohlepni fallback (za vecje redke komponente):
//   Iterativno izbiramo rob z najmanjso "skodo" (stevilo odstranjenih
//   vozlisc). Vec ponovitev z razlicnimi ureditvami robov izboljsa kakovost.
// ------------------------------------------------------------

// Dinamicna bitna mnozica za hitro preverjanje sosednosti.
struct DynamicBitset {
    vector<unsigned long long> words;

    DynamicBitset() = default;

    explicit DynamicBitset(int bitCount)
        : words((bitCount + 63) / 64, 0ULL) {}

    void reset(int bitCount) {
        words.assign((bitCount + 63) / 64, 0ULL);
    }

    void set(int position) {
        words[position >> 6] |= (1ULL << (position & 63));
    }

    bool test(int position) const {
        return ((words[position >> 6] >> (position & 63)) & 1ULL) != 0ULL;
    }
};

// Rob originalnega grafa.
struct Edge {
    int from;
    int to;
};

static string trim_copy(const string& value) {
    size_t first = 0;
    while (first < value.size() && isspace((unsigned char)value[first])) {
        ++first;
    }

    size_t last = value.size();
    while (last > first && isspace((unsigned char)value[last - 1])) {
        --last;
    }

    return value.substr(first, last - first);
}

// Prebere eno vrstico matrike sosednosti.
// Podprta sta oba formata: "0 1 0 1" in "0101".
// Strog parser: dovoljeni sta samo vrednosti 0 in 1.
static vector<unsigned char> parse_adjacency_row(const string& line, int vertexCount) {
    vector<unsigned char> row;
    row.reserve(vertexCount);

    bool containsSeparators = false;
    for (char ch : line) {
        if (isspace((unsigned char)ch)) {
            containsSeparators = true;
            break;
        }
    }

    if (containsSeparators) {
        stringstream ss(line);
        string token;
        while (ss >> token) {
            if (token != "0" && token != "1") {
                throw runtime_error("Matrika sosednosti lahko vsebuje samo vrednosti 0 ali 1.");
            }
            row.push_back((unsigned char)(token[0] - '0'));
        }

        if ((int)row.size() != vertexCount) {
            throw runtime_error("Napacno stevilo vrednosti v vrstici matrike sosednosti.");
        }

        return row;
    }

    string compact = trim_copy(line);
    if ((int)compact.size() != vertexCount) {
        throw runtime_error("Napacno stevilo vrednosti v vrstici matrike sosednosti.");
    }

    for (char ch : compact) {
        if (ch != '0' && ch != '1') {
            throw runtime_error("Matrika sosednosti lahko vsebuje samo vrednosti 0 ali 1.");
        }
        row.push_back((unsigned char)(ch - '0'));
    }

    return row;
}

// Prebere graf iz podanega vhodnega toka.
static Matrix read_graph(istream& input) {
    long long parsedVertexCount;
    if (!(input >> parsedVertexCount)) {
        throw runtime_error("Manjka stevilo vozlisc.");
    }

    if (parsedVertexCount < 0) {
        throw runtime_error("Stevilo vozlisc ne sme biti negativno.");
    }

    if (parsedVertexCount > MAX_VERTEX_COUNT) {
        throw runtime_error("Stevilo vozlisc je preveliko za varno obdelavo tega programa.");
    }

    int vertexCount = (int)parsedVertexCount;

    string line;
    getline(input, line);

    Matrix adjacencyMatrix(vertexCount, vector<unsigned char>(vertexCount, 0));

    for (int rowIndex = 0; rowIndex < vertexCount; ++rowIndex) {
        do {
            if (!getline(input, line)) {
                throw runtime_error("Premalo vrstic v vhodu.");
            }
        } while (trim_copy(line).empty());

        vector<unsigned char> row = parse_adjacency_row(line, vertexCount);
        for (int colIndex = 0; colIndex < vertexCount; ++colIndex) {
            adjacencyMatrix[rowIndex][colIndex] = row[colIndex];
        }
    }

    // Simetriziramo in nastavimo diagonalo na 0.
    for (int i = 0; i < vertexCount; ++i) {
        adjacencyMatrix[i][i] = 0;
        for (int j = i + 1; j < vertexCount; ++j) {
            unsigned char value = (adjacencyMatrix[i][j] || adjacencyMatrix[j][i]) ? 1 : 0;
            adjacencyMatrix[i][j] = value;
            adjacencyMatrix[j][i] = value;
        }
    }

    return adjacencyMatrix;
}

class MaximumCliqueSolver {
public:
    int vertexCount = 0;
    vector<DynamicBitset> adjacency;
    vector<Edge> representedEdges;
    vector<int> bestClique;

    vector<int> solve() {
        // bestClique se ne brise — morda je bil ze nastavljen z zunanjim
        // greedy sedom preden smo poklicali solve().
        time_exceeded = false;
        expand_calls = 0;

        compute_degrees();
        build_initial_greedy_clique();

        vector<int> candidateVertices(vertexCount);
        for (int i = 0; i < vertexCount; ++i) {
            candidateVertices[i] = i;
        }

        sort(candidateVertices.begin(), candidateVertices.end(), [&](int left, int right) {
            if (degree[left] != degree[right]) {
                return degree[left] > degree[right];
            }
            return left < right;
        });

        vector<int> currentClique;
        expand(currentClique, candidateVertices);

        sort(bestClique.begin(), bestClique.end(), EdgeIdComparator(*this));
        return bestClique;
    }

private:
    vector<int> degree;
    bool time_exceeded = false;
    int expand_calls = 0;

    void compute_degrees() {
        degree.assign(vertexCount, 0);
        for (int i = 0; i < vertexCount; ++i) {
            int d = 0;
            for (unsigned long long word : adjacency[i].words) {
                d += __builtin_popcountll(word);
            }
            degree[i] = d;
        }
    }

    // Zgradi zacetno greedy kliko kot spodnjo mejo za B&B,
    // kar omogoci zgodnejse obrezovanje vej.
    void build_initial_greedy_clique() {
        vector<int> candidates(vertexCount);
        for (int i = 0; i < vertexCount; ++i) {
            candidates[i] = i;
        }

        sort(candidates.begin(), candidates.end(), [&](int left, int right) {
            if (degree[left] != degree[right]) {
                return degree[left] > degree[right];
            }
            return left < right;
        });

        vector<int> clique;
        while (!candidates.empty()) {
            int chosen = candidates.front();
            clique.push_back(chosen);

            vector<int> next;
            next.reserve(candidates.size());
            for (size_t i = 1; i < candidates.size(); ++i) {
                if (adjacency[chosen].test(candidates[i])) {
                    next.push_back(candidates[i]);
                }
            }
            candidates.swap(next);
        }

        // Posodobimo le ce je boljse — bestClique je morda ze nastavljen
        // z zunanjim greedy sedom preden smo poklicali solve().
        if (clique.size() > bestClique.size()) {
            bestClique = clique;
        }
    }

    // Pohlepno barvanje: zgornja meja za velikost klike iz podmnozice kandidatov.
    void greedy_color_sort(const vector<int>& candidates,
                           vector<int>& orderedVertices,
                           vector<int>& colorBounds) const {
        orderedVertices.clear();
        colorBounds.clear();

        if (candidates.empty()) {
            return;
        }

        vector<int> remaining = candidates;
        sort(remaining.begin(), remaining.end(), [&](int left, int right) {
            if (degree[left] != degree[right]) {
                return degree[left] > degree[right];
            }
            return left < right;
        });

        orderedVertices.reserve(candidates.size());
        colorBounds.reserve(candidates.size());

        int currentColor = 0;

        while (!remaining.empty()) {
            ++currentColor;
            vector<int> colorClass;
            vector<int> nextRemaining;

            for (int v : remaining) {
                bool fits = true;
                for (int u : colorClass) {
                    if (adjacency[v].test(u)) {
                        fits = false;
                        break;
                    }
                }
                if (fits) {
                    colorClass.push_back(v);
                } else {
                    nextRemaining.push_back(v);
                }
            }

            for (int v : colorClass) {
                orderedVertices.push_back(v);
                colorBounds.push_back(currentColor);
            }

            remaining.swap(nextRemaining);
        }
    }

    // Rekurzivni branch-and-bound.
    // Preveri cas vsakih 4096 klicev, da ne upocasnimo z dragim syscallom.
    void expand(vector<int>& currentClique, const vector<int>& candidates) {
        if ((++expand_calls & 4095) == 0 && !time_exceeded) {
            time_exceeded = (chrono::steady_clock::now() >= g_solve_deadline);
        }
        if (time_exceeded) {
            return;
        }

        if (candidates.empty()) {
            if (currentClique.size() > bestClique.size()) {
                bestClique = currentClique;
            }
            return;
        }

        vector<int> orderedVertices;
        vector<int> colorBounds;
        greedy_color_sort(candidates, orderedVertices, colorBounds);

        for (int i = (int)orderedVertices.size() - 1; i >= 0; --i) {
            // <= ker izenacenje ne prinese strogo boljse resitve
            if (currentClique.size() + (size_t)colorBounds[i] <= bestClique.size()) {
                return;
            }

            int chosen = orderedVertices[i];
            currentClique.push_back(chosen);

            vector<int> nextCandidates;
            nextCandidates.reserve((size_t)i);
            for (int j = 0; j < i; ++j) {
                if (adjacency[chosen].test(orderedVertices[j])) {
                    nextCandidates.push_back(orderedVertices[j]);
                }
            }

            expand(currentClique, nextCandidates);
            currentClique.pop_back();
        }
    }

    struct EdgeIdComparator {
        const MaximumCliqueSolver& solver;
        explicit EdgeIdComparator(const MaximumCliqueSolver& s) : solver(s) {}
        bool operator()(int a, int b) const {
            const Edge& ea = solver.representedEdges[a];
            const Edge& eb = solver.representedEdges[b];
            if (ea.from != eb.from) return ea.from < eb.from;
            return ea.to < eb.to;
        }
    };
};

static vector<vector<int>> find_connected_components(const Matrix& adjacencyMatrix) {
    int n = (int)adjacencyMatrix.size();
    vector<int> visited(n, 0);
    vector<vector<int>> components;

    for (int start = 0; start < n; ++start) {
        if (visited[start]) continue;

        vector<int> comp;
        queue<int> q;
        q.push(start);
        visited[start] = 1;

        while (!q.empty()) {
            int u = q.front();
            q.pop();
            comp.push_back(u);
            for (int v = 0; v < n; ++v) {
                if (adjacencyMatrix[u][v] && !visited[v]) {
                    visited[v] = 1;
                    q.push(v);
                }
            }
        }

        components.push_back(comp);
    }

    return components;
}

static bool are_compatible(const Edge& e1, const Edge& e2, const Matrix& adj) {
    if (e1.from == e2.from || e1.from == e2.to ||
        e1.to == e2.from || e1.to == e2.to) {
        return false;
    }
    return !(adj[e1.from][e2.from] || adj[e1.from][e2.to] ||
             adj[e1.to][e2.from] || adj[e1.to][e2.to]);
}

static double compute_component_density(int vertexCount, int edgeCount) {
    if (vertexCount <= 1) return 0.0;
    double possible = (double)vertexCount * (double)(vertexCount - 1) / 2.0;
    return edgeCount / possible;
}

static bool should_use_greedy_fallback(int componentVertexCount, int edgeCount) {
    if (edgeCount > MAX_EXACT_EDGE_COUNT_OVERALL) {
        return true;
    }

    double density = compute_component_density(componentVertexCount, edgeCount);

    if (density <= SPARSE_COMPONENT_DENSITY_LIMIT &&
        edgeCount > MAX_EXACT_EDGE_COUNT_FOR_SPARSE_COMPONENT) {
        return true;
    }

    // Pri zelo redkih originalnih grafih je kompatibilnostni graf zelo gost,
    // ker se vecina parov robov ne dotika. Ocena gostote kompatibilnostnega
    // grafa: verjetnost da sta 2 robova kompatibilna ~ (1-p)^4.
    // Ce je ta gostota visoka, B&B ne more ucinkovito obrezovati vej.
    double q = 1.0 - density;
    double estCompatDensity = q * q * q * q;
    if (estCompatDensity > 0.85 && edgeCount > 300) {
        return true;
    }

    return false;
}

// En pohlepni zagon z dano ureditvijo robov.
// Izbira rob z najmanjso skodo (stevilo odstranjenih vozlisc).
// Stopnje se posodabljajo po vsaki odstranitvi za natancnejsi tie-breaking.
static vector<pair<int, int>> run_single_greedy(
    const Matrix& adjacencyMatrix,
    const vector<int>& sortedVertices,
    const vector<Edge>& edgeOrder
) {
    int n = (int)adjacencyMatrix.size();
    vector<unsigned char> available(n, 0);
    vector<int> degree(n, 0);

    for (int v : sortedVertices) {
        available[v] = 1;
    }

    for (int v : sortedVertices) {
        int d = 0;
        for (int u : sortedVertices) {
            if (adjacencyMatrix[v][u]) ++d;
        }
        degree[v] = d;
    }

    vector<pair<int, int>> result;

    while (true) {
        int bestU = -1, bestV = -1;
        int bestRemoved = n + 1, bestDegSum = n * 2 + 1;

        for (const Edge& e : edgeOrder) {
            int u = e.from, v = e.to;
            if (!available[u] || !available[v]) continue;

            int removed = 0;
            for (int x : sortedVertices) {
                if (!available[x]) continue;
                if (x == u || x == v || adjacencyMatrix[u][x] || adjacencyMatrix[v][x]) {
                    ++removed;
                }
            }

            int degSum = degree[u] + degree[v];
            bool better = false;

            if (removed < bestRemoved) {
                better = true;
            } else if (removed == bestRemoved && degSum < bestDegSum) {
                better = true;
            } else if (removed == bestRemoved && degSum == bestDegSum) {
                if (bestU == -1 || make_pair(u, v) < make_pair(bestU, bestV)) {
                    better = true;
                }
            }

            if (better) {
                bestRemoved = removed;
                bestDegSum = degSum;
                bestU = u;
                bestV = v;
            }
        }

        if (bestU == -1) break;

        result.push_back({bestU + 1, bestV + 1});

        // Najprej dolocimo mnozico za odstranitev.
        vector<int> toRemove;
        for (int x : sortedVertices) {
            if (!available[x]) continue;
            if (x == bestU || x == bestV ||
                adjacencyMatrix[bestU][x] || adjacencyMatrix[bestV][x]) {
                toRemove.push_back(x);
            }
        }

        // Posodobimo stopnje preostalih vozlisc preden jih oznacimo za nedosegljive.
        for (int x : toRemove) {
            for (int y : sortedVertices) {
                if (available[y] && adjacencyMatrix[x][y]) {
                    degree[y]--;
                }
            }
            available[x] = 0;
        }
    }

    sort(result.begin(), result.end());
    return result;
}

// Pohlepni fallback za vecje redke komponente.
// Izvede vec ponovitev z razlicnimi ureditvami robov in vrne najboljso.
static vector<pair<int, int>> solve_component_greedy_induced_matching(
    const Matrix& adjacencyMatrix,
    const vector<int>& sortedVertices,
    const vector<Edge>& componentEdges
) {
    vector<pair<int, int>> best = run_single_greedy(adjacencyMatrix, sortedVertices, componentEdges);

    mt19937 rng(42);
    vector<Edge> shuffled = componentEdges;

    for (int r = 1; r < GREEDY_RESTART_COUNT; ++r) {
        if (chrono::steady_clock::now() >= g_solve_deadline) break;

        shuffle(shuffled.begin(), shuffled.end(), rng);
        auto result = run_single_greedy(adjacencyMatrix, sortedVertices, shuffled);

        if (result.size() > best.size()) {
            best = result;
        }
    }

    sort(best.begin(), best.end());
    return best;
}

static vector<pair<int, int>> solve_component(const Matrix& adjacencyMatrix,
                                              const vector<int>& componentVertices) {
    vector<int> sortedVertices = componentVertices;
    sort(sortedVertices.begin(), sortedVertices.end());

    vector<Edge> componentEdges;
    for (size_t i = 0; i < sortedVertices.size(); ++i) {
        for (size_t j = i + 1; j < sortedVertices.size(); ++j) {
            if (adjacencyMatrix[sortedVertices[i]][sortedVertices[j]]) {
                componentEdges.push_back({sortedVertices[i], sortedVertices[j]});
            }
        }
    }

    sort(componentEdges.begin(), componentEdges.end(), [](const Edge& a, const Edge& b) {
        if (a.from != b.from) return a.from < b.from;
        return a.to < b.to;
    });

    int edgeCount = (int)componentEdges.size();
    if (edgeCount == 0) return {};

    if (should_use_greedy_fallback((int)sortedVertices.size(), edgeCount)) {
        return solve_component_greedy_induced_matching(adjacencyMatrix, sortedVertices, componentEdges);
    }

    MaximumCliqueSolver solver;
    solver.vertexCount = edgeCount;
    solver.representedEdges = componentEdges;
    solver.adjacency.assign(edgeCount, DynamicBitset(edgeCount));

    for (int i = 0; i < edgeCount; ++i) {
        for (int j = i + 1; j < edgeCount; ++j) {
            if (are_compatible(componentEdges[i], componentEdges[j], adjacencyMatrix)) {
                solver.adjacency[i].set(j);
                solver.adjacency[j].set(i);
            }
        }
    }

    vector<int> bestEdgeIds = solver.solve();

    vector<pair<int, int>> result;
    result.reserve(bestEdgeIds.size());
    for (int id : bestEdgeIds) {
        const Edge& e = componentEdges[id];
        result.push_back({e.from + 1, e.to + 1});
    }

    sort(result.begin(), result.end());
    return result;
}

static vector<pair<int, int>> solve_graph(const Matrix& adjacencyMatrix) {
    g_solve_deadline = chrono::steady_clock::now() +
                       chrono::milliseconds((long long)(SOLVE_TIME_LIMIT_SECONDS * 1000.0));

    vector<vector<int>> components = find_connected_components(adjacencyMatrix);
    vector<pair<int, int>> result;

    for (const vector<int>& comp : components) {
        vector<pair<int, int>> partial = solve_component(adjacencyMatrix, comp);
        result.insert(result.end(), partial.begin(), partial.end());
    }

    sort(result.begin(), result.end());
    return result;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    try {
        Matrix adjacencyMatrix;

        if (argc >= 2) {
            ifstream inputFile(argv[1]);
            if (!inputFile) {
                cerr << "Napaka: ne morem odpreti vhodne datoteke.\n";
                return 1;
            }
            adjacencyMatrix = read_graph(inputFile);
        } else {
            adjacencyMatrix = read_graph(cin);
        }

        vector<pair<int, int>> solution = solve_graph(adjacencyMatrix);

        if (argc >= 3) {
            ofstream outputFile(argv[2]);
            if (!outputFile) {
                cerr << "Napaka: ne morem odpreti izhodne datoteke.\n";
                return 1;
            }
            outputFile << solution.size() << '\n';
            for (const auto& e : solution) {
                outputFile << e.first << ' ' << e.second << '\n';
            }
        } else {
            cout << solution.size() << '\n';
            for (const auto& e : solution) {
                cout << e.first << ' ' << e.second << '\n';
            }
        }

    } catch (const bad_alloc&) {
        cerr << "Napaka: vhod je prevelik za razpolozljiv pomnilnik.\n";
        return 1;
    } catch (const exception& e) {
        cerr << "Napaka: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
