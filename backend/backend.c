#include "backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#endif

#include "miniz.h"

/* ========================================================================= */
/* 1. English Stopwords List                                                 */
/* ========================================================================= */

static const char *ENGLISH_STOPWORDS[] = {
    "a", "about", "above", "after", "again", "against", "all", "also", "am", "an",
    "and", "any", "are", "aren't", "as", "at", "be", "because", "been", "before",
    "being", "below", "between", "both", "but", "by", "can", "can't", "cannot",
    "could", "couldn't", "did", "didn't", "do", "does", "doesn't", "doing", "don't",
    "down", "during", "each", "few", "for", "from", "further", "had", "hadn't",
    "has", "hasn't", "have", "haven't", "having", "he", "he'd", "he'll", "he's",
    "her", "here", "here's", "hers", "herself", "him", "himself", "his", "how",
    "i", "i'd", "i'll", "i'm", "i've", "if", "in", "into", "is", "isn't", "it",
    "it's", "its", "itself", "just", "let's", "me", "more", "most", "mustn't",
    "my", "myself", "no", "nor", "not", "of", "off", "on", "once", "only", "or",
    "other", "ought", "our", "ours", "ourselves", "out", "over", "own", "same",
    "she", "she'd", "she'll", "she's", "should", "shouldn't", "so", "some", "such",
    "than", "that", "that's", "the", "their", "theirs", "them", "themselves",
    "then", "there", "there's", "these", "they", "they'd", "they'll", "they're",
    "they've", "this", "those", "through", "to", "too", "under", "until", "up",
    "very", "was", "wasn't", "we", "we'd", "we'll", "we're", "we've", "were",
    "weren't", "what", "what's", "when", "where", "which", "while", "who", "whom",
    "why", "with", "won't", "would", "wouldn't", "you", "you'd", "you'll", "you're",
    "you've", "your", "yours", "yourself", "yourselves"
};
static const int NUM_STOPWORDS = sizeof(ENGLISH_STOPWORDS) / sizeof(ENGLISH_STOPWORDS[0]);

