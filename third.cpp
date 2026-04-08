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

using namespace std;

struct DynamicBitset {
    vector<unsigned long long> w;

    DynamicBitset() {}
    explicit DynamicBitset(int nbits) : w((nbits + 63) / 64, 0ULL) {}

    void reset(int nbits) {
        w.assign((nbits + 63) / 64, 0ULL);
    }

    void set(int pos) {
        w[pos >> 6] |= (1ULL << (pos & 63));
    }

    bool test(int pos) const {
        return ((w[pos >> 6] >> (pos & 63)) & 1ULL) != 0ULL;
    }
};

struct Edge {
    int u;
    int v;
};

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

    if (has_separator) {
        stringstream ss(line);
        int x;
        while (ss >> x) row.push_back(x ? 1 : 0);
        if ((int)row.size() == n) return row;
        row.clear();
    }

    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '0' || line[i] == '1') row.push_back(line[i] - '0');
    }

    return row;
}

static vector< vector<int> > read_graph(istream& in) {
    int n;
    if (!(in >> n)) {
        throw runtime_error("Manjka stevilo vozlisc.");
    }

    string line;
    getline(in, line);

    vector< vector<int> > a(n, vector<int>(n, 0));

    for (int i = 0; i < n; ++i) {
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

struct CliqueSolver {
    int n;
    vector<DynamicBitset> adj;
    vector<Edge> edges;
    vector<int> best;

    CliqueSolver() : n(0) {}

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

            for (size_t i = 0; i < this_color.size(); ++i) {
                order.push_back(this_color[i]);
                colors.push_back(color);
            }

            remaining.swap(next_remaining);
        }
    }

    void expand(vector<int>& current, const vector<int>& P) {
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

        for (int i = (int)order.size() - 1; i >= 0; --i) {
            if (current.size() + (size_t)colors[i] < best.size()) return;

            int v = order[i];
            current.push_back(v);

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

    vector<int> solve() {
        best.clear();
        vector<int> P(n);
        for (int i = 0; i < n; ++i) P[i] = i;

        vector<int> current;
        expand(current, P);

        sort(best.begin(), best.end(), CompareEdgeIds(*this));
        return best;
    }

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

static vector< pair<int,int> > solve_component(const vector< vector<int> >& a, const vector<int>& comp) {
    vector<int> verts = comp;
    sort(verts.begin(), verts.end());

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

    for (int i = 0; i < m; ++i) {
        for (int j = i + 1; j < m; ++j) {
            const Edge& e1 = edges[i];
            const Edge& e2 = edges[j];

            bool compatible = true;

            if (e1.u == e2.u || e1.u == e2.v || e1.v == e2.u || e1.v == e2.v) {
                compatible = false;
            }
            if (compatible && (a[e1.u][e2.u] || a[e1.u][e2.v] || a[e1.v][e2.u] || a[e1.v][e2.v])) {
                compatible = false;
            }

            if (compatible) {
                solver.adj[i].set(j);
                solver.adj[j].set(i);
            }
        }
    }

    vector<int> best_ids = solver.solve();
    vector< pair<int,int> > answer;
    answer.reserve(best_ids.size());

    for (size_t i = 0; i < best_ids.size(); ++i) {
        const Edge& e = edges[best_ids[i]];
        answer.push_back(make_pair(e.u + 1, e.v + 1));
    }

    sort(answer.begin(), answer.end());
    return answer;
}

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

        vector< pair<int,int> > ans = solve_graph(a);

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
    } catch (const exception& e) {
        cerr << "Napaka: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
