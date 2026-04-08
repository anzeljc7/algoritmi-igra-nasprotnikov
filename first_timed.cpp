#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint> // <--- DODANO ZA uint64_t
#include <chrono>

// Dodano za ustvarjanje map (ločeno za Windows in Linux/macOS)
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// --- DODATKI ZA BITNE OPERACIJE (Cross-platform) ---
#ifdef _MSC_VER
#include <intrin.h>
#endif

using namespace std;

// --- BITNE FUNKCIJE ---
#ifdef _MSC_VER
inline int popcount64(uint64_t x)
{
    return (int)__popcnt64(x);
}
inline int ctz64(uint64_t x)
{
    unsigned long index;
    if (_BitScanForward64(&index, x))
        return (int)index;
    return 64;
}
#else
inline int popcount64(uint64_t x)
{
    return __builtin_popcountll(x);
}
inline int ctz64(uint64_t x)
{
    if (x == 0)
        return 64;
    return __builtin_ctzll(x);
}
#endif

// Struktura za hitro računanje z bitnimi množicami (optimizacija drevesa)
struct FastBitset
{
    std::vector<uint64_t> words;
    int num_bits;

    FastBitset(int n = 0)
    {
        num_bits = n;
        words.assign((n + 63) / 64, 0);
    }

    void set(int i) { words[i / 64] |= (1ULL << (i % 64)); }
    void reset(int i) { words[i / 64] &= ~(1ULL << (i % 64)); }

    int count() const
    {
        int c = 0;
        for (uint64_t w : words)
            c += popcount64(w);
        return c;
    }

    int find_first() const
    {
        for (size_t i = 0; i < words.size(); ++i)
        {
            if (words[i] != 0)
                return (int)(i * 64) + ctz64(words[i]);
        }
        return -1;
    }

    void set_all()
    {
        for (size_t i = 0; i < words.size(); ++i)
            words[i] = ~0ULL;
        if (num_bits % 64 != 0)
        {
            words.back() &= (1ULL << (num_bits % 64)) - 1;
        }
    }
};

// Pomožna funkcija za bitni presek (A AND NOT B)
FastBitset bitwise_and_not(const FastBitset &a, const FastBitset &b)
{
    FastBitset res(a.num_bits);
    for (size_t i = 0; i < a.words.size(); ++i)
    {
        res.words[i] = a.words[i] & ~b.words[i];
    }
    return res;
}

// ---------------------------------------------------------
// BRANJE IN PISANJE (Tvoja koda)
// ---------------------------------------------------------
vector<pair<int, int>> readGraph(const string &filename, int &n)
{
    ifstream infile(filename);
    if (!infile.is_open())
    {
        cerr << "Error: Could not open file '" << filename << "'!" << endl;
        n = 0; // Oznaka, da branje ni uspelo
        return {};
    }

    if (!(infile >> n))
    {
        cerr << "Error: Failed to read number of nodes in '" << filename << "'." << endl;
        n = 0;
        return {};
    }

    vector<pair<int, int>> edges;
    string line;
    getline(infile, line); // flush newline after infile >> n

    for (int i = 0; i < n; ++i)
    {
        if (!getline(infile, line))
        {
            cerr << "Error: Unexpected end of file at row " << i << " in '" << filename << "'." << endl;
            n = 0;
            return {};
        }

        string cleanLine = "";
        for (char c : line)
        {
            if (c == '0' || c == '1')
            {
                cleanLine += c;
            }
        }

        for (int j = i + 1; j < n; ++j)
        {
            if (j < (int)cleanLine.length() && cleanLine[j] == '1')
            {
                edges.push_back({i + 1, j + 1});
            }
        }
    }

    infile.close();
    cout << "Read " << n << " nodes and found " << edges.size() << " edges." << endl;
    return edges;
}

void writeResults(const string &filename, const vector<pair<int, int>> &foundPairs)
{
    ofstream outfile(filename);
    if (!outfile.is_open())
    {
        cerr << "Error: Could not create file '" << filename << "'! (Make sure the output directory exists)" << endl;
        return;
    }

    outfile << foundPairs.size() << "\n";
    for (size_t i = 0; i < foundPairs.size(); ++i)
    {
        outfile << foundPairs[i].first << " " << foundPairs[i].second << "\n";
    }

    outfile.close();
    cout << "Solution successfully saved to '" << filename << "'." << endl;
}

