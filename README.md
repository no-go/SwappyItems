# The SwappyItems Key-Values Store

The SwappyItems Store is a flexible c++17 multicore single disk key-value storage.
If the data in RAM gets large, it behaves similar to your system swap, but
with more parameters and it swaps only the data - not the program code!

Main difference to other key-value stores:

- Buckets with Bloom-filter instead of B*-tree or Log-structured merge-tree (LSM)
- posibility of persistance and breakpoints
- not RAM only, not DISK only
- very flexible in data: value is not just "String"
- not focused on multible disks or networking
- flat: no need of a fat framework
- no additional libs!

## API ...

### `constructor(id)`

It trys to wakeup from hibernate files. The id identifies the SwappyItems store.

### `destructor`

Makes a hibernate from ram to files.

### `set(key, value)`

### `set(key, value, vector<keys>)`

### `pair(value, vector<keys>) = get(key)`

### `del(key)`

### `getStatistic()`

### `each()`

- iterate through all elements at a fixed state
- with a lambda function it is similar to a "find"
- can change data via lambda
- gives a reference back to last processed data

### `apply()`

It is usable with a lambda function and similar to `get()` and `set()`.
It does not change the swap data and behavior and thus this access it a bit slow.
If you want to get or change data in the same SwappyItems store, where you
iter through with `each()`, then you must use `apply()` in the lambda
function of `each()`.


# may be Deprecated -------

the text that follows from here on is out of date and possibly incorrect or
does not reflect the current state of the project.


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


