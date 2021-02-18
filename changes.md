
# 2020-03-14

## Docker

how to get, build and run v0.78 :

    mkdir data
    place a pbf file in that data directory
    git clone https://github.com/no-go/SwappyItems.git app
    cd app
    docker build -t swappyitems .
    docker run -ti -v $(pwd)/../data:/data -v $(pwd):/app swappyitems bash
    make
    ./SwappyItems.exe /data/krefeld_mg.pbf

vorsicht. Die SwappyItems.cpp ist ab dem 15.02.2020 verkleinert und angepasst worden, um sie
mit STxxL besser vergleichen zu können.

## SwappyItems

- c++17 nur für das initale anlegen von Ordern gebraucht (sonst c++11)
- ein Key-Value Store
- der Key muss eindeutig sein und darf nicht 0 sein
- ein `get()` liefert nullptr, falls der Key nicht existiert
- ein `set()` legt an (return true) oder überschreibt
- ein `delete()` gibt es nicht!
- man muss bei Programmende die ausgelagerten Dateien selber löschen!
- sollte der RAM zu voll werden, werden alte Key-Value Paare in Dateien geschrieben
- jeder `get()` und `set()` Aufruf bedeutet, diese Daten sind "frisch"
- die Daten sind in den Dateien sortiert nach Key abgelegt, um die Suche eines Keys darin zu beschleunigen (NICHT IMPLEMENTIERT)
- durch das Laden einer Datei wird diese frei und kann erneut genutzt werden
- vor dem Laden einer Datei werden ggf. alte Daten in Dateien ausgelagert, um im RAM Platz zum Laden zu schaffen.
- Dateien werden nur in den RAM geladen, wenn ein gesuchter Key darin gefunden wurde
- das Durchsuchen einer Datei nach einem Key braucht seinen eigenen RAM
- ein bloom filter ist improvisiert, um vorab erkennen zu können, ob ein key neu ist
- vektor zu jeder datei, der dessen min und max key enthält, um dateien beim durchsuchen nach einem key ausschließen zu können
- Daten im RAM ist unordered_map
- es wird eine map beim auslagern genutzt, um sich das Sortieren zu ersparen
- ein deque merkt sich, wie neu die daten sind bzw. deren zugriff ist
- für einen schnellen speedup der deque, werden keys dort nicht gelöscht (bzw. ans ende verschoben), sondern mit key=0 als "gelöscht" markiert
- durch das "gelöscht" markieren darf ein **key nicht 0 sein** !!!!
- im Template können 3 Werte angegeben werden (neben den Typen):
  - Anzahl der Items in einer Datei
  - Menge der Dateien, die geschrieben werden sollen, wenn der RAM voll ist (Swap Event)
  - Um ein wieviel-faches der RAM größer sein soll als die Anzahl der Items, die bei einem Swap Event ausgelagert werden

Beispiel: `SwappyItems<Key,Value,(16*1024),8,4> nodes;` bedeutet

- 16k Items sind in einer Datei
- 8x 16k Items werden bei einem Swap ausgelagert
- Der Swap wird ausgelöst, wenn 4x 8x 16k Items im Speicher überschritten werden.
- vorsicht! new verwenden! der Constructor bekommt eine Nummer, die als prefix für ausgelagerte Dateien dient!

Das Testprogramm `SwappyItems.cpp` (v0.78) gibt folgende Auskunft, wenn die "size" durch 2048 teilbar ist:

- zeitpunkt
- anzahl der hinzugefügten Items
- anzahl der Items im RAM
- anzahl der Key in der Prioity Deque (größer, da nur Löschmarkierung)
- anzahl der Zugriffe, die kein "Hinzufügen" sind
- wie oft der Bloomfilter sagen konnte, dass ein Key neu ist
- wie oft der Bloomfilter falsch lag, wenn er meinte, der Key sei schon mal hinzugefügt worden
- wie oft das Speichern der min/max dafür sorgte, dass eine Datei nicht durchsucht werden muss
- wie oft trotz min/max am Ende der Key doch nicht in der Datei war
- wie oft ein Key-Value Paar durch das Laden einer Datei in den RAM geholt werden muss
- der Speicherverbrauch des Programms

