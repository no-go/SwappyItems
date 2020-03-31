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
e) Konflikte im Pseudocode
   - PrioQueue braucht Distanz als Priorität
   - Distanz wird aber auch in einer anderen Struktur abgelegt und upgedatet
   - das Update der Distanz/Prio in Queue zu einem gegebenen Knoten wird nicht beschrieben

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

Buckets

- Bucket Grenzen updaten
- ein Problem ist, nicht leere Buckets zu finden bei extractMin (Lösung: eigener Heap für die Keys)

## Adressable Priority Queue(s)

- Konzepte eher parallel, also zusätzlich zur ext. PQ
- wie ist diese Verkettung in externen sowie internen Datenstrukturen (?)
- wie geht man bei der Verkettung mit "Updates" um (andere Prio) ?
- Möglicherweise unvereinbar mit der Datenstruktur von Sequence Heaps (?)

### Lookup Tables

- auf dem Papier können Algorithmen so schnell sein
- leider für große Datenmengen nicht praktikabel

### Fibonacci Heap

- Konzept: Das Chaos zu Anfang anwachsen zu lassen, kann sich später
  als Vorteil erweisen
- auf dem Papier können Algorithmen so schnell sein
- diverse große Konstanten lassen an der praktischen Verwendung zweifeln
- im Baum müssen Markierung gespeichert werden, was die Datenmenge aufbläht

### Pairing Heap

- die leichtgewichtige form der Fibonacci Heaps

# Probleme Forschung und Veröffentlichungen

- oft wird "Dijkstra" nebensächlich genannt, wenn man sich nur mit PQ auseinander setzt.
- das Ändern der Prio und speziell Finden eines Schlüssels, was im Dijkstra nötig ist,
  wird nicht im Detail behandelt.
- dass die Verwaltung der Daten bereits nicht in den Arbeitsspeicher passen, wird
  nicht behandelt/umgangen.
- oft theoretische Algorithmen Optimierung, welche auf Kosten des Speicherplatzes geht
- oft wird die Verlangsamung von "externen Datenstruktur" nur auf "Cache faults" reduziert
  und daher kein Fokus auf Dateien gelegt
- Mangelnde Behandlung/Transparenz der Mischung aus
  - viele Daten (externe Datenstrukturen nötig)
  - theoritischer Effizienz und praktischer Umsetzung
  - der Verbindung aus Dijkstra-Algorithmus und einer Priority Queue

Daher ist es schwer Veröffentlichungen zu finden, die diese Bereiche abdecken,
weil nur durch intensives lesen und testen erkennbar wird, welche der Bereiche
wirklich akzeptable behandelt werden, um einen Mehrwert daraus schöpfen
zu können.

# Überlegungen

Beispiele oft fragwürdig:

- speichern (wie in vielen solcher Beispielen) der Distanz
  nicht (nur) in der Priority Queue (PQ)
- Update der Distanz außerhalb der PQ
- extractMin() ohne weitere Details

Das die PQ sich neu organisiert, sobald sich der Wert hinter einem
Pointer auf die Distanz sich ändert, mag ja noch halbwegs realisierbar
sind, aber was, wenn der Pointer ungültig wird durch das Auslagern
der Daten in eine Datei? In dem Fall wird es schwer beide Daten
getrennt zu behandeln - da kann ich mir was ausdenken. (xxxxxxxxxxxxxxxx)

Alternativ kann man die Distanz einen Knotens natürlich in der PQ ablegen,
allerdings brauche ich dann eine PQ, die...

- schnell das Element X mit minimaler Prio liefert
- schnell das Element Y auffindet, um dessen Prio zu ändern

Was gut ist: die Y's sind die Nachbarn von X!

Für den Dijkstra auf große Datenmengen brauche ich:

- schnellen Zugriff auf Nachbarn eines gegebenen Knotens
- schnellen Zugriff auf die Distanzen zu den Nachbarn
- eine "Verarbeitungshaufen", in dem "angefasste" Knoten (Nachbarn) abgelegt sind
  und diese sortiert sind nach ihrer Nähe(Prio) zum Startknoten
- schnellen Zugriff auf einen (Nachbar)Knoten, um bei diesem die Nähe(Prio)
  zum Startknoten auslesen und ggf. anpassen zu können (inkl. eines neuen Parents/Successors)
- die Möglichkeit selten "angefasste" Daten in Dateien auslagern und bei
  bedarf wieder einlesen zu können


