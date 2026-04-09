# Dodatni testni primeri za največjo ortogonalno CC-množico

Vsi grafi so zapisani v istem kompaktnem formatu kot tvoji primeri (brez presledkov med 0/1).
Za vsak graf je priložena tudi ena veljavna optimalna rešitev v ločeni `_solution.txt` datoteki.

| Primer | Opis | n | št. povezav | optimum |
|---|---|---:|---:|---:|
| test01_path12 | Pot P12 | 12 | 11 | 4 |
| test02_cycle15 | Cikel C15 | 15 | 15 | 5 |
| test03_star12 | Zvezda K1,11 | 12 | 11 | 1 |
| test04_k66 | Poln bipartiten graf K6,6 | 12 | 36 | 1 |
| test05_disjoint_edges25 | 25 nepovezanih povezav | 50 | 25 | 25 |
| test06_triangles8 | 8 nepovezanih trikotnikov | 24 | 24 | 8 |
| test07_large_path60 | Velika pot P60 | 60 | 59 | 20 |
| test08_large_cycle90 | Velik cikel C90 | 90 | 90 | 30 |
| test09_stars20_k1_4 | 20 nepovezanih zvezd K1,4 | 100 | 80 | 20 |
| test10_mixed_5K6_5P6 | Mešan graf: 5×K6 + 5×P6 | 60 | 100 | 15 |

## Opombe

- **test01_path12**: Za pot P_n je maksimum induced matchinga enak floor((n+1)/3).
- **test02_cycle15**: Za cikel C_n je maksimum induced matchinga enak floor(n/3).
- **test03_star12**: Vse povezave delijo središče, zato ne moreta biti izbrani dve.
- **test04_k66**: Vsaki dve izbrani povezavi imata med krajišči vedno še vsaj eno dodatno povezavo.
- **test05_disjoint_edges25**: To je že induced matching, zato vzamemo vse povezave.
- **test06_triangles8**: V vsakem trikotniku lahko vzameš največ eno povezavo; komponente so nepovezane.
- **test07_large_path60**: Ponovi vzorec “izberi eno povezavo, preskoči dve vozlišči”.
- **test08_large_cycle90**: Za večji cikel se optimum lepo periodično ponavlja na vsakih 3 vozlišča.
- **test09_stars20_k1_4**: Iz vsake komponente lahko vzameš natanko eno povezavo.
- **test10_mixed_5K6_5P6**: Goste komponente K6 prispevajo po 1, poti P6 pa po 2, skupaj 5+10=15.