Das Testprogramm ließt eine pbf Datei (Open Street Map) und verarbeitet/ließt Knoten von Wegen.

### Bloom filter und mehr

Um initial mit vielen neu hinzugefügten Keys zurecht zu kommen, muss es schnell
erkennbar sein, ob ein Key schon mal da war, aber evtl. durch das Auslagern in eine
Datei nicht im RAM (*unordered_map*) zu finden ist. Mit einem Bloom Filter habe ich
daher einen Fingerabdruck je Key in unterschiedlichen Datenstrukturen hinterlassen,
um sicher sagen zu können, ob ein Key neu ist.

Der verwendete Bloom-Filter ist sehr primitiv und aktuell nur aus einer
Intuition heraus "gut" konvektioniert.
Ein Key wird als Seed des C-Zufallsgenerators genommen. Um nun zu einem Key 4 Bits
zu bekommen, die gesetzt werden sollen, wird der Zufallsgenerator 4 mal aufgerufen.
Durch Modulo und diesen Pseudozufallsgenerator erhalte ich 4 Werte aus einem definierten
Wertebereich.

Anstelle des Setzens dieser Werte in einem sehr langen Bit-Vektor (was wäre hier optimal?)
entschied ich mich für folgendes:

Der erste Wert geht von 0 bis "Anzahl der Items, die bei einem Swap ausgelagert werden".
Dieser Wert wird in einem *Set* abgelegt. Ist bei einem Key dieser Wert nicht vorhanden,
ist der Key neu. Ansonsten:

Der zweite Wert geht von 0 bis "Anzahl der Items, die bei einem Swap ausgelagert werden".
Dieser Wert dient als Schlüssel in einer *unordered_map*.  Ist dieser Schlüssel nicht
vorhanden, ist der Key offensichtlich neu. Ansonsten:

Der 3. und 4. Wert geht von 0 bis "Anzahl der Items je Datei geteilt durch 4". Sind diese
beiden Werte nicht in dem *Set* der o.g. *unordered_map* an Schlüsselposition zu finden,
dann ist der Key neu. Ansonsten:

Es wird in den *Ranges* die Dateien herausgesucht, wo der Key enthalten sein
kann (min < key < max). Sollte jedoch keine Datei in Frage kommen, ist der Key neu. Ansonsten:

Durchsuche jede in Frage kommende Datei, bis der Key gefunden ist. Ansonsten ist der Key neu. 

## todo?

- schnelle Dateisuche, da Daten in Datei sortiert sind
- Speed compare (System Swap, STxxL, Badger from dgraph.io, Redis on flash)

## Hacks für die Zukunft

- Prioritätsgruppen (nicht jeder Key, sondern ein "Bucket", was gefüllt wird)
- Buckets als sorted maps, die in einem Vektor abgelegt sind
- welche Prio ein Bucket hat, wird in einem array abgelegt. Ein Head-Pointer (id im Array) zeigt auf das Aktuellste Bucket.
- eine Directory (unordered map) merkt sich min/max jedes Buckets
- alte Buckets werden in Dateien ausgelagert.

- über nicht buffered access oder multi thread nachdenken
- kann ich b+-Baum für meine Zwecke bei der Dateisuche nutzen?
- wie kann ich ohne dateisystem oder mit XFS arbeiten?

Mehrere Swappy Items (Constructor Variable)

- zwischen denen könnte man je key "switcht", um einen größeren Teil im RAM abzulegen.

# 2020-03-29

## gegeben ist...

i)   eine Datenmenge, die 100x größer ist, als der Arbeitsspeicher eines Rechners.
ii)  die Dateneinträge sind mit einem 64Bit Schlüssel identifizierbar.
iii) ein Multicore-Rechner mit SSD oder HDD ohne Netzwerkschnittstelle.

## gesucht ist...

1.1) eine Graphstruktur, die sich aus ca 1/5 der Datenmenge ergibt.
1.2) eine effiziente Speicherung von Kantengewicht, Abbiegebedingungen,
     Kantenrichtung, Kantenname und ggf. Abbiegekosten
