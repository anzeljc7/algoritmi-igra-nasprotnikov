#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iostream>
#include <new>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

using Matrix = vector<vector<unsigned char>>;

// Zgornja meja je namenoma preverjena pred alokacijo matrike,
// da neveljavni ali nerealno veliki vhodi ne povzrocijo std::bad_alloc.
static constexpr int MAX_VERTEX_COUNT = 5000;

// Pri vecjih redkih komponentah je pomocni graf kompatibilnosti zelo gost,
// zato exact max-clique pristop lahko postane eksponentno pocasen.
// Takrat uporabimo hiter direktni greedy fallback nad originalnim grafom.
static constexpr int MAX_EXACT_EDGE_COUNT_FOR_SPARSE_COMPONENT = 1200;
static constexpr int MAX_EXACT_EDGE_COUNT_OVERALL = 15000;
static constexpr double SPARSE_COMPONENT_DENSITY_LIMIT = 0.25;

// ------------------------------------------------------------
// Problem:
// Iz grafa zelimo izbrati najvecjo mnozico robov tako, da:
// 1) se izbrani robovi med seboj ne dotikajo,
// 2) med krajisci razlicnih izbranih robov ni nobene povezave.
//
// Takšna mnozica robov je najvecja ortogonalna CC-mnozica.
//
// Ideja exact resitve:
// - vsak rob originalnega grafa obravnavamo kot vozlisce v pomocnem grafu,
// - dve taki vozlisci povezemo, ce sta pripadajoca roba kompatibilna,
// - nato v tem pomocnem grafu poiscemo maksimalno kliko.
//
// Za velike redke komponente uporabimo greedy fallback, ker tam pomocni graf
// postane gost in branch-and-bound slabo reze veje.
// ------------------------------------------------------------

// Dinamicna bitna mnozica za hitro preverjanje sosednosti v pomocnem grafu.
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
// Podprta sta oba formata:
//   0 1 0 1
//   0101
// Parser je namenoma strog: dovoljeni sta samo vrednosti 0 in 1.
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
    getline(input, line); // porabimo preostanek prve vrstice

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

    // Graf obravnavamo kot neusmerjen:
    // - diagonala mora biti 0,
    // - matriko simetriziramo.
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
        bestClique.clear();
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

        // Za determinističen izpis uredimo izbrane robove leksikografsko.
        sort(bestClique.begin(), bestClique.end(), EdgeIdComparator(*this));
        return bestClique;
    }

