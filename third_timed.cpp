#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <chrono>

using namespace std;

// Preprosta dinamična bitna množica.
// Uporabimo jo za shranjevanje sosednosti v pomožnem grafu,
// ker je testiranje povezave med dvema vozliščema potem zelo hitro.
struct DynamicBitset {
    vector<unsigned long long> w;

    DynamicBitset() {}

    // Ustvari bitset za nbits bitov.
    explicit DynamicBitset(int nbits) : w((nbits + 63) / 64, 0ULL) {}

    // Ponastavi velikost in vse bite nastavi na 0.
    void reset(int nbits) {
        w.assign((nbits + 63) / 64, 0ULL);
    }

    // Nastavi bit na poziciji pos na 1.
    void set(int pos) {
        w[pos >> 6] |= (1ULL << (pos & 63));
    }

    // Preveri, ali je bit na poziciji pos nastavljen.
    bool test(int pos) const {
        return ((w[pos >> 6] >> (pos & 63)) & 1ULL) != 0ULL;
    }
};

// Povezava v originalnem grafu.
// Ker v pomožnem grafu delamo nad povezavami originalnega grafa,
// vsaka taka povezava postane "vozlišče" v pomožnem grafu.
struct Edge {
    int u;
    int v;
};

// Poskusi prebrati eno vrstico matrike sosednosti.
// Podpira oba formata:
// 1) vrednosti ločene s presledki, npr. 0 1 0 1
// 2) strnjeno zapisane vrednosti, npr. 0101
static vector<int> parse_row_tokens(const string& line, int n) {
    vector<int> row;
    row.reserve(n);

    bool has_separator = false;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == ' ' || line[i] == '\t') {
            has_separator = true;
            break;
        }
    }

    // Najprej poskusimo format s presledki.
    if (has_separator) {
        stringstream ss(line);
        int x;
        while (ss >> x) row.push_back(x ? 1 : 0);
        if ((int)row.size() == n) return row;
        row.clear();
    }

    // Če to ni uspelo, preberemo vse znake 0/1 brez ločil.
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '0' || line[i] == '1') row.push_back(line[i] - '0');
    }

    return row;
}

// Prebere vhodni graf iz toka.
// Vhod je matrika sosednosti velikosti n x n.
static vector< vector<int> > read_graph(istream& in) {
    int n;
    if (!(in >> n)) {
        throw runtime_error("Manjka stevilo vozlisc.");
    }

    string line;
    getline(in, line);

    vector< vector<int> > a(n, vector<int>(n, 0));

    for (int i = 0; i < n; ++i) {
        // Preskočimo morebitne prazne vrstice.
        do {
            if (!getline(in, line)) {
                throw runtime_error("Premalo vrstic v vhodu.");
            }
        } while (line.size() == 0);

        vector<int> row = parse_row_tokens(line, n);
        if ((int)row.size() != n) {
            throw runtime_error("Napacen format matrike sosednosti.");
        }

        for (int j = 0; j < n; ++j) {
            a[i][j] = row[j] ? 1 : 0;
        }
    }

    // Normalizacija matrike:
    // - diagonala mora biti 0
    // - graf obravnavamo kot neusmerjen, zato simetriziramo matriko
    for (int i = 0; i < n; ++i) {
        a[i][i] = 0;
        for (int j = i + 1; j < n; ++j) {
            int val = (a[i][j] || a[j][i]) ? 1 : 0;
            a[i][j] = val;
            a[j][i] = val;
        }
    }

    return a;
}

// Razred za iskanje maksimalne klike v pomožnem grafu.
// Ideja rešitve:
// - vsaka povezava originalnega grafa postane vozlišče v pomožnem grafu,
// - dve taki "vozlišči" v pomožnem grafu povežemo, če sta originalni povezavi kompatibilni,
//   torej če se njuni krajišči ne prekrivata in med njimi ni dodatnih povezav,
// - iskana največja ortogonalna CC-množica je zato enaka maksimalni kliki v tem pomožnem grafu.
struct CliqueSolver {
    int n;
    vector<DynamicBitset> adj;
    vector<Edge> edges;
    vector<int> best;

    CliqueSolver() : n(0) {}

    // Primerjava dveh rešitev leksikografsko po robovih.
    // To uporabljamo samo pri izenačenem številu elementov,
    // da dobimo determinističen izhod.
    bool lex_better(const vector<int>& candidate, const vector<int>& incumbent) const {
        if (incumbent.empty()) return true;

        vector< pair<int,int> > ca;
        vector< pair<int,int> > ib;
        ca.reserve(candidate.size());
        ib.reserve(incumbent.size());

        for (size_t i = 0; i < candidate.size(); ++i) {
            int id = candidate[i];
            ca.push_back(make_pair(edges[id].u, edges[id].v));
        }
        for (size_t i = 0; i < incumbent.size(); ++i) {
            int id = incumbent[i];
            ib.push_back(make_pair(edges[id].u, edges[id].v));
        }

        sort(ca.begin(), ca.end());
        sort(ib.begin(), ib.end());
        return lexicographical_compare(ca.begin(), ca.end(), ib.begin(), ib.end());
    }