2) ein kürzester Weg durch diese Graphstruktur zwischen 2 ausgewählten Knoten.
3) eine schnelle Verarbeitungsform und effiziente Speicherung, um (2)
   für beliebige neue Knoten zu ermitteln.
   
## problematisch ist...

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

## Lösungsansätze und Herausforderungen

### Flat, Sorted, Bloomfilters, Most Recently Used

- nutze ich ....
- ...

Most Recently Used:

- keine Statistik nötig (nur eine Liste als "Zeitstempel")
- Blick auf einen aktuell wichtigen "Working-Set"

Most Frequently Used: 

- wichtig bei Zugriffen, die sich auf einige wenige Daten konzentrieren
- es fehlt die Dynamik einer Alterung (ggf. nachrüstbar)

### Looser/Tournament Tree

- gut verwendbar für min/max Heaps (= priority Queue)
- minimiert vergleiche durch vorhalten von Teilinfos
- Daten sind sortieren Sequenzen hinterlegt (runs/merges/k-merge)
- Prozesse lassen sich Puffern, um Zeit für ext. Datenstrukturen zu gewinnen

### Sequence Heaps

- gut für priority Queues mit externen Datenstrukturen
- Mischung aus Gruppen (Buckets?), Vorsortierungen und Tournament Tree

### Log-structured merge-tree (LSM)

- vermutlich identisch mit "Sequence Heaps" (?)
- viele Systeme für große Datenmengen nutzen das

### 2-level Bucket Heap

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

## Probleme Forschung und Veröffentlichungen

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

## Überlegungen

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

# 2020-08-04

Ich habe nun nach 4 Monaten nochmal in die Map und Routing Sachen
reingeschaut. Das war ganz gut, da mir so klar wurde, wo ich schlecht
bzw. nicht gut nachvollziehbar programmiert und dokumenntiert habe.

Ich habe ca ende März unbewusst alles als gescheitert abgestempelt,
sonst hätte ich es ja so lange nicht liegen lassen. Dann kam
Corona-Blues und viel Unzufriedenheit mit dem Job dazu.

An folgenden Punkten scheiterte ich damals:

- Mangel an "Grifffestigkeit" des Themas und der Motivation der
  möglichen Masterarbeit
- zu viele kleine und große Baustellen bei dem Themenkomplex
- nichts, wo ich 100% hinter stehe und gut erklähren könnte, warum
  mich Herr Wanke betreuen sollte
  
Konkret:

- ich scheitere bei der Begründung (im Detail!), warum man das Caching
  der großen Datenmengen nicht dem Betriebsystem überlassen sollte
- ich scheitere an der Komplexität der OpenStreetMap-Daten, um diese
  im Umfang "europa" sinvoll zu reduzieren.
- hatte mit über einem Tag rechnen (und nahe meinem Hardware-Limit)
  bisher nur NRW verarbeiten können = ein gerichteter Graph mit
  Positionen, Abständen aber ohne(!!) Abbiegebedingungen und daher
  sinnlos
- trotz viel Aufwand in ein eigenes Caching und Ext. Speicherstruktur
  ist der "Reduzierungsaufwand" zeitlich weiterhin extrem hoch
- trotz einer umfangreichen Literaturrecherche findet sich keine
  brauchbare Beschreibung einer Ext. Priority Queue, die einen Teil
  im Speicher hat oder mit großen Datenmengen arbeitet und gleichzeitig
  eine Anpassung der Priorität bietet

Alles das ist weit weit weit von dem entfernt, wo du sagtest: Da ließt
man sich jeweils in 1-2 Paper ein und am Ende will ich einen Vergleich
und Implementierungen von den Datenstrukturen und Algorithemen dieser
Paper haben :-S

Bisher laufen alle Sachen, die ich in der Literatur gefunden habe, auf
diese Punkte und Extreme hinaus:

- um einen Graphen zu haben, dessen Datenmenge eine Ext. Priority Queue
  beim Routing rechtfertigt, muss ich schon Deutschland oder Europa aus
  Roh-Daten einlesen, um daraus einen entsprechend großen Graphen zu
  bilden. 