private:
    vector<int> degree;

    void compute_degrees() {
        degree.assign(vertexCount, 0);
        for (int i = 0; i < vertexCount; ++i) {
            int currentDegree = 0;
            for (unsigned long long word : adjacency[i].words) {
                currentDegree += __builtin_popcountll(word);
            }
            degree[i] = currentDegree;
        }
    }

    // Hitro zgradimo zacetno kliko, da branch-and-bound ze na zacetku
    // dobi uporabno spodnjo mejo in lahko odreze vec vej.
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
            int chosenVertex = candidates.front();
            clique.push_back(chosenVertex);

            vector<int> nextCandidates;
            nextCandidates.reserve(candidates.size());

            for (size_t i = 1; i < candidates.size(); ++i) {
                int candidateVertex = candidates[i];
                if (adjacency[chosenVertex].test(candidateVertex)) {
                    nextCandidates.push_back(candidateVertex);
                }
            }

            candidates.swap(nextCandidates);
        }

        bestClique = clique;
    }

    // Pohlepno barvanje kandidatov.
    // Barve uporabimo kot zgornjo mejo za velikost klike,
    // kar omogoca ucinkovito obrezovanje v branch-and-bound postopku.
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
            vector<int> currentColorClass;
            vector<int> nextRemaining;

            for (int vertex : remaining) {
                bool canUseCurrentColor = true;

                for (int coloredVertex : currentColorClass) {
                    if (adjacency[vertex].test(coloredVertex)) {
                        canUseCurrentColor = false;
                        break;
                    }
                }

                if (canUseCurrentColor) {
                    currentColorClass.push_back(vertex);
                } else {
                    nextRemaining.push_back(vertex);
                }
            }

            for (int vertex : currentColorClass) {
                orderedVertices.push_back(vertex);
                colorBounds.push_back(currentColor);
            }

            remaining.swap(nextRemaining);
        }
    }

    // Rekurzivni branch-and-bound postopek za iskanje maksimalne klike.
    void expand(vector<int>& currentClique, const vector<int>& candidates) {
        if (candidates.empty()) {
            // Enako velika resitev za nalogo ni boljša, zato je ne raziskujemo
            // in ne zamenjujemo trenutne najboljše resitve.
            if (currentClique.size() > bestClique.size()) {
                bestClique = currentClique;
            }
            return;
        }

        vector<int> orderedVertices;
        vector<int> colorBounds;
        greedy_color_sort(candidates, orderedVertices, colorBounds);

        for (int i = (int)orderedVertices.size() - 1; i >= 0; --i) {
            // Ce niti v najboljšem primeru ne moremo izboljšati trenutne resitve,
            // celotno vejo odrezemo. Uporabimo <=, ker izenacenje ne prinese
            // boljše ocene pri tej nalogi.
            if (currentClique.size() + (size_t)colorBounds[i] <= bestClique.size()) {
                return;
            }

            int chosenVertex = orderedVertices[i];
            currentClique.push_back(chosenVertex);

            // Naslednji kandidati so le sosedi izbranega vozlisca,
            // saj mora mnozica ostati klika.
            vector<int> nextCandidates;
            nextCandidates.reserve((size_t)i);

            for (int j = 0; j < i; ++j) {
                int candidateVertex = orderedVertices[j];
                if (adjacency[chosenVertex].test(candidateVertex)) {
                    nextCandidates.push_back(candidateVertex);
                }
            }

            expand(currentClique, nextCandidates);
            currentClique.pop_back();
        }
    }

    struct EdgeIdComparator {
        const MaximumCliqueSolver& solver;

        explicit EdgeIdComparator(const MaximumCliqueSolver& solver) : solver(solver) {}

        bool operator()(int leftId, int rightId) const {
            const Edge& leftEdge = solver.representedEdges[leftId];
            const Edge& rightEdge = solver.representedEdges[rightId];

            if (leftEdge.from != rightEdge.from) {
                return leftEdge.from < rightEdge.from;
            }
            return leftEdge.to < rightEdge.to;
        }
    };
};

// Razbije graf na povezane komponente.
// To je dobra optimizacija, ker lahko vsako komponento resujemo loceno.
static vector<vector<int>> find_connected_components(const Matrix& adjacencyMatrix) {
    int vertexCount = (int)adjacencyMatrix.size();
    vector<int> visited(vertexCount, 0);
    vector<vector<int>> components;

    for (int startVertex = 0; startVertex < vertexCount; ++startVertex) {
        if (visited[startVertex]) {
            continue;
        }

        vector<int> componentVertices;
        queue<int> bfsQueue;
        bfsQueue.push(startVertex);
        visited[startVertex] = 1;

        while (!bfsQueue.empty()) {
            int currentVertex = bfsQueue.front();
            bfsQueue.pop();
            componentVertices.push_back(currentVertex);

            for (int nextVertex = 0; nextVertex < vertexCount; ++nextVertex) {
                if (adjacencyMatrix[currentVertex][nextVertex] && !visited[nextVertex]) {
                    visited[nextVertex] = 1;
                    bfsQueue.push(nextVertex);
                }
            }
        }

        components.push_back(componentVertices);
    }

    return components;
}