// ---------------------------------------------------------
// REKURZIJA ZA BRANCH & BOUND (STRICTLY SINGLE-THREADED)
// ---------------------------------------------------------
void branch_and_bound(FastBitset candidates, std::vector<int> &current_is,
                      int &max_size, std::vector<int> &best_is,
                      const std::vector<FastBitset> &conflict_adj)
{
    // Obrezovanje (Pruning): Če trenutna velikost + preostali kandidati ne morejo preseči najboljše rešitve
    if (current_is.size() + candidates.count() <= max_size)
    {
        return;
    }

    int v = candidates.find_first();
    // Če ni več kandidatov, preverimo, če imamo novo najboljšo rešitev
    if (v == -1)
    {
        if (current_is.size() > max_size)
        {
            max_size = (int)current_is.size();
            best_is = current_is;
        }
        return;
    }

    // VEJA 1: Vključimo vozlišče 'v' v rešitev
    current_is.push_back(v);
    FastBitset new_candidates = bitwise_and_not(candidates, conflict_adj[v]);
    new_candidates.reset(v); // 'v' ne more biti več kandidat
    branch_and_bound(new_candidates, current_is, max_size, best_is, conflict_adj);
    current_is.pop_back(); // backtracking

    // VEJA 2: Izključimo vozlišče 'v' iz rešitve
    candidates.reset(v);
    branch_and_bound(candidates, current_is, max_size, best_is, conflict_adj);
}

// ---------------------------------------------------------
// GLAVNI ALGORITEM ZA REŠEVANJE
// ---------------------------------------------------------
vector<pair<int, int>> solveOrthogonalCCSet(int n, const vector<pair<int, int>> &edges)
{
    if (edges.empty())
        return {};

    // 1. Zgradimo matriko sosednosti originalnega grafa za O(1) poizvedbe
    vector<vector<bool>> orig_adj(n + 1, vector<bool>(n + 1, false));
    for (const auto &e : edges)
    {
        orig_adj[e.first][e.second] = true;
        orig_adj[e.second][e.first] = true;
    }

    int E = edges.size();

    // 2. Tvorba KONFLIKTNEGA GRAFA
    std::vector<FastBitset> conflict_adj(E, FastBitset(E));

    for (int i = 0; i < E; ++i)
    {
        for (int j = i + 1; j < E; ++j)
        {
            auto e1 = edges[i];
            auto e2 = edges[j];

            // Povezavi sta v konfliktu če:
            // a) si delita isto vozlišče
            bool share_vertex = (e1.first == e2.first || e1.first == e2.second ||
                                 e1.second == e2.first || e1.second == e2.second);

            // b) obstaja povezava med katerimkoli vozliščem iz e1 in e2
            bool connected = (orig_adj[e1.first][e2.first] || orig_adj[e1.first][e2.second] ||
                              orig_adj[e1.second][e2.first] || orig_adj[e1.second][e2.second]);

            if (share_vertex || connected)
            {
                conflict_adj[i].set(j);
                conflict_adj[j].set(i);
            }
        }
    }

    // 3. Priprava na Branch and Bound
    FastBitset initial_candidates(E);
    initial_candidates.set_all();

    std::vector<int> current_is;
    std::vector<int> best_is;
    int max_size = 0;

    // Zagon rekurzije
    branch_and_bound(initial_candidates, current_is, max_size, best_is, conflict_adj);

    // 4. Preslikava nazaj v pare
    vector<pair<int, int>> result;
    for (int idx : best_is)
    {
        result.push_back(edges[idx]);
    }

    cout << "Najdena je najvecja ortogonalna CC-mnozica velikosti: " << result.size() << endl;
    return result;
}

// ---------------------------------------------------------
// MAIN FUNKCIJA (Tvoja koda)
// ---------------------------------------------------------
int main()
{
    string inputDir = "input";
    string outputDir = "output";
    string targetFileName = "05.txt";

// KREIRANJE MAPE
#ifdef _WIN32
    _mkdir(outputDir.c_str());
#else
    mkdir(outputDir.c_str(), 0777);
#endif

    string inputFile = inputDir + "/" + targetFileName;

    // Ročno iskanje imena brez končnice
    size_t dotPos = targetFileName.find_last_of('.');
    string stem = (dotPos == string::npos) ? targetFileName : targetFileName.substr(0, dotPos);
    string ext = (dotPos == string::npos) ? "" : targetFileName.substr(dotPos);

    // Zgradi ime izhodne datoteke
    string outputFile = outputDir + "/" + stem + "-out" + ext;

    cout << "\n---------------------------------------------------------" << endl;
    cout << "Processing: " << inputFile << endl;

    int n = 0;
    vector<pair<int, int>> edges = readGraph(inputFile, n);

    // Če je branje uspelo, zaženi algoritem
    if (n != 0)
    {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        vector<pair<int, int>> foundPairs = solveOrthogonalCCSet(n, edges);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli> >(end - start).count();
        writeResults(outputFile, foundPairs);

        cout << "Time: " << elapsed_ms << " ms\n";
    }
    else
    {
        cerr << "Skipping algorithm due to read error. (Make sure 'input' folder and file exist!)" << endl;
    }

    cout << "\nFinished processing." << endl;

    return 0;
}