- um Deutschland oder Europa als Roh-Daten zu einem Graphen verarbeiten
  zu können, muss ich derzeit noch einen enormen Hardware- und
  Zeitaufwand betreiben. In dem Fall habe ich (oder die
  wissenschaftliche Veröffentlichung, die ich laß) bereits genug
  Hardware, so dass eine Ext. Priority Queue nicht nötig ist, sollte
  man hinterher Routing machen müssen
- Die wissenschaftlichen Veröffentlichungen zum Routing, die ich laß,
  haben alle mit bereits reduzierten roh-Daten gearbeitet um zum Teil
  beschrieben, wie sich diese noch weiter reduzieren lassen, dass keine
  Ext. Datenstrukturen bemüht werden müssen. Dies jedoch zum Leidwesen
  der Brauchbarkeit: man hat dann zwar einen minimalen Spannbaum, jedoch
  ist dieser nicht mehr mit einer realen Karte in Verbindung zu bringen
  und Abbiege-Regeln oder Tempo-Limits fehlen.
- Die wissenschaftlichen Veröffentlichungen zu Priority Queues, die ich
  laß, haben entweder keinen Fokus auf Bigdata und Externe
  Datenstrukturen oder beschreiben fürs Routing unbrauchbare Lösungen,
  in denen man die Priorität eines bestimmten Datensatzes (wahlfreier
  Zugriff) nicht ändern kann. 

In meinen damals letzten Überlegungen stolperte ich über folgendes:

- warum nicht 2 sortierte Listen nehmen:
  - sortiert nach Priority und je Priority nach Key
  - sortiert nach Key und darin abgelegt die Priority + Daten
- wie lege ich das im RAM ab, um nicht oft zu springen?
- wie füge ich eine Sortierung hinzu, so dass alte Daten (selten
  genutzte Keys) extern gespeichert werden?
- Wie manchen es andere? Antwort: Sobald "Bigdata" im Spiel ist, beginnt
  die Framework- und vernetzte Hardware-Schlacht!

Im Grunde finde ich die Hürden an die nötige Datenstruktur(en) zu groß:

- zur Verarbeitung einer Datenmenge, wo bereits die Anzahl der
  Primärschlüssel nicht in den Arbeitsspeicher passt
- eine Datenstruktur, die die Daten von seltenen Key-Zugriffen extern
  auslagert
- eine Datenstruktur, die nach einer Priorität sortiert ist
- eine Datenstruktur, die niedrige Prioritäten extern auslagert
- eine Datenstruktur, bei der man wahlfreien Zugriff hat, um eine
  Priorität ändern zu können
- eine Datenstruktur, die große Daten auf einem Rechner verarbeiten
  kann (z.B. SSD 100x größer als der RAM und Daten ca 1/10 der SSD)
- eine Datenstruktur, die optimal die 2D Eigenschaft/Nähe nutzen um
  ggf. Prefetching der Daten zu machen oder diese optimal im RAM
  platziert für nur wenige Sprünge (ggf. Cache-Effekte nutzen)

Daher hab ich wohl Ende März innerlich kapituliert!

Aktuell lese ich mich jedoch nochmal in den Code ein, um:

- am Beispiel "Krefeld" die Abbiege-Bedingungen zu verarbeiten
- meine letzten Ideen bzgl. einer Ext. Prio Queue nochmal
  nachvollziehen und zu ende bringen zu können

Zwischendurch hatte ich die Idee, man könne doch man krefeld einlesen
und aus Krefeld eine "brauchbare" Einbahnstraßen-Struktur machen.
Die Idee dabei ist, dass aus der 2. Spur ein Radweg wird. Ziel ist
es, 50% der Straßen zu Radwegen zu machen und daraus dann wieder
eine neue OpenStreetMap-Karte zu generieren. Allerdings sollte die
Karte so erzeugt werden, dass Ampeln reduziert werden können (mehr
Kreisverkehre oder Regionen/Geraden, wo Autos nicht Kreuzen) und
Autos weiterhin mit wenig Umwegen nah ans Ziel kommen.

# 2021-02-19

Über Externe Datenstrukturen und Priority Queues habe ich folgendes
zusammenfassend rausgefunden:

