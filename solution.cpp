#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

// ------------------------------------------------------------
// Problem:
// Iz grafa želimo izbrati največjo množico robov tako, da:
// 1) se izbrani robovi med seboj ne dotikajo,
// 2) med krajišči različnih izbranih robov ni nobene povezave.
//
// Takšna množica robov je največja ortogonalna CC-množica.
//
// Ideja rešitve:
// - vsak rob originalnega grafa obravnavamo kot vozlišče v pomožnem grafu,
// - dve taki vozlišči povežemo, če sta pripadajoča roba kompatibilna,
// - nato v tem pomožnem grafu poiščemo maksimalno kliko.
//
// Maksimalna klika v pomožnem grafu torej predstavlja največjo množico
// medsebojno kompatibilnih robov v originalnem grafu.
// ------------------------------------------------------------

// Dinamična bitna množica za hitro preverjanje sosednosti v pomožnem grafu.
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

// Prebere eno vrstico matrike sosednosti.
// Podprta sta oba formata:
//   0 1 0 1
//   0101
static vector<int> parse_adjacency_row(const string& line, int vertexCount) {
    vector<int> row;
    row.reserve(vertexCount);

    bool containsSeparators = false;
    for (char ch : line) {
        if (ch == ' ' || ch == '\t') {
            containsSeparators = true;
            break;
        }
    }

    // Najprej poskusimo format s presledki.
    if (containsSeparators) {
        stringstream ss(line);
        int value;
        while (ss >> value) {
            row.push_back(value ? 1 : 0);
        }
        if ((int)row.size() == vertexCount) {
            return row;
        }
        row.clear();
    }

    // Če to ni uspelo, poberemo vse znake 0 in 1 brez ločil.
    for (char ch : line) {
        if (ch == '0' || ch == '1') {
            row.push_back(ch - '0');
        }
    }

    return row;
}