    // Pohlepno barvanje kandidatov.
    // Namen barvanja ni prava optimalna koloracija, ampak zgornja meja:
    // če potrebujemo k barv, največja klika med temi kandidati ne more biti večja od k.
    // To uporabimo za obrezovanje v branch-and-bound iskanju.
    void color_sort(const vector<int>& P, vector<int>& order, vector<int>& colors) const {
        order.clear();
        colors.clear();
        if (P.empty()) return;

        vector<int> remaining = P;
        order.reserve(P.size());
        colors.reserve(P.size());

        int color = 0;
        while (!remaining.empty()) {
            ++color;
            vector<int> this_color;
            vector<int> next_remaining;

            for (size_t i = 0; i < remaining.size(); ++i) {
                int v = remaining[i];
                bool can_take = true;

                // Vozlišče lahko dobi trenutno barvo,
                // če ni povezano z nobenim že obarvanim vozliščem iste barve.
                for (size_t j = 0; j < this_color.size(); ++j) {
                    int u = this_color[j];
                    if (adj[v].test(u)) {
                        can_take = false;
                        break;
                    }
                }

                if (can_take) this_color.push_back(v);
                else next_remaining.push_back(v);
            }

            // V order shranimo vrstni red, v colors pa številko barve,
            // ki predstavlja zgornjo mejo za velikost klike do tiste pozicije.
            for (size_t i = 0; i < this_color.size(); ++i) {
                order.push_back(this_color[i]);
                colors.push_back(color);
            }

            remaining.swap(next_remaining);
        }
    }

    // Rekurzivni branch-and-bound za maksimalno kliko.
    // current = trenutna klika
    // P       = kandidati, ki jih še lahko dodamo v current
    void expand(vector<int>& current, const vector<int>& P) {
        // Če ni več kandidatov, smo dobili eno maksimalno kliko.
        if (P.empty()) {
            if (current.size() > best.size() ||
                (current.size() == best.size() && lex_better(current, best))) {
                best = current;
            }
            return;
        }

        vector<int> order;
        vector<int> colors;
        color_sort(P, order, colors);

        // Kandidate obravnavamo od zadaj naprej.
        for (int i = (int)order.size() - 1; i >= 0; --i) {
            // Obrezovanje:
            // current.size() + colors[i] je zgornja meja,
            // kako velika rešitev še lahko nastane iz te veje.
            // Če je strogo manjša od najboljše, se ne splača nadaljevati.
            if (current.size() + (size_t)colors[i] < best.size()) return;

            int v = order[i];
            current.push_back(v);

            // Novi kandidati so samo tisti, ki so povezani z v,
            // ker mora ostati množica klika.
            vector<int> nextP;
            nextP.reserve((size_t)i);
            for (int j = 0; j < i; ++j) {
                int u = order[j];
                if (adj[v].test(u)) nextP.push_back(u);
            }

            expand(current, nextP);
            current.pop_back();
        }
    }

    // Zažene iskanje maksimalne klike na celotnem pomožnem grafu.
    vector<int> solve() {
        best.clear();

        vector<int> P(n);
        for (int i = 0; i < n; ++i) P[i] = i;

        vector<int> current;
        expand(current, P);

        // Za lep in determinističen izpis še enkrat uredimo robove.
        sort(best.begin(), best.end(), CompareEdgeIds(*this));
        return best;
    }

    // Primerjalnik indeksov robov po dejanskih krajiščih.
    struct CompareEdgeIds {
        const CliqueSolver& self;

        CompareEdgeIds(const CliqueSolver& solver) : self(solver) {}

        bool operator()(int a, int b) const {
            const Edge& ea = self.edges[a];
            const Edge& eb = self.edges[b];
            if (ea.u != eb.u) return ea.u < eb.u;
            return ea.v < eb.v;
        }
    };
};

// Razbije graf na povezane komponente.
// To je pomembna optimizacija, ker lahko vsako komponento rešujemo posebej.
// Ker med različnimi komponentami ni povezav, se rešitve med seboj ne motijo.
static vector< vector<int> > connected_components(const vector< vector<int> >& a) {
    int n = (int)a.size();
    vector<int> vis(n, 0);
    vector< vector<int> > comps;

    for (int s = 0; s < n; ++s) {
        if (vis[s]) continue;

        vector<int> comp;
        queue<int> q;
        q.push(s);
        vis[s] = 1;

        // Klasičen BFS po matriki sosednosti.
        while (!q.empty()) {
            int v = q.front();
            q.pop();
            comp.push_back(v);

            for (int u = 0; u < n; ++u) {
                if (a[v][u] && !vis[u]) {
                    vis[u] = 1;
                    q.push(u);
                }
            }
        }

        comps.push_back(comp);
    }

    return comps;
}