- besser two phase pairing Heaps nehmen statt Fibunacci Heaps
- sich mit winner trees auseinander setzen (und dessen Mergeing)
- Vorsicht bei (Pre-)Veröffentlichungen, wo als Wichtigkeit "Dijkstra" erwähnt
  wird, jedoch in der vorgestellten Priority Queue keine Methode zum Ändern
  der Priorität vorgestellt wird
- viele Frameworks sind: RAM only, DISK only oder bauen auf ein Netz von Rechnern
- manche Frameworks sind speziell bei der Priority Queues schnarchend langsam
- Argumentation: Wie ist zu belegen oder zu begründen, dass man nicht dem
  Betriebssystem das "Swappen" auf die SSD überlassen sollte?
- Wahlfreier Zugriff: Viele theoretische Betrachtungen umgehen das physikalische
  Problem, dass man bei großen Datenmengen eben NICHT O(1) Zeit braucht, um
  im Speicher(medium) mit einem Key auf einen Value zuzugreifen. Speziell das
  wiederfinden eines Elements, bei dem man die Priorität anpassen will, wird
  oft nicht oder sehr theoretisch und banal übergangen.
- RAM only Dijkstra in Java mit OSM: https://www.graphhopper.com/developers/
- Eine Bucket-based priority queue ist noch interessant. Jede Priorität hat ihren
  eigenen Eimer, in dem die Daten mit selben Priorität liegen. Auch diese
  Buckets könnte man im Heap anordnen. Das Verhältnis, mit wie vielen Buckets
  und Daten man es zu tun hat, ist interessant. Wann macht welche Strucktur
  sinn?

Aufgrund der Unzulänglichkeit mancher Frameworks habe ich selbst einen Key-Value Speicher
entwickelt, der meist benutzte Daten in std::unordered_map ablegt und altes
in Dateien auslagert. Für die "Alterung" nutze ich eine std::deque, bei der
ich benutzte Keys oder deren Löschung ans Ende schreibe und vorne die Elemente
nutze, um "altes" zu identifizieren. Nach Anwachsen der std::unordered_map
wird ein Teil in sortierter Form in mehrere Dateien geschrieben und so
ausgelagert. Durch Speichern von min/max Werten sowie einer Bitmaske (Bloomfilter)
kann ich leicht angeforderte Keys in Dateien wiederfinden und wieder laden.
Bisher habe ich in dem Code keine bedenklichen Fehler gefunden. Das gilt leider
nicht für den darauf aufbauenden two phase pairing Heap:

https://github.com/no-go/SwappyItems/blob/master/src/SwappyQueue.hpp

Im two phase pairing Heap wird oft von Leftmost und Siblings gesprochen,
und dass kein Parent zum höheren Knoten existiert, was alles erschwert.
Das war aus meiner Sicht unsinn. "leftmost" ist in allem, was ich
fand, ein ganz normaler Sibling und es gibt auch keinen Zwang, die 
Siblings zu sortieren. Ein Parent mach daher anstelle eines 
"Leftmost" mehr Sinn, da ich so bei einem wahlfreien Zugriff
direkt auf den übgeordneten Knoten und alle dessen Siblings zugreifen kann
zwecks Aufbau eines neuen Heaps. Viel habe ich auf delete() ausgelagert, denn
das Ändern einer Priorität ist für mich nichts anderes als ein delete() und
ein insert(). Mehr ins Detail für mehr Effizienz wollte ich nicht gehen, da
eine Version ohne mein "SwappyItems" tadellos tat:

https://github.com/no-go/SwappyItems/blob/4f4c180e5c4f371a40356fddb5c135a7f0c72b87/examples/TwoPhasePairPrioQ/TwoPhasePairingHeap.hpp

Daher ist das Projekt zur "Verheiratung" von SwappyItems mit dem Heap
erstmal auf Eis gelegt.

Hier die Doku zu "SwappyItems": https://no-go.github.io/SwappyItems/

Hier Details zu dem Template Variablen von SwappyItems:

https://no-go.github.io/SwappyItems/classSwappyItems.html#details

Ich habe mir bei Beispielen und Dokumentation sehr viel mühe gemacht und auch
versucht sehr viele neue C++ Sachen beispielhaft einzubinden. So könnte es
auch eine gute Referenz zu anderen, neuen Projekten sein.
