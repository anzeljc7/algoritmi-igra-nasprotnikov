# Igra nasprotnika: Konstrukcija največje ortogonalne CC-množice

To je osnovno ogrodje za reševanje problema "Konstrukcija največje ortogonalne CC-množice" v okviru predmeta/tekmovanja Igra nasprotnika. Program prebere graf v obliki matrike sosednosti iz vhodne datoteke, izvede algoritem in zapiše pare vozlišč v izhodno datoteko.

## 📁 Struktura projekta

Za pravilno delovanje programa mora biti struktura map in datotek naslednja:

├── main.cpp          # Glavna datoteka s C++ kodo (sem prilepi kodo)
├── input/            # Mapa z vhodnimi datotekami
│   └── vhod.txt      # Vhodna datoteka z matriko sosednosti (ime nastaviš v kodi)
└── output/           # Mapa, ki se ustvari samodejno in vsebuje rešitve

V kodi (v funkciji `main`) je trenutno nastavljeno, da prebere specifično datoteko `vhod.txt` iz mape `input`. Če želiš prebrati drugo datoteko, spremeni vrednost spremenljivke `targetFileName`. Za namen testiranja programske kode si lahko vhodne podatke generirate sami.

## ⚙️ Zahteve za prevajanje

Program za delo z mapami in datotekami uporablja knjižnico `<filesystem>`, ki je del standarda **C++17**. Zato je nujno, da prevajalniku izrecno poveš, naj uporabi ta standard. Pri reševanju lahko sicer uporabite poljubni programski jezik.

## 🚀 Kako zagnati program

### 1. Prevajanje (Kompajliranje)
Odpri terminal (ali ukazni poziv) v mapi, kjer se nahaja `main.cpp`. Za prevajanje z GCC/G++ uporabi naslednji ukaz:

```bash
g++ -O3 main.cpp -o cc_mnozica -std=c++17
```

Ta ukaz bo ustvaril izvršljivo datoteko z imenom `cc_mnozica`.

### 2. Zagon
Če je prevajanje uspelo brez napak, program zaženi:

**Na Linux / macOS:**

```bash
./cc_mnozica
```

**Na operacijskem sistemu Windows:**

```bash
cc_mnozica.exe
```

## 🧠 Razvoj algoritma

To ogrodje poskrbi za vse branje in pisanje datotek. Vajina naloga je le, da implementirata logiko iskanja največje podmnožice parov v funkcijo `solveOrthogonalCCSet`.

V tej funkciji velja naslednji pogoj problema: izbrani pari vozlišč morajo biti sosednji, vendar pa nobeno vozlišče iz enega para ne sme biti sosednje z ostalimi vozlišči drugih izbranih parov v podmnožici. Meril se bo čas izvajanja in pravilnost rešitve. Pravilna rešitev ni enolična, različne rešitve z enakim številom parov so vse enako veljavne.