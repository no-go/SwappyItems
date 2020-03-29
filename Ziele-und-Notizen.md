# gegeben ist...

i)   eine Datenmenge, die 100x größer ist, als der Arbeitsspeicher eines Rechners.
ii)  die Dateneinträge sind mit einem 64Bit Schlüssel identifizierbar.
iii) ein Multicore-Rechner mit SSD oder HDD ohne Netzwerkschnittstelle.

# gesucht ist...

1.1) eine Graphstruktur, die sich aus ca 1/5 der Datenmenge ergibt.
1.2) eine effiziente Speicherung von Kantengewicht, Abbiegebedingungen,
     Kantenrichtung, Kantenname und ggf. Abbiegekosten
2) ein kürzester Weg durch diese Graphstruktur zwischen 2 ausgewählten Knoten.
3) eine schnelle Verarbeitungsform und effiziente Speicherung, um (2)
   für beliebige neue Knoten zu ermitteln.
   
# problematisch ist...

a) bereits die koventionell Verwaltung aller Schlüssel (i und ggf. 1.1) ohne
   Daten passt nicht in den Arbeitsspeicher
b) selbst SSD als Massenspeicher benötigt weiterhin eine sinnvolle Balancierung
   der Daten zwischen SSD und Arbeitsspeicher
c) die Verwendung des virtuellen Speichers mit "Swap" Dateien/Partition
   ist nicht gut verwendbar, da sie weder für spezielle Zugriffmuster, noch
   für spezielle Datenstrukturen optimierbar ist
d) die Priority Queue bei der Suche kürzester Wege muss...
   - eine "sortierte" Ausgabe der Kanten nach Priorität bieten
   - einen schnellen Zugriff (via Schlüssel) für eine Reduzierung
     einer Priorität einer Kante bieten
   - Verwaltungsstruktur sowie Daten müssen ggf. auch auf SSD ausgelagert
     werden können

# Lösungsansätze und Herausforderungen

## Flat, Sorted, Bloomfilters, Most Recently Used

- nutze ich ....
- ...

Most Recently Used:

- keine Statistik nötig (nur eine Liste als "Zeitstempel")
- Blick auf einen aktuell wichtigen "Working-Set"

Most Frequently Used: 

- wichtig bei Zugriffen, die sich auf einige wenige Daten konzentrieren
- es fehlt die Dynamik einer Alterung (ggf. nachrüstbar)

## Looser/Tournament Tree

- gut verwendbar für min/max Heaps (= priority Queue)
- minimiert vergleiche durch vorhalten von Teilinfos
- Daten sind sortieren Sequenzen hinterlegt (runs/merges/k-merge)
- Prozesse lassen sich Puffern, um Zeit für ext. Datenstrukturen zu gewinnen

## Sequence Heaps

- gut für priority Queues mit externen Datenstrukturen
- Mischung aus Gruppen (Buckets?), Vorsortierungen und Tournament Tree

## Log-structured merge-tree (LSM)

- vermutlich identisch mit "Sequence Heaps" (?)
- viele Systeme für große Datenmengen nutzen das

## 2-level Bucket Heap

- eines der besten Verfahren für eine priority Queue
- scheinbar nicht im Fokus von externen Datenstrukturen betrachtet worden (?)

### Buckets und probabilistisches Zählen

probabilistisches Zählen: könnte man für "Most Frequently Used" nutzen,
um Keys in eine "Häufigkeitsgruppe" abzulegen?

## Adressable Priority Queue(s)

- in der Regel neben Heap/Baum für PQ eine verlinkung
- wie ist diese Verkettung in externen sowie internen Datenstrukturen (?)
- wie geht man bei der Verkettung mit "Updates" um (andere Prio) ?

### Lookup Tables

- auf dem Papier können Algorithmen so schnell sein
- leider für große Datenmengen nicht praktikabel

### Fibonacci Heap

- Konzept: Das Chaos zu Anfang anwachsen zu lassen, kann sich später
  als Vorteil erweisen
- auf dem Papier können Algorithmen so schnell sein
- diverse große Konstanten lassen an der praktischen Verwendung zweifeln
- 

### Pairing Heap

- die leichtgewichtige form der Fibonacci Heaps
