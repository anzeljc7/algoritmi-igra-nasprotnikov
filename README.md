# Igra nasprotnika: Konstrukcija največje ortogonalne CC-množice

Ta repozitorij vsebuje rešitev problema **Konstrukcija največje ortogonalne CC-množice**.
Za podan graf je cilj poiskati čim večjo množico parov sosednjih vozlišč tako, da med vozlišči različnih izbranih parov ni nobene povezave.

## Struktura projekta

Spodaj je prikazana bistvena struktura projekta:

```text
.
├── solution.cpp
├── input/
├── output/
└── README.md
```

- `solution.cpp` – glavna rešitev
- `input/` – vhodne datoteke
- `output/` – izhodne datoteke

## Prevajanje

Program prevedeš z ukazom:

```bash
g++ -O3 solution.cpp -o solution -std=c++17
```

## Zagon

### Z vhodno datoteko

```bash
./solution pot/do/vhoda.txt
```

### Z vhodno in izhodno datoteko

```bash
./solution pot/do/vhoda.txt pot/do/izhoda.txt
```

Če izhodne datoteke ne podaš, se rezultat izpiše na standardni izhod.
Program na koncu izpiše tudi čas izvajanja algoritma.

## Vhodni format

Vhodna datoteka vsebuje:

- v prvi vrstici število vozlišč `n`
- nato `n` vrstic matrike sosednosti

Primer:

```text
7
0 0 1 0 0 0 0
0 0 0 0 0 1 0
1 0 0 0 0 0 1
0 0 0 0 1 0 1
0 0 0 1 0 0 0
0 1 0 0 0 0 0
0 0 1 1 0 0 0
```

## Izhodni format

Izhod vsebuje:

- v prvi vrstici število najdenih parov
- nato po en par vozlišč v vsaki vrstici

Primer:

```text
3
1 3
2 6
4 5
```