// Prebere graf iz podanega vhodnega toka.
static vector<vector<int>> read_graph(istream& input) {
    int vertexCount;
    if (!(input >> vertexCount)) {
        throw runtime_error("Manjka stevilo vozlisc.");
    }

    string line;
    getline(input, line); // porabimo preostanek prve vrstice

    vector<vector<int>> adjacencyMatrix(vertexCount, vector<int>(vertexCount, 0));

    for (int rowIndex = 0; rowIndex < vertexCount; ++rowIndex) {
        do {
            if (!getline(input, line)) {
                throw runtime_error("Premalo vrstic v vhodu.");
            }
        } while (line.empty());

        vector<int> row = parse_adjacency_row(line, vertexCount);
        if ((int)row.size() != vertexCount) {
            throw runtime_error("Napacen format matrike sosednosti.");
        }

        for (int colIndex = 0; colIndex < vertexCount; ++colIndex) {
            adjacencyMatrix[rowIndex][colIndex] = row[colIndex] ? 1 : 0;
        }
    }

    // Graf obravnavamo kot neusmerjen:
    // - diagonala mora biti 0,
    // - matriko simetriziramo.
    for (int i = 0; i < vertexCount; ++i) {
        adjacencyMatrix[i][i] = 0;
        for (int j = i + 1; j < vertexCount; ++j) {
            int value = (adjacencyMatrix[i][j] || adjacencyMatrix[j][i]) ? 1 : 0;
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

        vector<int> candidateVertices(vertexCount);
        for (int i = 0; i < vertexCount; ++i) {
            candidateVertices[i] = i;
        }

        vector<int> currentClique;
        expand(currentClique, candidateVertices);

        // Za determinističen izpis uredimo izbrane robove leksikografsko.
        sort(bestClique.begin(), bestClique.end(), EdgeIdComparator(*this));
        return bestClique;
    }

private:
    // Pri dveh enako velikih klikah izberemo leksikografsko manjšo,
    // da je rezultat vedno enak.
    bool is_lexicographically_better(const vector<int>& candidateClique,
                                     const vector<int>& incumbentClique) const {
        if (incumbentClique.empty()) {
            return true;
        }

        vector<pair<int, int>> candidateEdges;
        vector<pair<int, int>> incumbentEdges;
        candidateEdges.reserve(candidateClique.size());
        incumbentEdges.reserve(incumbentClique.size());

        for (int edgeId : candidateClique) {
            candidateEdges.push_back({representedEdges[edgeId].from, representedEdges[edgeId].to});
        }
        for (int edgeId : incumbentClique) {
            incumbentEdges.push_back({representedEdges[edgeId].from, representedEdges[edgeId].to});
        }

        sort(candidateEdges.begin(), candidateEdges.end());
        sort(incumbentEdges.begin(), incumbentEdges.end());

        return lexicographical_compare(candidateEdges.begin(), candidateEdges.end(),
                                       incumbentEdges.begin(), incumbentEdges.end());
    }

    // Pohlepno barvanje kandidatov.
    // Barve uporabimo kot zgornjo mejo za velikost klike,
    // kar omogoča učinkovito obrezovanje v branch-and-bound postopku.
    void greedy_color_sort(const vector<int>& candidates,
                           vector<int>& orderedVertices,
                           vector<int>& colorBounds) const {
        orderedVertices.clear();
        colorBounds.clear();

        if (candidates.empty()) {
            return;
        }

        vector<int> remaining = candidates;
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
            if (currentClique.size() > bestClique.size() ||
                (currentClique.size() == bestClique.size() &&
                 is_lexicographically_better(currentClique, bestClique))) {
                bestClique = currentClique;
            }
            return;
        }

        vector<int> orderedVertices;
        vector<int> colorBounds;
        greedy_color_sort(candidates, orderedVertices, colorBounds);

        for (int i = (int)orderedVertices.size() - 1; i >= 0; --i) {
            // Če niti v najboljšem primeru ne moremo izboljšati trenutne rešitve,
            // celotno vejo odrežemo.
            if (currentClique.size() + (size_t)colorBounds[i] < bestClique.size()) {
                return;
            }

            int chosenVertex = orderedVertices[i];
            currentClique.push_back(chosenVertex);

            // Naslednji kandidati so le sosedi izbranega vozlišča,
            // saj mora množica ostati klika.
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
// To je dobra optimizacija, ker lahko vsako komponento rešujemo ločeno.
static vector<vector<int>> find_connected_components(const vector<vector<int>>& adjacencyMatrix) {
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
                           const vector<vector<int>>& adjacencyMatrix) {
    // Robova ne smeta deliti krajišča.
    if (firstEdge.from == secondEdge.from ||
        firstEdge.from == secondEdge.to ||
        firstEdge.to == secondEdge.from ||
        firstEdge.to == secondEdge.to) {
        return false;
    }

    // Med krajišči različnih robov ne sme biti nobene povezave.
    if (adjacencyMatrix[firstEdge.from][secondEdge.from] ||
        adjacencyMatrix[firstEdge.from][secondEdge.to] ||
        adjacencyMatrix[firstEdge.to][secondEdge.from] ||
        adjacencyMatrix[firstEdge.to][secondEdge.to]) {
        return false;
    }

    return true;
}

// Reši eno povezano komponento originalnega grafa.
static vector<pair<int, int>> solve_component(const vector<vector<int>>& adjacencyMatrix,
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

    // Zgradimo pomožni graf kompatibilnosti nad robovi originalnega grafa.
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

    // Maksimalna klika v pomožnem grafu predstavlja iskano rešitev.
    vector<int> bestEdgeIds = solver.solve();

    vector<pair<int, int>> result;
    result.reserve(bestEdgeIds.size());

    // V izhodu uporabljamo 1-based indeksiranje vozlišč.
    for (int edgeId : bestEdgeIds) {
        const Edge& edge = componentEdges[edgeId];
        result.push_back({edge.from + 1, edge.to + 1});
    }

    sort(result.begin(), result.end());
    return result;
}

// Reši celoten graf.
static vector<pair<int, int>> solve_graph(const vector<vector<int>>& adjacencyMatrix) {
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
        vector<vector<int>> adjacencyMatrix;

        // argv[1] = vhodna datoteka (če obstaja)
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

        // Merimo samo čas glavnega algoritma.
        auto startTime = chrono::steady_clock::now();
        vector<pair<int, int>> solution = solve_graph(adjacencyMatrix);
        auto endTime = chrono::steady_clock::now();

        double elapsedMilliseconds =
            chrono::duration_cast<chrono::duration<double, milli>>(endTime - startTime).count();

        // argv[2] = izhodna datoteka (če obstaja)
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

        cout << "Time: " << elapsedMilliseconds << " ms\n";
    } catch (const exception& e) {
        cerr << "Napaka: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