// Preveri, ali sta dva roba kompatibilna glede na definicijo problema.
static bool are_compatible(const Edge& firstEdge,
                           const Edge& secondEdge,
                           const Matrix& adjacencyMatrix) {
    // Robova ne smeta deliti krajisca.
    if (firstEdge.from == secondEdge.from ||
        firstEdge.from == secondEdge.to ||
        firstEdge.to == secondEdge.from ||
        firstEdge.to == secondEdge.to) {
        return false;
    }

    // Med krajisci razlicnih robov ne sme biti nobene povezave.
    if (adjacencyMatrix[firstEdge.from][secondEdge.from] ||
        adjacencyMatrix[firstEdge.from][secondEdge.to] ||
        adjacencyMatrix[firstEdge.to][secondEdge.from] ||
        adjacencyMatrix[firstEdge.to][secondEdge.to]) {
        return false;
    }

    return true;
}

static double compute_component_density(int vertexCount, int edgeCount) {
    if (vertexCount <= 1) {
        return 0.0;
    }

    double possibleEdges = (double)vertexCount * (double)(vertexCount - 1) / 2.0;
    return edgeCount / possibleEdges;
}

static bool should_use_greedy_fallback(int componentVertexCount, int edgeCount) {
    if (edgeCount > MAX_EXACT_EDGE_COUNT_OVERALL) {
        return true;
    }

    double density = compute_component_density(componentVertexCount, edgeCount);
    return density <= SPARSE_COMPONENT_DENSITY_LIMIT &&
           edgeCount > MAX_EXACT_EDGE_COUNT_FOR_SPARSE_COMPONENT;
}

// Hiter fallback za velike redke komponente.
// Rob izberemo tako, da odstrani cim manj razpolozljivih vozlisc
// iz zaprte soseske N[u] U N[v]. S tem poskusamo pustiti cim vec prostora
// za naslednje pare. Resitev je vedno veljavna, ni pa nujno optimalna.
static vector<pair<int, int>> solve_component_greedy_induced_matching(
    const Matrix& adjacencyMatrix,
    const vector<int>& sortedVertices,
    const vector<Edge>& componentEdges
) {
    int graphVertexCount = (int)adjacencyMatrix.size();
    vector<unsigned char> available(graphVertexCount, 0);
    vector<int> degree(graphVertexCount, 0);

    for (int vertex : sortedVertices) {
        available[vertex] = 1;
    }

    for (int vertex : sortedVertices) {
        int currentDegree = 0;
        for (int other : sortedVertices) {
            if (adjacencyMatrix[vertex][other]) {
                ++currentDegree;
            }
        }
        degree[vertex] = currentDegree;
    }

    vector<pair<int, int>> result;

    while (true) {
        int bestU = -1;
        int bestV = -1;
        int bestRemovedCount = graphVertexCount + 1;
        int bestDegreeSum = graphVertexCount * 2 + 1;

        for (const Edge& edge : componentEdges) {
            int u = edge.from;
            int v = edge.to;

            if (!available[u] || !available[v]) {
                continue;
            }

            int removedCount = 0;
            for (int x : sortedVertices) {
                if (!available[x]) {
                    continue;
                }

                if (x == u || x == v || adjacencyMatrix[u][x] || adjacencyMatrix[v][x]) {
                    ++removedCount;
                }
            }

            int degreeSum = degree[u] + degree[v];
            bool better = false;

            if (removedCount < bestRemovedCount) {
                better = true;
            } else if (removedCount == bestRemovedCount && degreeSum < bestDegreeSum) {
                better = true;
            } else if (removedCount == bestRemovedCount && degreeSum == bestDegreeSum) {
                if (bestU == -1 || make_pair(u, v) < make_pair(bestU, bestV)) {
                    better = true;
                }
            }

            if (better) {
                bestRemovedCount = removedCount;
                bestDegreeSum = degreeSum;
                bestU = u;
                bestV = v;
            }
        }

        if (bestU == -1) {
            break;
        }

        result.push_back({bestU + 1, bestV + 1});

        for (int x : sortedVertices) {
            if (!available[x]) {
                continue;
            }

            if (x == bestU || x == bestV ||
                adjacencyMatrix[bestU][x] || adjacencyMatrix[bestV][x]) {
                available[x] = 0;
            }
        }
    }

    sort(result.begin(), result.end());
    return result;
}

