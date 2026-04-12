#include <iostream>
#include <fstream>
#include <vector>
#include <string>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Uporaba: ./checker <input.txt> <output.txt>\n";
        return 0;
    }

    ifstream in(argv[1]);
    if (!in) {
        cout << "Napaka: vhodne datoteke ni mogoce odpreti.\n";
        return 0;
    }

    int n;
    in >> n;
    if (!in || n < 0) {
        cout << "Napaka: neveljavno stevilo vozlisc.\n";
        return 0;
    }

    vector<vector<int> > g(n, vector<int>(n, 0));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!(in >> g[i][j])) {
                cout << "Napaka: matrika sosednosti nima vseh elementov.\n";
                return 0;
            }
        }
    }

    ifstream out(argv[2]);
    if (!out) {
        cout << "Napaka: izhodne datoteke ni mogoce odpreti.\n";
        return 0;
    }

    int m;
    if (!(out >> m)) {
        cout << "Napaka: izhod je prazen ali pa ni pravilno zapisan.\n";
        return 0;
    }

    if (m < 0) {
        cout << "Napaka: stevilo izpisanih parov ne more biti negativno.\n";
        return 0;
    }

    vector<pair<int, int> > ans;
    for (int i = 0; i < m; i++) {
        int a, b;
        if (!(out >> a >> b)) {
            cout << "Napaka: v izhodu manjka kaksen par vozlisc.\n";
            return 0;
        }
        ans.push_back(make_pair(a - 1, b - 1));
    }

    for (int i = 0; i < m; i++) {
        int a = ans[i].first;
        int b = ans[i].second;

        if (a < 0 || a >= n || b < 0 || b >= n) {
            cout << "Napaka: vsaj eno vozlisce v paru "
                 << "(" << a + 1 << "," << b + 1 << ") ni v obsegu 1..n.\n";
            return 0;
        }

        if (a == b) {
            cout << "Napaka: par (" << a + 1 << "," << b + 1
                 << ") uporablja isto vozlisce dvakrat.\n";
            return 0;
        }

        if (g[a][b] == 0) {
            cout << "Napaka: par (" << a + 1 << "," << b + 1
                 << ") ni povezava v vhodnem grafu.\n";
            return 0;
        }

        for (int j = i + 1; j < m; j++) {
            int c = ans[j].first;
            int d = ans[j].second;

            if (a == c || a == d || b == c || b == d) {
                cout << "Napaka: para (" << a + 1 << "," << b + 1 << ") in ("
                     << c + 1 << "," << d + 1 << ") si delita vozlisce.\n";
                return 0;
            }

            if (g[a][c] || g[a][d] || g[b][c] || g[b][d]) {
                cout << "Napaka: para (" << a + 1 << "," << b + 1 << ") in ("
                     << c + 1 << "," << d + 1 << ") nista ortogonalna.\n";
                return 0;
            }
        }
    }

    int extra;
    if (out >> extra) {
        cout << "Napaka: v izhodni datoteki so dodatni podatki po zadnjem paru.\n";
        return 0;
    }

    cout << "Resitev je veljavna. Najdenih parov: " << m << "\n";
    return 0;
}
