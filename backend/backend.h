#ifndef BACKEND_H
#define BACKEND_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_WORD_LEN 64
#define MAX_URL_LEN 256
#define MAX_PASSAGE_LEN 512
#define MAX_STOPWORD_LIST 100

/* ========================================================================= */
/* 1. Academic Data Structure: Binary Search Tree (BST)                      */
/* ========================================================================= */

typedef struct BSTNode {
    char word[MAX_WORD_LEN];       /* Normalized unique vocabulary term */
    int frequency;                 /* Occurrence count in document corpus */
    struct BSTNode *left;          /* Pointer to left child subtree */
    struct BSTNode *right;         /* Pointer to right child subtree */
} BSTNode;

/* BST Operations */
BSTNode* bst_create_node(const char *word);
BSTNode* bst_insert(BSTNode *root, const char *word);
const BSTNode* bst_search(const BSTNode *root, const char *word);
int bst_count_nodes(const BSTNode *root);
int bst_height(const BSTNode *root);
void bst_free(BSTNode *root);

/* ========================================================================= */
/* 2. Academic Data Structure: Sequential Token and Sentence Arrays          */
/* ========================================================================= */

typedef struct {
    char original[MAX_WORD_LEN];   /* Original word with case preserved */
    char normalized[MAX_WORD_LEN]; /* Lowercase alphanumeric word */
    int start_char;                /* Starting character offset in text */
    int end_char;                  /* Ending character offset in text */
    int sentence_idx;              /* Sentence index in document */
    bool is_stopword;              /* Flag: high-frequency common word */
} Token;

typedef struct {
    Token *tokens;
    int count;
    int capacity;
} TokenArray;

typedef struct {
    int start_char;
    int end_char;
    int token_start_idx;
    int token_count;
    char *text;
} SentenceSpan;

typedef struct {
    SentenceSpan *sentences;
    int count;
    int capacity;
} SentenceArray;

TokenArray* token_array_create(int initial_capacity);
void token_array_append(TokenArray *arr, const Token *tok);
void token_array_free(TokenArray *arr);

SentenceArray* sentence_array_create(int initial_capacity);
void sentence_array_append(SentenceArray *arr, const SentenceSpan *span);
void sentence_array_free(SentenceArray *arr);

void tokenize_document(const char *text, TokenArray *out_tokens, SentenceArray *out_sentences);

/* ========================================================================= */
/* 3. Stopword Filtering & Normalization                                     */
/* ========================================================================= */

bool is_stopword(const char *word);
void normalize_word(const char *src, char *dst, size_t dst_size);

/* ========================================================================= */
/* 4. Document Extraction (PDF, DOCX, TXT)                                   */
/* ========================================================================= */

typedef enum {
    DOC_TYPE_UNKNOWN = 0,
    DOC_TYPE_TXT,
    DOC_TYPE_PDF,
    DOC_TYPE_DOCX
} DocumentType;

DocumentType detect_document_type(const char *filename);
char* backend_extract_text(const char *filename, const unsigned char *data, size_t size, char *err_buf, size_t err_size);

/* ========================================================================= */
/* 5. Plagiarism Analysis & Matching Records                                 */
/* ========================================================================= */

typedef struct {
    char match_type[32];           /* "Exact Phrase", "Lexical Overlap", "Online Match" */
    char candidate_passage[MAX_PASSAGE_LEN];
    char source_passage[MAX_PASSAGE_LEN];
    char source_title[128];
    char source_url[MAX_URL_LEN];
    double confidence;             /* 0.0 to 100.0 */
} MatchRecord;

typedef struct {
    char document_name[128];
    char document_type[16];
    char *extracted_text;
    int total_words;
    int total_chars;
    int distinct_words;
    
    /* Academic BST metrics */
    int bst_nodes;
    int bst_height;
    
    /* Similarity Results */
    double overall_similarity;     /* Aggregated similarity percentage */
    double exact_similarity;       /* Exact phrase match percentage */
    double lexical_similarity;     /* Distinctive vocabulary overlap */
    char verdict[32];              /* "No Match", "Low", "Moderate", "High", "Very High" */
    
    /* Evidence & Attribution */
    MatchRecord *matches;
    int match_count;
    int match_capacity;
    
    /* Excluded Common Words (Stopwords) */
    char excluded_stopwords[MAX_STOPWORD_LIST][MAX_WORD_LEN];
    int excluded_stopwords_count;
    
    /* Status & Online Search Info */
    char status_message[256];
    bool search_performed;
} PlagiarismReport;

/* Subsystem Lifecycle */
void backend_init(void);
void backend_cleanup(void);

/* Core Analysis Function */
PlagiarismReport* backend_analyze(const char *doc_name, const char *text);
void backend_free_report(PlagiarismReport *report);

/* Web & Security Functions */
bool is_safe_public_url(const char *url);
char* fetch_webpage_text(const char *url, size_t max_bytes);

#endif /* BACKEND_H */
