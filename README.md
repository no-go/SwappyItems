# SwappyItems

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
- im Template können 3 Werte angegeben werden:
  - Anzahl der Items in einer Datei
  - Menge der Dateien, die geschrieben werden sollen, wenn der RAM voll ist (Swap Event)
  - Um ein wieviel-faches der RAM größer sein soll als die Anzahl der Items, die bei einem Swap Event ausgelagert werden

Beispiel: `SwappyItems<Key,Value,(16*1024),8,4> nodes;` bedeutet

- 16k Items sind in einer Datei
- 8x 16k Items werden bei einem Swap ausgelagert
- Der Swap wird ausgelöst, wenn 4x 8x 16k Items im Speicher überschritten werden.

Das Testprogramm `SwappyItems.cpp` gibt alle 2048 neue Items Auskunft:

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

## todo?

- iterator
- delete (does not work because of bloom filter)
- schnelle Dateisuche, da Daten in Datei sortiert sind
- Vergleich/Grafiken zur Performance

## Hacks für die Zukunft

- Prioritätsgruppen (nicht jeder Key, sondern ein "Bucket", was gefüllt wird)
- Buckets sind sorted maps, die in einem Vektor abgelegt sind
- welche Prio ein Bucket hat, wird in einem array abgelegt. Ein Head-Pointer (id im Array) zeigt auf das Aktuellste Bucket.
- eine Directory (unordered map) merkt sich min/max jedes Buckets
- alte Buckets werden in Dateien ausgelagert.

Man könnte auch über mehrerer SwappyItems nachdenken, zwischen denen man je key "switcht", um einen größeren Teil im RAM abzulegen.