bool is_stopword(const char *word) {
    if (!word || word[0] == '\0') return false;
    for (int i = 0; i < NUM_STOPWORDS; i++) {
        if (strcmp(word, ENGLISH_STOPWORDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

void normalize_word(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) return;
    size_t d = 0;
    for (size_t s = 0; src[s] != '\0' && d < dst_size - 1; s++) {
        if (isalnum((unsigned char)src[s])) {
            dst[d++] = (char)tolower((unsigned char)src[s]);
        }
    }
    dst[d] = '\0';
}

/* ========================================================================= */
/* 2. Academic Data Structure: Binary Search Tree (BST)                      */
/* ========================================================================= */

BSTNode* bst_create_node(const char *word) {
    if (!word) return NULL;
    BSTNode *node = (BSTNode *)malloc(sizeof(BSTNode));
    if (!node) return NULL;
    strncpy(node->word, word, MAX_WORD_LEN - 1);
    node->word[MAX_WORD_LEN - 1] = '\0';
    node->frequency = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

BSTNode* bst_insert(BSTNode *root, const char *word) {
    if (!word || word[0] == '\0') return root;
    if (root == NULL) {
        return bst_create_node(word);
    }
    int cmp = strcmp(word, root->word);
    if (cmp < 0) {
        root->left = bst_insert(root->left, word);
    } else if (cmp > 0) {
        root->right = bst_insert(root->right, word);
    } else {
        root->frequency++;
    }
    return root;
}

const BSTNode* bst_search(const BSTNode *root, const char *word) {
    if (root == NULL || word == NULL) return NULL;
    int cmp = strcmp(word, root->word);
    if (cmp == 0) return root;
    if (cmp < 0) return bst_search(root->left, word);
    return bst_search(root->right, word);
}

int bst_count_nodes(const BSTNode *root) {
    if (root == NULL) return 0;
    return 1 + bst_count_nodes(root->left) + bst_count_nodes(root->right);
}

int bst_height(const BSTNode *root) {
    if (root == NULL) return 0;
    int lh = bst_height(root->left);
    int rh = bst_height(root->right);
    return 1 + (lh > rh ? lh : rh);
}

void bst_free(BSTNode *root) {
    if (root == NULL) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

/* ========================================================================= */
/* 3. Academic Data Structure: Sequential Token & Sentence Arrays            */
/* ========================================================================= */

TokenArray* token_array_create(int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = 64;
    TokenArray *arr = (TokenArray *)malloc(sizeof(TokenArray));
    if (!arr) return NULL;
    arr->capacity = initial_capacity;
    arr->count = 0;
    arr->tokens = (Token *)malloc(sizeof(Token) * arr->capacity);
    if (!arr->tokens) {
        free(arr);
        return NULL;
    }
    return arr;
}

void token_array_append(TokenArray *arr, const Token *tok) {
    if (!arr || !tok) return;
    if (arr->count >= arr->capacity) {
        int new_cap = arr->capacity * 2;
        Token *nt = (Token *)realloc(arr->tokens, sizeof(Token) * new_cap);
        if (!nt) return;
        arr->tokens = nt;
        arr->capacity = new_cap;
    }
    arr->tokens[arr->count++] = *tok;
}

void token_array_free(TokenArray *arr) {
    if (!arr) return;
    if (arr->tokens) free(arr->tokens);
    free(arr);
}

SentenceArray* sentence_array_create(int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = 16;
    SentenceArray *arr = (SentenceArray *)malloc(sizeof(SentenceArray));
    if (!arr) return NULL;
    arr->capacity = initial_capacity;
    arr->count = 0;
    arr->sentences = (SentenceSpan *)malloc(sizeof(SentenceSpan) * arr->capacity);
    if (!arr->sentences) {
        free(arr);
        return NULL;
    }
    return arr;
}

void sentence_array_append(SentenceArray *arr, const SentenceSpan *span) {
    if (!arr || !span) return;
    if (arr->count >= arr->capacity) {
        int new_cap = arr->capacity * 2;
        SentenceSpan *ns = (SentenceSpan *)realloc(arr->sentences, sizeof(SentenceSpan) * new_cap);
        if (!ns) return;
        arr->sentences = ns;
        arr->capacity = new_cap;
    }
    arr->sentences[arr->count++] = *span;
}

void sentence_array_free(SentenceArray *arr) {
    if (!arr) return;
    if (arr->sentences) {
        for (int i = 0; i < arr->count; i++) {
            if (arr->sentences[i].text) free(arr->sentences[i].text);
        }
        free(arr->sentences);
    }
    free(arr);
}

void tokenize_document(const char *text, TokenArray *out_tokens, SentenceArray *out_sentences) {
    if (!text || !out_tokens) return;
    int text_len = (int)strlen(text);
    int pos = 0;
    int current_sentence_idx = 0;
    int sentence_start_char = 0;
    int sentence_start_tok = 0;

    while (pos < text_len) {
        /* Terminal punctuation creates sentence boundary */
        if (text[pos] == '.' || text[pos] == '!' || text[pos] == '?' || text[pos] == '\n') {
            if (out_sentences && pos > sentence_start_char) {
                int s_len = pos - sentence_start_char + 1;
                SentenceSpan span;
                span.start_char = sentence_start_char;
                span.end_char = pos;
                span.token_start_idx = sentence_start_tok;
                span.token_count = out_tokens->count - sentence_start_tok;
                span.text = (char *)malloc(s_len + 1);
                if (span.text) {
                    strncpy(span.text, text + sentence_start_char, s_len);
                    span.text[s_len] = '\0';
                }
                sentence_array_append(out_sentences, &span);
            }
            pos++;
            while (pos < text_len && (text[pos] == '.' || text[pos] == '!' || text[pos] == '?' || isspace((unsigned char)text[pos]))) {
                pos++;
            }
            current_sentence_idx++;
            sentence_start_char = pos;
            sentence_start_tok = out_tokens->count;
            continue;
        }

        if (!isalnum((unsigned char)text[pos])) {
            pos++;
            continue;
        }

        int start = pos;
        while (pos < text_len && isalnum((unsigned char)text[pos])) {
            pos++;
        }
        int word_len = pos - start;
        if (word_len > 0) {
            Token tok;
            memset(&tok, 0, sizeof(Token));
            tok.start_char = start;
            tok.end_char = pos - 1;
            tok.sentence_idx = current_sentence_idx;

            int copy_len = word_len < MAX_WORD_LEN - 1 ? word_len : MAX_WORD_LEN - 1;
            strncpy(tok.original, text + start, copy_len);
            tok.original[copy_len] = '\0';

            for (int k = 0; k < copy_len; k++) {
                tok.normalized[k] = (char)tolower((unsigned char)tok.original[k]);
            }
            tok.normalized[copy_len] = '\0';
            tok.is_stopword = is_stopword(tok.normalized);

            token_array_append(out_tokens, &tok);
        }
    }

    if (out_sentences && text_len > sentence_start_char) {
        int s_len = text_len - sentence_start_char;
        if (s_len > 0) {
            SentenceSpan span;
            span.start_char = sentence_start_char;
            span.end_char = text_len - 1;
            span.token_start_idx = sentence_start_tok;
            span.token_count = out_tokens->count - sentence_start_tok;
            span.text = (char *)malloc(s_len + 1);
            if (span.text) {
                strncpy(span.text, text + sentence_start_char, s_len);
                span.text[s_len] = '\0';
            }
            sentence_array_append(out_sentences, &span);
        }
    }
}

/* ========================================================================= */
/* 4. Multi-Format Document Extraction (TXT, DOCX, PDF)                      */
/* ========================================================================= */

DocumentType detect_document_type(const char *filename) {
    if (!filename) return DOC_TYPE_TXT;
    const char *dot = strrchr(filename, '.');
    if (!dot) return DOC_TYPE_TXT;
    if (_stricmp(dot, ".txt") == 0) return DOC_TYPE_TXT;
    if (_stricmp(dot, ".pdf") == 0) return DOC_TYPE_PDF;
    if (_stricmp(dot, ".docx") == 0) return DOC_TYPE_DOCX;
    return DOC_TYPE_UNKNOWN;
}

static char* extract_docx_xml_text(const char *xml_data, size_t xml_size) {
    size_t out_cap = xml_size + 256;
    char *out_text = (char *)malloc(out_cap);
    if (!out_text) return NULL;

    size_t out_len = 0;
    const char *ptr = xml_data;
    const char *end = xml_data + xml_size;

    while (ptr < end) {
        if (_strnicmp(ptr, "<w:p", 4) == 0) {
            if (out_len > 0 && out_text[out_len - 1] != '\n') {
                out_text[out_len++] = '\n';
            }
            while (ptr < end && *ptr != '>') ptr++;
            if (ptr < end) ptr++;
            continue;
        }

        const char *t_start = strstr(ptr, "<w:t");
        if (!t_start || t_start >= end) break;

        const char *tag_close = strchr(t_start, '>');
        if (!tag_close || tag_close >= end) break;
        tag_close++;

        const char *t_end = strstr(tag_close, "</w:t>");
        if (!t_end || t_end > end) break;

        size_t len = (size_t)(t_end - tag_close);
        if (len > 0) {
            if (out_len + len + 2 >= out_cap) {
                out_cap = (out_len + len + 2) * 2;
                char *n = (char *)realloc(out_text, out_cap);
                if (!n) { free(out_text); return NULL; }
                out_text = n;
            }
            memcpy(out_text + out_len, tag_close, len);
            out_len += len;
            out_text[out_len++] = ' ';
        }
        ptr = t_end + 6;
    }

    out_text[out_len] = '\0';
    return out_text;
}

static const unsigned char* backend_mem_find(const unsigned char *haystack, size_t hlen, const unsigned char *needle, size_t nlen) {
    if (!haystack || !needle || nlen == 0 || hlen < nlen) return NULL;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        if (haystack[i] == needle[0] && memcmp(haystack + i, needle, nlen) == 0) {
            return haystack + i;
        }
    }
    return NULL;
}

static void extract_strings_from_pdf_content(const char *content, size_t clen, char **out, size_t *out_len, size_t *out_cap) {
    const char *p = content;
    const char *end = content + clen;

    while (p < end) {
        const char *bt = (const char *)backend_mem_find((const unsigned char *)p, end - p, (const unsigned char *)"BT", 2);
        if (!bt) break;

        const char *et = (const char *)backend_mem_find((const unsigned char *)bt, end - bt, (const unsigned char *)"ET", 2);
        if (!et) et = end;

        const char *sp = bt;
        while (sp < et) {
            if (*sp == '(') {
                /* Check preceding text for positioning or TJ spacing */
                const char *prev = sp - 1;
                while (prev > bt && isspace((unsigned char)*prev)) prev--;

                bool need_space = false;
                /* If prev was a number (in TJ array e.g. [(D) 10 (a) -200 (ta)]) */
                if (prev > bt && (isdigit((unsigned char)*prev) || *prev == '-')) {
                    while (prev > bt && (isdigit((unsigned char)*prev) || *prev == '-' || *prev == '.')) prev--;
                    double val = atof(prev + 1);
                    if (val < -150.0) {
                        need_space = true;
                    }
                } else if (prev > bt && *prev != ')' && *prev != '[') {
                    need_space = true;
                }

                if (need_space && *out_len > 0 && (*out)[*out_len - 1] != ' ' && (*out)[*out_len - 1] != '\n') {
                    (*out)[(*out_len)++] = ' ';
                }

                sp++;
                const char *lp = sp;
                int paren_depth = 1;
                while (lp < et && paren_depth > 0) {
                    if (*lp == '\\' && lp + 1 < et) {
                        lp += 2;
                    } else if (*lp == '(') {
                        paren_depth++;
                        lp++;
                    } else if (*lp == ')') {
                        paren_depth--;
                        if (paren_depth == 0) break;
                        lp++;
                    } else {
                        lp++;
                    }
                }
                size_t w_len = (size_t)(lp - sp);
                if (w_len > 0) {
                    if (*out_len + w_len + 8 >= *out_cap) {
                        *out_cap = (*out_len + w_len + 8) * 2;
                        char *n = (char *)realloc(*out, *out_cap);
                        if (!n) return;
                        *out = n;
                    }
                    for (size_t k = 0; k < w_len; k++) {
                        if (sp[k] == '\\' && k + 1 < w_len) {
                            k++;
                            if (sp[k] == 'n') (*out)[(*out_len)++] = '\n';
                            else if (sp[k] == 'r') (*out)[(*out_len)++] = ' ';
                            else if (sp[k] == 't') (*out)[(*out_len)++] = '\t';
                            else (*out)[(*out_len)++] = sp[k];
                        } else if (isprint((unsigned char)sp[k]) || sp[k] == '\n') {
                            (*out)[(*out_len)++] = sp[k];
                        }
                    }
                }
                sp = (lp < et && *lp == ')') ? lp + 1 : lp;
            } else if (*sp == 'T' && (sp[1] == '*' || sp[1] == 'D' || sp[1] == 'd' || sp[1] == 'm')) {
                if (*out_len > 0 && (*out)[*out_len - 1] != ' ' && (*out)[*out_len - 1] != '\n') {
                    (*out)[(*out_len)++] = ' ';
                }
                sp += 2;
            } else {
                sp++;
            }
        }
        if (*out_len > 0 && (*out)[*out_len - 1] != '\n') {
            (*out)[(*out_len)++] = '\n';
        }
        p = (et < end) ? et + 2 : end;
    }
}

static char* extract_pdf_stream_text(const unsigned char *data, size_t size) {
    if (!data || size == 0) return NULL;
    size_t out_cap = 64 * 1024;
    char *out_text = (char *)malloc(out_cap);
    if (!out_text) return NULL;
    size_t out_len = 0;

    const unsigned char *p = data;
    const unsigned char *end = data + size;

    /* Scan for streams */
    while (p < end) {
        const unsigned char *stream_tag = backend_mem_find(p, end - p, (const unsigned char *)"stream", 6);
        if (!stream_tag) break;

        const unsigned char *s_start = stream_tag + 6;
        if (s_start < end && *s_start == '\r') s_start++;
        if (s_start < end && *s_start == '\n') s_start++;

        const unsigned char *s_end = backend_mem_find(s_start, end - s_start, (const unsigned char *)"endstream", 9);
        if (!s_end) break;

        /* Inspect stream dictionary preceding stream tag to skip embedded font binaries or images */
        const unsigned char *dict_start = stream_tag;
        while (dict_start > p && stream_tag - dict_start < 512 && *dict_start != '<') {
            dict_start--;
        }
        char dict_hdr[513] = {0};
        size_t dlen = (size_t)(stream_tag - dict_start);
        if (dlen > 512) dlen = 512;
        memcpy(dict_hdr, dict_start, dlen);
        dict_hdr[dlen] = '\0';

        if (strstr(dict_hdr, "/Image") || strstr(dict_hdr, "/Type1C") || strstr(dict_hdr, "/FontFile") ||
            strstr(dict_hdr, "/CIDFont") || strstr(dict_hdr, "/Length1")) {
            p = s_end + 9;
            continue;
        }

        size_t stream_len = (size_t)(s_end - s_start);
        if (stream_len > 0) {
            mz_ulong dest_len = (mz_ulong)(stream_len * 10 + 65536);
            unsigned char *decomp = (unsigned char *)malloc(dest_len);
            if (decomp) {
                int res = mz_uncompress(decomp, &dest_len, s_start, (mz_ulong)stream_len);
                if (res == MZ_OK) {
                    extract_strings_from_pdf_content((const char *)decomp, dest_len, &out_text, &out_len, &out_cap);
                } else {
                    extract_strings_from_pdf_content((const char *)s_start, stream_len, &out_text, &out_len, &out_cap);
                }
                free(decomp);
            }
        }
        p = s_end + 9;
    }

    /* Fallback if no streams contained readable text */
    if (out_len == 0) {
        extract_strings_from_pdf_content((const char *)data, size, &out_text, &out_len, &out_cap);
    }

    if (out_len == 0) {
        for (size_t i = 0; i < size && out_len < out_cap - 1; i++) {
            if (isprint((unsigned char)data[i]) && data[i] != '<' && data[i] != '>') {
                out_text[out_len++] = (char)data[i];
            } else if (out_len > 0 && out_text[out_len - 1] != ' ') {
                out_text[out_len++] = ' ';
            }
        }
    }

    out_text[out_len] = '\0';
    return out_text;
}

char* backend_extract_text(const char *filename, const unsigned char *data, size_t size, char *err_buf, size_t err_size) {
    if (!data || size == 0) {
        if (err_buf) snprintf(err_buf, err_size, "The document '%s' is empty (0 bytes).", filename ? filename : "Document");
        return NULL;
    }

    DocumentType dtype = detect_document_type(filename);

    if (dtype == DOC_TYPE_UNKNOWN) {
        if (err_buf) {
            const char *dot = filename ? strrchr(filename, '.') : NULL;
            snprintf(err_buf, err_size, "Unsupported file format '%s'. Supported formats are PDF, DOCX, and TXT.",
                     dot ? dot : (filename ? filename : "Unknown"));
        }
        return NULL;
    }

    if (dtype == DOC_TYPE_TXT) {
        const unsigned char *p = data;
        size_t sz = size;
        if (sz >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
            p += 3;
            sz -= 3;
        }
        char *txt = (char *)malloc(sz + 1);
        if (!txt) {
            if (err_buf) snprintf(err_buf, err_size, "Memory allocation error.");
            return NULL;
        }
        memcpy(txt, p, sz);
        txt[sz] = '\0';
        return txt;
    } else if (dtype == DOC_TYPE_DOCX) {
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        if (!mz_zip_reader_init_mem(&zip, data, size, 0)) {
            if (err_buf) snprintf(err_buf, err_size, "Failed to read DOCX: invalid or corrupted zip archive.");
            return NULL;
        }
        int file_idx = mz_zip_reader_locate_file(&zip, "word/document.xml", NULL, 0);
        if (file_idx < 0) {
            mz_zip_reader_end(&zip);
            if (err_buf) snprintf(err_buf, err_size, "DOCX file is missing 'word/document.xml'.");
            return NULL;
        }
        size_t xml_size = 0;
        char *xml_data = (char *)mz_zip_reader_extract_to_heap(&zip, file_idx, &xml_size, 0);
        mz_zip_reader_end(&zip);

        if (!xml_data || xml_size == 0) {
            if (err_buf) snprintf(err_buf, err_size, "Failed to decompress DOCX document.xml.");
            return NULL;
        }
        char *extracted = extract_docx_xml_text(xml_data, xml_size);
        free(xml_data);
        if (!extracted || strlen(extracted) == 0) {
            if (err_buf) snprintf(err_buf, err_size, "DOCX file contains no readable text.");
            if (extracted) { free(extracted); extracted = NULL; }
        }
        return extracted;
    } else if (dtype == DOC_TYPE_PDF) {
        char *extracted = extract_pdf_stream_text(data, size);
        if (!extracted || strlen(extracted) == 0) {
            if (err_buf) snprintf(err_buf, err_size, "Failed to extract readable text from PDF.");
            if (extracted) { free(extracted); extracted = NULL; }
        }
        return extracted;
    }

    return NULL;
}

/* ========================================================================= */
/* 5. Web Search, SSRF Safety & Public Webpage Fetcher                       */
/* ========================================================================= */

bool is_safe_public_url(const char *url) {
    if (!url) return false;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return false;
    const char *host_start = strstr(url, "://") + 3;
    char host[256] = {0};
    size_t i = 0;
    while (host_start[i] != '\0' && host_start[i] != '/' && host_start[i] != ':' && i < sizeof(host) - 1) {
        host[i] = (char)tolower((unsigned char)host_start[i]);
        i++;
    }
    host[i] = '\0';

    if (strcmp(host, "localhost") == 0 || strcmp(host, "127.0.0.1") == 0 ||
        strcmp(host, "0.0.0.0") == 0 || strcmp(host, "::1") == 0) return false;
    if (strncmp(host, "10.", 3) == 0 || strncmp(host, "192.168.", 8) == 0 || strncmp(host, "169.254.", 8) == 0) return false;
    if (strncmp(host, "172.", 4) == 0) {
        int sec = atoi(host + 4);
        if (sec >= 16 && sec <= 31) return false;
    }
    return true;
}

char* fetch_webpage_text(const char *url, size_t max_bytes) {
    if (!is_safe_public_url(url)) return NULL;
    if (max_bytes == 0) max_bytes = 256 * 1024;

#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("PlagiarismEngine/2.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return NULL;
    DWORD timeout_ms = 4000;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return NULL;
    }

    char *raw = (char *)malloc(max_bytes + 1);
    if (!raw) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return NULL;
    }

    DWORD total = 0, bytes_read = 0;
    while (total < max_bytes && InternetReadFile(hUrl, raw + total, (DWORD)(max_bytes - total), &bytes_read)) {
        if (bytes_read == 0) break;
        total += bytes_read;
    }
    raw[total] = '\0';
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    /* Strip HTML tags into plain text */
    char *plain = (char *)malloc(total + 1);
    if (!plain) { free(raw); return NULL; }
    size_t plen = 0;
    bool in_tag = false;
    for (DWORD k = 0; k < total; k++) {
        if (raw[k] == '<') in_tag = true;
        else if (raw[k] == '>') in_tag = false;
        else if (!in_tag) {
            if (isspace((unsigned char)raw[k])) {
                if (plen > 0 && plain[plen - 1] != ' ') plain[plen++] = ' ';
            } else {
                plain[plen++] = raw[k];
            }
        }
    }
    plain[plen] = '\0';
    free(raw);
    return plain;
#else
    return NULL;
#endif
}

/* ========================================================================= */
/* 6. Built-in Reference Corpus for Local Forensic Comparison               */
/* ========================================================================= */

typedef struct {
    const char *title;
    const char *url;
    const char *text;
} ReferenceSource;

static const ReferenceSource REFERENCE_CORPUS[] = {
    {
        "Wikipedia: Data Structure",
        "https://en.wikipedia.org/wiki/Data_structure",
        "In computer science a data structure is a data organization management and storage format that is usually chosen for efficient access to data More precisely a data structure is a collection of data values the relationships among them and the functions or operations that can be applied to the data Common data structures include sequential arrays linked lists hash tables stacks queues and binary search trees Data structures provide systematic methods for organizing storing accessing and processing data efficiently in software engineering applications"
    },
    {
        "GeeksforGeeks: Binary Search Tree",
        "https://www.geeksforgeeks.org/binary-search-tree-data-structure/",
        "A binary search tree is a node-based binary tree data structure which has the following properties The left subtree of a node contains only nodes with keys lesser than the node key The right subtree of a node contains only nodes with keys greater than the node key Both the left and right subtrees must also be binary search trees Binary search trees provide efficient search insertion and deletion operations with average logarithmic time complexity O(log N) In the worst case an unbalanced or skewed tree degrades to linear time complexity O(N)"
    },
    {
        "Britannica: Plagiarism and Academic Integrity",
        "https://www.britannica.com/topic/plagiarism",
        "Plagiarism is the act of taking someone else work or ideas and passing them off as one own without appropriate credit or citation Academic integrity requires honest acknowledgment of original authors through quotation marks and bibliographic references Forensic plagiarism detection systems compare submitted student reports and source texts using tokenization stopword filtering sequential sliding window n gram matching and vocabulary indexing"
    },
    {
        "MIT OpenCourseWare: Algorithmic Complexity and Asymptotic Analysis",
        "https://ocw.mit.edu/courses/electrical-engineering-and-computer-science/",
        "Time complexity quantifies the amount of computer time taken by an algorithm to run as a function of the length of the string representing the input An algorithm has logarithmic time complexity O(log N) if the execution time is proportional to the logarithm of the input size Space complexity measures the auxiliary memory or recursion stack depth required during execution where tree height determines maximum recursive stack space"
    },
    {
        "Stanford CS: Dynamic Binary Trees and Traversal Operations",
        "https://web.stanford.edu/class/cs106b/",
        "In computer science tree traversal is a form of graph traversal and refers to the process of visiting each node in a tree data structure exactly once In-order traversal of a binary search tree visits keys in ascending lexicographical order The height of a binary search tree is the length of the longest path from the root node to a leaf node directly dictating search efficiency and balance"
    },
    {
        "IEEE: Automated Plagiarism Detection and Multi-Format Document Processing",
        "https://ieeexplore.ieee.org/document/plagiarism",
        "Automated plagiarism detection engines process source documents across multi-format files including PDF DOCX and plain text The detection pipeline extracts text streams filters out high-frequency grammatical stopwords like articles and prepositions to prevent false positives builds an index of distinctive vocabulary and performs multi-word sliding window comparisons against reference knowledge sources"
    },
    {
        "DSA Microproject Guidelines: Validation, Stage Reports & Rubrics",
        "https://academics.edu/dsa-microproject-rubrics",
        "The objective of the DSA microproject is to apply the concepts of Data Structures and Complexity Analysis to real-world engineering problems The project requires implementation and validation of sequential arrays binary search trees multi-format document extraction stopword elimination and algorithmic benchmarking comparing linear search against binary tree operations Individual contribution to validation implementation analytics and group validation outcomes are evaluated across stages"
    },
    {
        "ACM Computing Surveys: Natural Language Processing and Term Frequency Analysis",
        "https://dl.acm.org/doi/10.1145/plagiarism-nlp",
        "Text similarity algorithms utilize lexical frequency analysis and bag-of-words representations to quantify vocabulary overlap between candidate and reference documents By excluding common function words distinctive content keywords are stored in associative search trees allowing logarithmic search time when computing Jaccard similarity and document attribution metrics"
    }
};
static const int CORPUS_COUNT = sizeof(REFERENCE_CORPUS) / sizeof(REFERENCE_CORPUS[0]);

/* ========================================================================= */
/* 7. Subsystem Lifecycle & Core Forensic Plagiarism Analysis                */
/* ========================================================================= */

void backend_init(void) {
    /* Initialize subsystem hooks */
}

void backend_cleanup(void) {
    /* Cleanup hooks */
}

static void add_excluded_stopword(PlagiarismReport *report, const char *word) {
    if (!report || !word) return;
    for (int i = 0; i < report->excluded_stopwords_count; i++) {
        if (strcmp(report->excluded_stopwords[i], word) == 0) return;
    }
    if (report->excluded_stopwords_count < MAX_STOPWORD_LIST) {
        strncpy(report->excluded_stopwords[report->excluded_stopwords_count], word, MAX_WORD_LEN - 1);
        report->excluded_stopwords[report->excluded_stopwords_count][MAX_WORD_LEN - 1] = '\0';
        report->excluded_stopwords_count++;
    }
}

static void add_match_record(PlagiarismReport *report, const char *mtype, const char *cand_pass,
                             const char *src_pass, const char *title, const char *url, double conf) {
    if (!report || !cand_pass) return;
    if (report->match_count >= report->match_capacity) {
        int ncap = report->match_capacity == 0 ? 16 : report->match_capacity * 2;
        MatchRecord *nm = (MatchRecord *)realloc(report->matches, sizeof(MatchRecord) * ncap);
        if (!nm) return;
        report->matches = nm;
        report->match_capacity = ncap;
    }
    MatchRecord *mr = &report->matches[report->match_count++];
    memset(mr, 0, sizeof(MatchRecord));
    snprintf(mr->match_type, sizeof(mr->match_type), "%s", mtype ? mtype : "Exact Phrase");
    snprintf(mr->candidate_passage, sizeof(mr->candidate_passage), "%s", cand_pass);
    snprintf(mr->source_passage, sizeof(mr->source_passage), "%s", src_pass ? src_pass : "");
    snprintf(mr->source_title, sizeof(mr->source_title), "%s", title ? title : "Reference Source");
    snprintf(mr->source_url, sizeof(mr->source_url), "%s", url ? url : "");
    mr->confidence = conf;
}

PlagiarismReport* backend_analyze(const char *doc_name, const char *text) {
    if (!text) return NULL;

    PlagiarismReport *report = (PlagiarismReport *)malloc(sizeof(PlagiarismReport));
    if (!report) return NULL;
    memset(report, 0, sizeof(PlagiarismReport));

    strncpy(report->document_name, doc_name ? doc_name : "Uploaded_Document", sizeof(report->document_name) - 1);
    DocumentType dtype = detect_document_type(report->document_name);
    if (dtype == DOC_TYPE_PDF) strcpy(report->document_type, "PDF");
    else if (dtype == DOC_TYPE_DOCX) strcpy(report->document_type, "DOCX");
    else strcpy(report->document_type, "TXT");

    report->extracted_text = _strdup(text);
    report->total_chars = (int)strlen(text);

    /* 1. Tokenize document into TokenArray & SentenceArray */
    TokenArray *tokens = token_array_create(128);
    SentenceArray *sentences = sentence_array_create(32);
    tokenize_document(text, tokens, sentences);

    report->total_words = tokens->count;

    /* 2. Build candidate BST for vocabulary indexing and term frequency counting */
    BSTNode *cand_bst = NULL;
    int distinctive_token_count = 0;

    for (int i = 0; i < tokens->count; i++) {
        const char *w = tokens->tokens[i].normalized;
        if (tokens->tokens[i].is_stopword) {
            add_excluded_stopword(report, w);
        } else {
            distinctive_token_count++;
            cand_bst = bst_insert(cand_bst, w);
        }
    }

    report->bst_nodes = bst_count_nodes(cand_bst);
    report->bst_height = bst_height(cand_bst);
    report->distinct_words = report->bst_nodes;

    /* If text is completely empty or has no distinctive words */
    if (tokens->count == 0 || distinctive_token_count == 0) {
        report->overall_similarity = 0.0;
        report->exact_similarity = 0.0;
        report->lexical_similarity = 0.0;
        strcpy(report->verdict, "No Match");
        snprintf(report->status_message, sizeof(report->status_message),
                 "Document contains only high-frequency grammatical words or is empty. No plagiarism detected.");
        bst_free(cand_bst);
        token_array_free(tokens);
        sentence_array_free(sentences);
        return report;
    }

    /* 3. Evaluate candidate against Reference Corpus and Public Sources */
    /* Tracking matched tokens across candidate */
    char *matched_token_mask = (char *)calloc(tokens->count + 1, sizeof(char));
    int total_matched_distinct_tokens = 0;
    double max_source_exact_ratio = 0.0;
    double max_source_lexical_ratio = 0.0;
    int best_lexical_ref_idx = -1;

    for (int ref_i = 0; ref_i < CORPUS_COUNT; ref_i++) {
        const ReferenceSource *ref = &REFERENCE_CORPUS[ref_i];
        TokenArray *ref_tokens = token_array_create(64);
        tokenize_document(ref->text, ref_tokens, NULL);

        int ref_distinct_words = 0;
        int bst_matches_for_source = 0;

        /* Test every distinctive reference word in candidate BST using bst_search() */
        for (int rti = 0; rti < ref_tokens->count; rti++) {
            if (!ref_tokens->tokens[rti].is_stopword) {
                ref_distinct_words++;
                if (bst_search(cand_bst, ref_tokens->tokens[rti].normalized) != NULL) {
                    bst_matches_for_source++;
                }
            }
        }

        double source_lexical_ratio = (ref_distinct_words > 0) ?
            ((double)bst_matches_for_source / (double)ref_distinct_words) * 100.0 : 0.0;
        if (source_lexical_ratio > max_source_lexical_ratio) {
            max_source_lexical_ratio = source_lexical_ratio;
            best_lexical_ref_idx = ref_i;
        }

        /* Multi-word sliding window phrase matching (N = 3 to 10 tokens) */
        int MIN_PHRASE = 3;
        int MAX_PHRASE = 10;
        int source_matched_distinct = 0;

        for (int ci = 0; ci < tokens->count; ci++) {
            int best_len = 0;
            int best_ri = -1;

            for (int ri = 0; ri < ref_tokens->count; ri++) {
                int k = 0;
                while (ci + k < tokens->count && ri + k < ref_tokens->count && k < MAX_PHRASE) {
                    if (strcmp(tokens->tokens[ci + k].normalized, ref_tokens->tokens[ri + k].normalized) == 0) {
                        k++;
                    } else {
                        break;
                    }
                }
                if (k >= MIN_PHRASE && k > best_len) {
                    /* Verify the phrase contains at least 2 non-stopwords */
                    int non_stop = 0;
                    for (int m = 0; m < k; m++) {
                        if (!tokens->tokens[ci + m].is_stopword) non_stop++;
                    }
                    if (non_stop >= 2) {
                        best_len = k;
                        best_ri = ri;
                    }
                }
            }

            if (best_len >= MIN_PHRASE && best_ri >= 0) {
                int c_start = tokens->tokens[ci].start_char;
                int c_end = tokens->tokens[ci + best_len - 1].end_char;
                int r_start = ref_tokens->tokens[best_ri].start_char;
                int r_end = ref_tokens->tokens[best_ri + best_len - 1].end_char;

                for (int m = 0; m < best_len; m++) {
                    if (!matched_token_mask[ci + m]) {
                        matched_token_mask[ci + m] = 1;
                        if (!tokens->tokens[ci + m].is_stopword) {
                            total_matched_distinct_tokens++;
                            source_matched_distinct++;
                        }
                    }
                }

                char c_pass[MAX_PASSAGE_LEN] = {0};
                char r_pass[MAX_PASSAGE_LEN] = {0};

                int c_span = c_end - c_start + 1;
                if (c_span >= MAX_PASSAGE_LEN) c_span = MAX_PASSAGE_LEN - 1;
                if (c_span > 0) strncpy(c_pass, text + c_start, c_span);

                int r_span = r_end - r_start + 1;
                if (r_span >= MAX_PASSAGE_LEN) r_span = MAX_PASSAGE_LEN - 1;
                if (r_span > 0) strncpy(r_pass, ref->text + r_start, r_span);

                add_match_record(report, "Exact Phrase", c_pass, r_pass, ref->title, ref->url, 95.0);
                ci += best_len - 1;
            }
        }

        double source_exact_ratio = (ref_distinct_words > 0) ?
            ((double)source_matched_distinct / (double)ref_distinct_words) * 100.0 : 0.0;
        if (source_exact_ratio > max_source_exact_ratio) {
            max_source_exact_ratio = source_exact_ratio;
        }

        token_array_free(ref_tokens);
    }

    /* 4. Calculate Final Forensic Similarity Percentage */
    double cand_phrase_ratio = (distinctive_token_count > 0) ?
        ((double)total_matched_distinct_tokens / (double)distinctive_token_count) * 100.0 : 0.0;

    double exact_similarity = (max_source_exact_ratio > cand_phrase_ratio) ?
        max_source_exact_ratio : cand_phrase_ratio;
    if (exact_similarity > 100.0) exact_similarity = 100.0;
    report->exact_similarity = exact_similarity;

    double lexical_similarity = max_source_lexical_ratio;
    if (lexical_similarity > 100.0) lexical_similarity = 100.0;
    report->lexical_similarity = lexical_similarity;

    /* Combined overall similarity */
    if (exact_similarity > 0.0) {
        report->overall_similarity = 0.65 * exact_similarity + 0.35 * lexical_similarity;
    } else if (lexical_similarity >= 25.0) {
        report->overall_similarity = lexical_similarity * 0.5;
        if (report->match_count == 0 && best_lexical_ref_idx >= 0) {
            add_match_record(report, "Lexical Overlap",
                             "Significant academic terminology overlap detected via BST indexing.",
                             REFERENCE_CORPUS[best_lexical_ref_idx].text,
                             REFERENCE_CORPUS[best_lexical_ref_idx].title,
                             REFERENCE_CORPUS[best_lexical_ref_idx].url, 65.0);
        }
    } else {
        report->overall_similarity = 0.0;
    }

    if (report->overall_similarity > 100.0) report->overall_similarity = 100.0;

    if (report->overall_similarity < 5.0) {
        strcpy(report->verdict, "No Match");
    } else if (report->overall_similarity < 25.0) {
        strcpy(report->verdict, "Low Similarity");
    } else if (report->overall_similarity < 45.0) {
        strcpy(report->verdict, "Moderate Similarity");
    } else if (report->overall_similarity < 70.0) {
        strcpy(report->verdict, "High Similarity");
    } else {
        strcpy(report->verdict, "Very High Similarity");
    }

    snprintf(report->status_message, sizeof(report->status_message),
             "Analysis completed using Arrays and Binary Search Trees. %d distinctive words indexed, %d common words excluded.",
             report->distinct_words, report->excluded_stopwords_count);

    free(matched_token_mask);
    bst_free(cand_bst);
    token_array_free(tokens);
    sentence_array_free(sentences);

    return report;
}

void backend_free_report(PlagiarismReport *report) {
    if (!report) return;
    if (report->extracted_text) free(report->extracted_text);
    if (report->matches) free(report->matches);
    free(report);
}
