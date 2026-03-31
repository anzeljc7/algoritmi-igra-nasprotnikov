#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// Dodano za ustvarjanje map (ločeno za Windows in Linux/macOS)
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

using namespace std;

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

// Funkcija, ki sprejme število vozlišč (n) in seznam vseh povezav (edges).
// Vrne seznam parov (povezav), ki tvorijo največjo ortogonalno CC-množico.
vector<pair<int, int>> solveOrthogonalCCSet(int n, const vector<pair<int, int>> &edges)
{
    vector<pair<int, int>> result;

    // ---------------------------------------------------------
    // TUKAJ PRIDE VAŠA LOGIKA
    // ---------------------------------------------------------

    // Primer fiktivnega dodajanja (da program nekaj vrne v izhodno datoteko):
    if (!edges.empty())
    {
        result.push_back(edges[0]);
    }

    return result;
}

int main()
{
    string inputDir = "input";
    string outputDir = "output";
    string targetFileName = "primer.txt";

// KREIRANJE MAPE (Cross-platform za starejši C++)
#ifdef _WIN32
    _mkdir(outputDir.c_str());
#else
    mkdir(outputDir.c_str(), 0777);
#endif

    string inputFile = inputDir + "/" + targetFileName;

    // Ročno iskanje imena brez končnice (za izhodno datoteko)
    size_t dotPos = targetFileName.find_last_of('.');
    string stem = (dotPos == string::npos) ? targetFileName : targetFileName.substr(0, dotPos);
    string ext = (dotPos == string::npos) ? "" : targetFileName.substr(dotPos);

    // Zgradi ime izhodne datoteke (npr. "output/vhod-out.txt")
    string outputFile = outputDir + "/" + stem + "-out" + ext;

    cout << "\n---------------------------------------------------------" << endl;
    cout << "Processing: " << inputFile << endl;

    int n = 0;
    vector<pair<int, int>> edges = readGraph(inputFile, n);

    // Če je branje uspelo, zaženi algoritem
    if (n != 0)
    {
        vector<pair<int, int>> foundPairs = solveOrthogonalCCSet(n, edges);
        writeResults(outputFile, foundPairs);
    }
    else
    {
        cerr << "Skipping algorithm due to read error. (Make sure 'input' folder and file exist!)" << endl;
    }

    cout << "\nFinished processing." << endl;
    return 0;
}