// Reši eno povezano komponento originalnega grafa.
static vector<pair<int, int>> solve_component(const Matrix& adjacencyMatrix,
                                              const vector<int>& componentVertices) {
    vector<int> sortedVertices = componentVertices;
    sort(sortedVertices.begin(), sortedVertices.end());

    // Zberemo vse robove znotraj komponente.
    vector<Edge> componentEdges;
    for (size_t i = 0; i < sortedVertices.size(); ++i) {
        for (size_t j = i + 1; j < sortedVertices.size(); ++j) {
            if (adjacencyMatrix[sortedVertices[i]][sortedVertices[j]]) {
                componentEdges.push_back({sortedVertices[i], sortedVertices[j]});
            }
        }
    }

    sort(componentEdges.begin(), componentEdges.end(), [](const Edge& left, const Edge& right) {
        if (left.from != right.from) {
            return left.from < right.from;
        }
        return left.to < right.to;
    });

    int edgeCount = (int)componentEdges.size();
    if (edgeCount == 0) {
        return {};
    }

    if (should_use_greedy_fallback((int)sortedVertices.size(), edgeCount)) {
        return solve_component_greedy_induced_matching(adjacencyMatrix, sortedVertices, componentEdges);
    }

    // Zgradimo pomocni graf kompatibilnosti nad robovi originalnega grafa.
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

    // Maksimalna klika v pomocnem grafu predstavlja iskano exact resitev.
    vector<int> bestEdgeIds = solver.solve();

    vector<pair<int, int>> result;
    result.reserve(bestEdgeIds.size());

    // V izhodu uporabljamo 1-based indeksiranje vozlisc.
    for (int edgeId : bestEdgeIds) {
        const Edge& edge = componentEdges[edgeId];
        result.push_back({edge.from + 1, edge.to + 1});
    }

    sort(result.begin(), result.end());
    return result;
}

// Resi celoten graf.
static vector<pair<int, int>> solve_graph(const Matrix& adjacencyMatrix) {
    vector<vector<int>> connectedComponents = find_connected_components(adjacencyMatrix);
    vector<pair<int, int>> result;

    for (const vector<int>& componentVertices : connectedComponents) {
        vector<pair<int, int>> partialResult = solve_component(adjacencyMatrix, componentVertices);
        result.insert(result.end(), partialResult.begin(), partialResult.end());
    }

    sort(result.begin(), result.end());
    return result;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    try {
        Matrix adjacencyMatrix;

        // argv[1] = vhodna datoteka, ce obstaja
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

        // Merimo samo cas glavnega algoritma.
        // auto startTime = chrono::steady_clock::now();
        vector<pair<int, int>> solution = solve_graph(adjacencyMatrix);
        // auto endTime = chrono::steady_clock::now();

        // double elapsedMilliseconds =
        //     chrono::duration_cast<chrono::duration<double, milli>>(endTime - startTime).count();

        // argv[2] = izhodna datoteka, ce obstaja
        if (argc >= 3) {
            ofstream outputFile(argv[2]);
            if (!outputFile) {
                cerr << "Napaka: ne morem odpreti izhodne datoteke.\n";
                return 1;
            }

            outputFile << solution.size() << '\n';
            for (const auto& edge : solution) {
                outputFile << edge.first << ' ' << edge.second << '\n';
            }
        } else {
            cout << solution.size() << '\n';
            for (const auto& edge : solution) {
                cout << edge.first << ' ' << edge.second << '\n';
            }
        }

        // cout << "Time: " << elapsedMilliseconds << " ms\n";
    } catch (const bad_alloc&) {
        cerr << "Napaka: vhod je prevelik za razpolozljiv pomnilnik.\n";
        return 1;
    } catch (const exception& e) {
        cerr << "Napaka: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
