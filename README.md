# Key Finder – Plugin VST3 per Ableton Live (Windows)

Analizza l'audio in ingresso in tempo reale (sia dal vivo, sia da una
traccia/clip riprodotta in Ableton) e stima la tonalità, mostrandola in
notazione **Camelot** (es. `8A`, `5B`) su una ruota interattiva, con nome
classico della tonalità, indicatore di confidenza e VU meter del livello
in ingresso.

Il plugin **non modifica l'audio**: è un analizzatore "in linea" da
mettere su una traccia (o sul master) come un normale effetto.

---

## 1. Requisiti (da installare una sola volta)

1. **Visual Studio 2022** (Community è gratuita) con il carico di lavoro
   *"Sviluppo di applicazioni desktop con C++"*
   https://visualstudio.microsoft.com/it/downloads/
2. **CMake** (versione 3.22 o superiore)
   https://cmake.org/download/ — durante l'installazione seleziona
   "Add CMake to system PATH".
3. **Git** (per scaricare automaticamente JUCE)
   https://git-scm.com/download/win

Non serve installare separatamente JUCE o il VST3 SDK: il file
`CMakeLists.txt` incluso li scarica da solo al primo build (serve
connessione internet solo in quel momento).

---

## 2. Compilazione

Apri **"Developer Command Prompt for VS 2022"** (cercalo nel menu Start)
e naviga nella cartella del progetto:

```bat
cd percorso\a\KeyFinderVST3
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

La prima volta il comando `cmake -B build` impiegherà qualche minuto:
sta scaricando JUCE da GitHub. I comandi successivi saranno molto più
rapidi.

Al termine della compilazione (`cmake --build`), il plugin VST3 verrà
copiato **automaticamente** nella cartella standard dei plugin VST3 di
Windows:

```
C:\Program Files\Common Files\VST3\Key Finder.vst3
```

(grazie all'opzione `COPY_PLUGIN_AFTER_BUILD TRUE` nel CMakeLists.txt).

Se preferisci compilare con l'interfaccia grafica di Visual Studio invece
che da riga di comando, apri il file generato in `build\KeyFinderVST3.sln`
e compila in configurazione **Release**.

---

## 3. Attivazione in Ableton Live

1. Apri Ableton Live.
2. Vai in **Preferenze → File/Cartelle** e assicurati che
   *"Usa cartella plugin VST3 aggiuntiva"* sia attivo, oppure che sia
   selezionata la cartella standard `C:\Program Files\Common Files\VST3`.
3. Clicca **"Ripristina lista plugin"** (Rescan) se il plugin non compare
   subito.
4. Nel browser di Ableton, sotto **Plug-in → VST3**, troverai **"Key
   Finder"**. Trascinalo su una traccia audio (o sul master) come un
   normale effetto/analizzatore.
5. Fai suonare l'audio (dal vivo tramite l'ingresso della traccia, oppure
   riproducendo una clip): la ruota Camelot evidenzierà la tonalità
   rilevata in tempo reale.

---

## 4. Come funziona l'algoritmo (in breve)

- L'audio viene analizzato a blocchi di 4096 campioni con **FFT** e
  finestratura di Hann.
- Lo spettro viene proiettato sui 12 semitoni (**chroma vector**),
  indipendentemente dall'ottava.
- Il chroma vector viene smussato nel tempo con una media mobile
  esponenziale, per evitare che la tonalità "salti" ad ogni nota.
- Viene calcolata la **correlazione di Pearson** tra il chroma vector e i
  24 profili tonali di **Krumhansl-Schmuckler** (12 maggiori + 12 minori),
  uno degli algoritmi più usati e validati in ambito MIR (Music
  Information Retrieval) per il key-finding.
- La tonalità con correlazione più alta è la stima corrente; il margine
  rispetto alla seconda ipotesi migliore diventa l'indicatore di
  **confidenza** mostrato nella UI.
- La tonalità stimata viene tradotta in notazione **Camelot** tramite una
  tabella di conversione standard (usata per il mixing armonico dai DJ).

---

## 5. Struttura del progetto

```
KeyFinderVST3/
├── CMakeLists.txt        # configurazione build (scarica JUCE automaticamente)
├── Source/
│   ├── PluginProcessor.h/.cpp   # motore audio del plugin (VST3)
│   ├── PluginEditor.h/.cpp      # interfaccia grafica (ruota Camelot, VU meter)
│   ├── KeyDetector.h/.cpp       # FFT, chroma vector, Krumhansl-Schmuckler
│   └── CamelotWheel.h/.cpp      # mappatura tonalità <-> notazione Camelot
└── README.md
```

---

## 6. Personalizzazioni comuni

- **Sensibilità/velocità di risposta**: nel file `KeyDetector.cpp`, la
  costante `alpha` (in `computeChromaFromMagnitudes`) controlla quanto
  velocemente la stima si adatta ai cambi di tonalità. Valori più alti =
  risposta più rapida ma meno stabile.
- **Range di frequenze analizzato**: `minFreq`/`maxFreq` in
  `computeChromaFromMagnitudes`.
- **Notazione classica al posto/insieme a Camelot**: il testo mostrato è
  già `CamelotCode (NomeClassico)` in `drawKeyLabel` (PluginEditor.cpp) —
  puoi cambiare l'ordine o mostrarne solo uno.

---

## Nota

Questo progetto è stato generato come codice sorgente completo e pronto
da compilare; non è stato compilato né testato dentro Ableton in questo
ambiente (che non ha accesso a una toolchain Windows/JUCE). Se durante
la compilazione o l'uso in Ableton incontri errori o comportamenti
inattesi, incollami il messaggio di errore e ti aiuto a risolverlo.
