# Plagiarism Detection Engine
### Pure C (C11) Implementation • Arrays & Binary Search Trees • Multi-Format Extraction • Modern Web UI

[![Language: C11](https://img.shields.io/badge/Language-C11-00599C?style=flat-square&logo=c)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-0078D6?style=flat-square&logo=windows)](https://microsoft.com)
[![Compiler: GCC](https://img.shields.io/badge/Compiler-MinGW--w64%20GCC-green?style=flat-square)](https://www.mingw-w64.org/)
[![UI: Tailwind CSS](https://img.shields.io/badge/UI-Tailwind%20CSS-38B2AC?style=flat-square&logo=tailwind-css)](https://tailwindcss.com)
[![Data Structures](https://img.shields.io/badge/DSA-Arrays%20%26%20BST-orange?style=flat-square)](https://en.wikipedia.org/wiki/Binary_search_tree)

An academic-grade, high-performance **Plagiarism Detection Engine** written in pure C (C11 standard) demonstrating Data Structures & Complexity Analysis (DSA).

The application features a modern, responsive web interface (with dark/light theme support) served directly by a native C loopback HTTP server (`winsock2.h`). It demonstrates **Sequential Arrays**, **Binary Search Trees (BST)**, **Linguistic Stopword Filtering**, **Multi-Format Document Extraction (PDF, DOCX, TXT)**, and **Attributed Source Matching**.

---

## 1. Key Features

- **Pure C Architecture:** Written in standard C11 with zero external runtime dependencies (no Python, Node.js, or Java).
- **Embedded Loopback Web Server:** Native Winsock2 HTTP daemon listening on `127.0.0.1:8080` handling multipart file uploads and form submissions.
- **Modern Web Interface:**
  - Modern typography powered by Google Fonts (`Inter` & `JetBrains Mono`).
  - Icons powered by `Lucide Icons`.
  - Dark / Light mode toggle with persistent state.
  - Interactive file drag-and-drop dropzone supporting PDF, DOCX, and TXT files.
  - Live character and word counters with quick sample loaders.
  - Animated C11 BST progress feedback.
- **Academic Data Structures:**
  - Dynamic **Token and Sentence Arrays** (`TokenArray`, `SentenceArray`).
  - Self-sorting **Binary Search Tree** (`BSTNode`) for $O(\log N)$ vocabulary indexing and term frequency counting.
  - Interactive in-browser **C11 BST Node Traversal Inspector** displaying live tree metrics (depth, root, subtrees, and matched nodes).
- **Multi-Format Document Extraction:**
  - **PDF:** Decompresses PDF `/FlateDecode` content streams using `miniz` zlib inflation, extracting text while checking character positioning and kerning shifts (`TJ` spacing).
  - **DOCX:** Unzips Office Open XML archives to extract `word/document.xml` text runs and paragraph structures.
  - **TXT:** Direct UTF-8 and ASCII stream parsing with BOM detection.
- **Linguistic Stopword Filtering:** Filters high-frequency grammatical words (`the`, `is`, `and`, `on`, `also`) from plagiarism scoring while preserving full text for forensic display.
- **Multi-Factor Plagiarism Scoring:** Combines exact sequence sliding-window matching with BST lexical overlap search via `bst_search()`.

---

## 2. Project Layout

```text
plagiarism_detector/
├── frontend/
│   ├── frontend.c      # Native C Winsock2 HTTP server & Tailwind-styled UI rendering
│   └── frontend.h      # Server lifecycle and request routing API
├── backend/
│   ├── backend.c       # BST, Token Arrays, Stopwords, PDF/DOCX/TXT extraction & scoring
│   ├── backend.h       # Core data structures and analysis prototypes
│   ├── miniz.c         # C-based ZIP and FlateDecode decompression for DOCX and PDF
│   └── miniz.h         # Miniz header
├── samples/            # Test documents for validation
│   ├── Stage1_Microproject_Report.pdf
│   ├── Stage2_Microproject_Report.docx
│   ├── academic_dsa_sample.txt
│   └── original_clean_sample.txt
├── main.c              # Application entry point, CLI args, and signal handling
├── Makefile            # Standard GNU Makefile for MinGW-w64 GCC
├── build.bat           # 1-click Windows compilation script
├── .gitignore          # Repository ignore rules (binaries, objects, IDE artifacts)
└── README.md           # Documentation and DSA evaluation guide
```

---

## 3. Academic Data Structures & Complexity Analysis

### A. Sequential Token & Sentence Arrays (`TokenArray`, `SentenceArray`)
- **Structure:** Resizable dynamic array buffer storing word strings, start/end character offsets, sentence indices, and stopword flags.
- **Operations:**
  - `tokenize_document()`: Scans text in $O(L)$ linear time (where $L$ is text character length).
  - Sentence Segmentation: Splits on terminal punctuation (`.`, `!`, `?`, `\n`) while preserving character offsets.
  - Multi-Word Sliding Window: Analyzes sequence of tokens ($N \ge 3$) containing distinctive vocabulary.

### B. Dynamic Binary Search Tree (`BSTNode`)
- **Structure:** Dynamic tree node containing unique vocabulary string (`word`), term occurrence frequency (`frequency`), and child pointers (`left`, `right`).
- **Core Operations:**
  - `bst_insert()`: Inserts distinctive candidate vocabulary in lexicographical order.
  - `bst_search()`: Performs binary search for word presence in $O(\log N)$ average time.
  - `bst_count_nodes()`: Computes total unique vocabulary size in $O(N)$ time.
  - `bst_height()`: Recursively calculates tree depth to assess balance and recursion space.
  - `bst_free()`: Post-order recursive deallocation ensuring zero memory leaks.

### C. Algorithmic Complexity Comparison

| Operation | Average / Expected Time | Worst Case (Skewed) | Space Complexity |
| :--- | :---: | :---: | :---: |
| **Document Tokenization** | $O(L)$ | $O(L)$ | $O(L)$ |
| **BST Word Insertion** | $O(\log N)$ | $O(N)$ | $O(N)$ |
| **BST Word Search** | $O(\log N)$ | $O(N)$ | $O(1)$ auxiliary |
| **Linear Array Search** | $O(N)$ | $O(N)$ | $O(1)$ |
| **BST In-Order Traversal** | $O(N)$ | $O(N)$ | $O(H)$ recursion stack |

---

## 4. Multi-Factor Scoring Formula

Plagiarism detection combines exact phrase sliding window matching with BST lexical vocabulary search:

$$\text{Overall Similarity} = 0.65 \times \text{Exact Match \%} + 0.35 \times \text{BST Lexical \%}$$

- **Exact Match %:** Measures the sequence coverage of distinctive phrases ($N \ge 3$) matched against reference repositories.
- **BST Lexical %:** Evaluates distinctive reference terms found in the candidate document's Binary Search Tree using `bst_search()`.
- **Clean / Unrelated Content:** Naturally computes to **0.0% ("No Match")**, preventing false positives.

---

## 5. How to Build & Run on Windows

### Prerequisites
- GCC compiler ([MinGW-w64](https://www.mingw-w64.org/) or equivalent)

### Step 1: Compile the Project
Open PowerShell or Command Prompt in the project folder and run:
```powershell
.\build.bat
```
*Or using GNU Make:*
```powershell
mingw32-make
```

### Step 2: Start the Web Server
```powershell
.\plagiarism_detector.exe
```

The server will initialize and print:
```text
========================================================
   Plagiarism Detection Engine - SLA Stage 2
   Pure C Architecture | Arrays & Binary Search Trees
========================================================
Server started successfully.
Open: http://127.0.0.1:8080
Press Ctrl+C to stop server.
========================================================
```

### Step 3: Open in Browser
Open your browser and navigate to:
```text
http://127.0.0.1:8080
```

---

## 6. Testing with Sample Documents

The `samples/` directory includes sample files for instant evaluation:
- `samples/Stage1_Microproject_Report.pdf` (PDF format test)
- `samples/Stage2_Microproject_Report.docx` (DOCX format test)
- `samples/academic_dsa_sample.txt` (Verbatim DSA sample)
- `samples/original_clean_sample.txt` (Clean non-plagiarized sample)

---

## 7. License & Academic Attribution
Developed as part of the Second-Year B.Tech Data Structures and Algorithms (DSA) Microproject.  
Licensed under the [MIT License](LICENSE).