// Reši problem samo na eni povezani komponenti.
static vector< pair<int,int> > solve_component(const vector< vector<int> >& a, const vector<int>& comp) {
    vector<int> verts = comp;
    sort(verts.begin(), verts.end());

    // Najprej zberemo vse robove znotraj komponente originalnega grafa.
    vector<Edge> edges;
    for (size_t i = 0; i < verts.size(); ++i) {
        for (size_t j = i + 1; j < verts.size(); ++j) {
            if (a[verts[i]][verts[j]]) {
                Edge e;
                e.u = verts[i];
                e.v = verts[j];
                edges.push_back(e);
            }
        }
    }

    sort(edges.begin(), edges.end(), [](const Edge& x, const Edge& y) {
        if (x.u != y.u) return x.u < y.u;
        return x.v < y.v;
    });

    int m = (int)edges.size();
    if (m == 0) return vector< pair<int,int> >();

    CliqueSolver solver;
    solver.n = m;
    solver.edges = edges;
    solver.adj.assign(m, DynamicBitset(m));

    // Zgradimo pomožni graf kompatibilnosti.
    // Vozlišče i predstavlja rob edges[i].
    // Dve vozlišči i in j povežemo, če sta ustrezna roba kompatibilna,
    // torej ju lahko hkrati vzamemo v ortogonalno CC-množico.
    for (int i = 0; i < m; ++i) {
        for (int j = i + 1; j < m; ++j) {
            const Edge& e1 = edges[i];
            const Edge& e2 = edges[j];

            bool compatible = true;

            // Roka ne smeta deliti vozlišča.
            if (e1.u == e2.u || e1.u == e2.v || e1.v == e2.u || e1.v == e2.v) {
                compatible = false;
            }

            // Nobeno krajišče prvega roba ne sme biti povezano z nobenim krajiščem drugega roba.
            // To je natančno pogoj iz definicije ortogonalne CC-množice.
            if (compatible && (a[e1.u][e2.u] || a[e1.u][e2.v] || a[e1.v][e2.u] || a[e1.v][e2.v])) {
                compatible = false;
            }

            if (compatible) {
                solver.adj[i].set(j);
                solver.adj[j].set(i);
            }
        }
    }

    // Največja klika v pomožnem grafu nam da iskano največjo množico robov.
    vector<int> best_ids = solver.solve();

    vector< pair<int,int> > answer;
    answer.reserve(best_ids.size());

    // Pretvorimo nazaj v 1-based izpis vozlišč.
    for (size_t i = 0; i < best_ids.size(); ++i) {
        const Edge& e = edges[best_ids[i]];
        answer.push_back(make_pair(e.u + 1, e.v + 1));
    }

    sort(answer.begin(), answer.end());
    return answer;
}

// Reši celoten graf.
// Ker komponente rešujemo ločeno, samo združimo delne odgovore.
static vector< pair<int,int> > solve_graph(const vector< vector<int> >& a) {
    vector< vector<int> > comps = connected_components(a);
    vector< pair<int,int> > answer;

    for (size_t i = 0; i < comps.size(); ++i) {
        vector< pair<int,int> > part = solve_component(a, comps[i]);
        answer.insert(answer.end(), part.begin(), part.end());
    }

    sort(answer.begin(), answer.end());
    return answer;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(NULL);

    try {
        vector< vector<int> > a;

        // Če je podan prvi argument, beremo iz datoteke.
        // Sicer beremo s standardnega vhoda.
        if (argc >= 2) {
            ifstream fin(argv[1]);
            if (!fin) {
                cerr << "Napaka: ne morem odpreti vhodne datoteke.\n";
                return 1;
            }
            a = read_graph(fin);
        } else {
            a = read_graph(cin);
        }

        // Merjenje časa samo za glavni algoritem reševanja,
        // brez branja vhoda in brez izpisa rezultata.
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        vector< pair<int,int> > ans = solve_graph(a);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli> >(end - start).count();

        // Če je podan še drugi argument, rezultat zapišemo v datoteko.
        // Sicer ga izpišemo na standardni izhod.
        if (argc >= 3) {
            ofstream fout(argv[2]);
            if (!fout) {
                cerr << "Napaka: ne morem odpreti izhodne datoteke.\n";
                return 1;
            }
            fout << ans.size() << '\n';
            for (size_t i = 0; i < ans.size(); ++i) {
                fout << ans[i].first << ' ' << ans[i].second << '\n';
            }
        } else {
            cout << ans.size() << '\n';
            for (size_t i = 0; i < ans.size(); ++i) {
                cout << ans[i].first << ' ' << ans[i].second << '\n';
            }
        }

        cout << "Time: " << elapsed_ms << " ms\n";
    } catch (const exception& e) {
        cerr << "Napaka: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
