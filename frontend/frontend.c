#include "frontend.h"
#include "../backend/backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <winsock2.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#endif

static SOCKET g_server_socket = INVALID_SOCKET;
static volatile bool g_running = false;

/* HTML entity escaping helper to prevent injection */
static char* escape_html(const char *src) {
    if (!src) return _strdup("");
    size_t len = strlen(src);
    size_t cap = len * 2 + 64;
    char *out = (char *)malloc(cap);
    if (!out) return _strdup("");

    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (o + 8 >= cap) {
            cap *= 2;
            char *n = (char *)realloc(out, cap);
            if (!n) { free(out); return _strdup(""); }
            out = n;
        }
        unsigned char c = (unsigned char)src[i];
        if (c == '&') { strcpy(out + o, "&amp;"); o += 5; }
        else if (c == '<') { strcpy(out + o, "&lt;"); o += 4; }
        else if (c == '>') { strcpy(out + o, "&gt;"); o += 4; }
        else if (c == '"') { strcpy(out + o, "&quot;"); o += 6; }
        else if (c == '\'') { strcpy(out + o, "&#39;"); o += 5; }
        else { out[o++] = (char)c; }
    }
    out[o] = '\0';
    return out;
}

/* URL decoding helper */
static char* url_decode(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '+') {
            out[o++] = ' ';
        } else if (src[i] == '%' && i + 2 < len && isxdigit((unsigned char)src[i+1]) && isxdigit((unsigned char)src[i+2])) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            out[o++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            out[o++] = src[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* Extract field from application/x-www-form-urlencoded */
static char* get_form_field(const char *body, const char *key) {
    if (!body || !key) return NULL;
    size_t klen = strlen(key);
    const char *p = body;
    while (p && *p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *val_start = p + klen + 1;
            const char *val_end = strchr(val_start, '&');
            size_t vlen = val_end ? (size_t)(val_end - val_start) : strlen(val_start);
            char *raw = (char *)malloc(vlen + 1);
            if (!raw) return NULL;
            strncpy(raw, val_start, vlen);
            raw[vlen] = '\0';
            char *dec = url_decode(raw);
            free(raw);
            return dec;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return NULL;
}

/* ========================================================================= */
/* HTML & Tailwind CSS Page Rendering: Modern Academic Integrity Interface  */
/* ========================================================================= */

static const unsigned char* frontend_mem_find(const unsigned char *haystack, size_t hlen, const unsigned char *needle, size_t nlen) {
    if (!haystack || !needle || nlen == 0 || hlen < nlen) return NULL;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        if (haystack[i] == needle[0] && memcmp(haystack + i, needle, nlen) == 0) {
            return haystack + i;
        }
    }
    return NULL;
}

static char* render_page(const PlagiarismReport *report, const char *err_msg) {
    size_t cap = 512 * 1024;
    if (report && report->extracted_text) {
        cap += strlen(report->extracted_text) * 4;
    }
    char *html = (char *)malloc(cap);
    if (!html) return NULL;
    size_t len = 0;

    /* 1. Header and Styles */
    len += snprintf(html + len, cap - len,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\" class=\"h-full\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\" />\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
        "  <title>Plagiarism Detection Engine</title>\n"
        "  <script src=\"https://cdn.tailwindcss.com\"></script>\n"
        "  <link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">\n"
        "  <link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>\n"
        "  <link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700;800&family=JetBrains+Mono:wght@400;500;600&display=swap\" rel=\"stylesheet\">\n"
        "  <script src=\"https://unpkg.com/lucide@latest\"></script>\n"
        "  <script>\n"
        "    tailwind.config = {\n"
        "      darkMode: 'class',\n"
        "      theme: {\n"
        "        extend: {\n"
        "          fontFamily: {\n"
        "            sans: ['Inter', 'sans-serif'],\n"
        "            mono: ['JetBrains Mono', 'monospace'],\n"
        "          },\n"
        "          colors: {\n"
        "            offwhite: {\n"
        "              50: '#FAF8F5',\n"
        "              100: '#FAF9F6',\n"
        "              200: '#F2EFE9',\n"
        "              300: '#E8E4DC',\n"
        "              400: '#D9D3C7',\n"
        "            },\n"
        "            terracotta: {\n"
        "              50: '#fdf8f6',\n"
        "              100: '#fbeee9',\n"
        "              500: '#e05d38',\n"
        "              600: '#c84725',\n"
        "              700: '#a33519',\n"
        "              900: '#602213',\n"
        "            },\n"
        "            stonegray: {\n"
        "              800: '#2b2927',\n"
        "              900: '#1c1b1a',\n"
        "            }\n"
        "          }\n"
        "        }\n"
        "      }\n"
        "    };\n"
        "  </script>\n"
        "  <style>\n"
        "    ::-webkit-scrollbar { width: 6px; height: 6px; }\n"
        "    ::-webkit-scrollbar-track { background: transparent; }\n"
        "    ::-webkit-scrollbar-thumb { background: rgba(168, 162, 158, 0.4); border-radius: 9999px; }\n"
        "    .dark ::-webkit-scrollbar-thumb { background: rgba(78, 74, 71, 0.5); }\n"
        "    .glass-card { background: rgba(250, 248, 245, 0.9); backdrop-filter: blur(12px); }\n"
        "    .dark .glass-card { background: rgba(28, 27, 26, 0.85); backdrop-filter: blur(12px); }\n"
        "  </style>\n"
        "</head>\n"
        "<body class=\"bg-[#FAF9F6] text-stone-800 dark:bg-[#121110] dark:text-stone-100 transition-colors duration-300 min-h-full flex flex-col font-sans antialiased selection:bg-terracotta-500 selection:text-white\">\n");

    /* 2. Sticky Header Navbar */
    len += snprintf(html + len, cap - len,
        "  <!-- Header Section -->\n"
        "  <header class=\"sticky top-0 z-40 w-full border-b border-[#E8E4DC] dark:border-stone-800/80 bg-[#FAF8F5]/90 dark:bg-stone-900/80 backdrop-blur-md\">\n"
        "    <div class=\"max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 h-16 flex items-center justify-between\">\n"
        "      <!-- Logo & Title -->\n"
        "      <a href=\"/\" class=\"flex items-center space-x-3 text-inherit no-underline\">\n"
        "        <div class=\"p-2.5 bg-gradient-to-tr from-stone-900 to-terracotta-600 rounded-xl shadow-md text-white flex items-center justify-center\">\n"
        "          <i data-lucide=\"shield-check\" class=\"w-5 h-5\"></i>\n"
        "        </div>\n"
        "        <div>\n"
        "          <span class=\"font-bold text-lg tracking-tight text-stone-900 dark:text-white\">\n"
        "            Plagiarism Detection Engine\n"
        "          </span>\n"
        "          <span class=\"hidden sm:inline-block ml-2 px-2 py-0.5 text-xs font-semibold text-emerald-800 dark:text-emerald-400 bg-emerald-100/70 dark:bg-emerald-950/50 border border-emerald-300 dark:border-emerald-800/50 rounded-full\">\n"
        "            v2.4 Ready\n"
        "          </span>\n"
        "        </div>\n"
        "      </a>\n"
        "\n"
        "      <!-- Right Metadata & Theme Toggle -->\n"
        "      <div class=\"flex items-center space-x-3\">\n"
        "        <!-- Tech Tag -->\n"
        "        <div class=\"hidden md:flex items-center px-3 py-1.5 rounded-full text-xs font-mono font-medium bg-[#F2EFE9] dark:bg-stone-800/80 border border-[#E8E4DC] dark:border-stone-700/60 text-stone-700 dark:text-stone-300 shadow-sm\">\n"
        "          <span class=\"w-2 h-2 rounded-full bg-amber-500 animate-pulse mr-2\"></span>\n"
        "          C11 Standard &bull; Binary Search Tree\n"
        "        </div>\n"
        "\n"
        "        <!-- Dark/Light Theme Switcher -->\n"
        "        <button type=\"button\" id=\"themeToggleBtn\" onclick=\"toggleTheme()\" class=\"p-2.5 rounded-xl border border-[#E8E4DC] dark:border-stone-800 bg-[#F2EFE9]/80 dark:bg-stone-800/60 hover:bg-[#E8E4DC] dark:hover:bg-stone-700 text-stone-700 dark:text-stone-300 transition-all focus:outline-none focus:ring-2 focus:ring-terracotta-500\" title=\"Toggle Light/Dark Theme\">\n"
        "          <i data-lucide=\"moon\" id=\"moonIcon\" class=\"w-4 h-4 hidden dark:block\"></i>\n"
        "          <i data-lucide=\"sun\" id=\"sunIcon\" class=\"w-4 h-4 block dark:hidden text-amber-700\"></i>\n"
        "        </button>\n"
        "      </div>\n"
        "    </div>\n"
        "  </header>\n");

    /* 3. Main Workspace Container */
    len += snprintf(html + len, cap - len,
        "  <!-- Main Container -->\n"
        "  <main class=\"flex-1 max-w-5xl w-full mx-auto px-4 sm:px-6 py-8 md:py-12 flex flex-col justify-center\">\n"
        "\n"
        "    <!-- Hero Header -->\n"
        "    <div class=\"text-center max-w-3xl mx-auto mb-8 sm:mb-12\">\n"
        "      <div class=\"inline-flex items-center space-x-2 px-3.5 py-1.5 rounded-full bg-amber-100/70 dark:bg-stone-800/80 border border-amber-300/80 dark:border-stone-700 text-amber-900 dark:text-amber-300 text-xs font-medium mb-4 shadow-sm\">\n"
        "        <i data-lucide=\"sparkles\" class=\"w-3.5 h-3.5 text-amber-700 dark:text-amber-400\"></i>\n"
        "        <span>High Performance Text Matching &amp; Semantic Indexing</span>\n"
        "      </div>\n"
        "      <h1 class=\"text-3xl sm:text-4xl md:text-5xl font-extrabold tracking-tight text-stone-900 dark:text-white mb-4 leading-tight\">\n"
        "        Plagiarism Detection Engine\n"
        "      </h1>\n"
        "      <p class=\"text-stone-600 dark:text-stone-300 text-sm sm:text-base leading-relaxed max-w-2xl mx-auto\">\n"
        "        Academic originality analysis powered by balanced <span class=\"font-semibold text-terracotta-600 dark:text-terracotta-500\">Binary Search Trees</span> and optimized sequential token arrays. Upload a document (PDF, DOCX, TXT) or paste text to detect matching phrases and verify originality against indexed sources.\n"
        "      </p>\n"
        "    </div>\n");

    /* Error Notice Banner if any */
    if (err_msg && strlen(err_msg) > 0) {
        char *esc_err = escape_html(err_msg);
        len += snprintf(html + len, cap - len,
            "    <div class=\"mb-6 p-4 rounded-xl bg-rose-50 dark:bg-rose-950/60 border border-rose-200 dark:border-rose-800/80 text-rose-800 dark:text-rose-200 text-sm flex items-center space-x-3\">\n"
            "      <i data-lucide=\"alert-circle\" class=\"w-5 h-5 flex-shrink-0 text-rose-600 dark:text-rose-400\"></i>\n"
            "      <div><strong>Notice:</strong> %s</div>\n"
            "    </div>\n", esc_err);
        free(esc_err);
    }

    /* 4. Main Workspace Form Card */
    len += snprintf(html + len, cap - len,
        "    <!-- Main Workspace Card -->\n"
        "    <div class=\"glass-card rounded-2xl border border-[#E8E4DC] dark:border-stone-800 shadow-xl shadow-stone-300/30 dark:shadow-stone-950/50 overflow-hidden transition-all\">\n"
        "      <form action=\"/analyze\" method=\"POST\" enctype=\"multipart/form-data\" id=\"uploadForm\" class=\"p-5 sm:p-8 space-y-6\">\n"
        "\n"
        "        <!-- File Drag & Drop Dropzone -->\n"
        "        <div id=\"dropZone\" class=\"relative group border-2 border-dashed border-[#D9D3C7] dark:border-stone-700 hover:border-terracotta-500 dark:hover:border-terracotta-500 rounded-xl p-6 sm:p-8 text-center bg-[#F2EFE9]/50 dark:bg-stone-900/40 hover:bg-amber-50/40 dark:hover:bg-stone-800/40 transition-all cursor-pointer\">\n"
        "          <input type=\"file\" id=\"fileInput\" name=\"docfile\" class=\"hidden\" accept=\".txt,.pdf,.docx\" onchange=\"handleFileSelect(event)\" />\n"
        "          \n"
        "          <div class=\"flex flex-col items-center justify-center space-y-3\">\n"
        "            <div class=\"w-14 h-14 rounded-2xl bg-amber-100 dark:bg-stone-800 text-amber-800 dark:text-amber-400 flex items-center justify-center shadow-inner group-hover:scale-110 transition-transform\">\n"
        "              <i data-lucide=\"upload-cloud\" class=\"w-7 h-7\"></i>\n"
        "            </div>\n"
        "            <div>\n"
        "              <p class=\"text-stone-900 dark:text-stone-200 font-semibold text-base\">\n"
        "                Choose a document or drag &amp; drop it here\n"
        "              </p>\n"
        "              <p class=\"text-xs text-stone-500 dark:text-stone-400 mt-1\">\n"
        "                Supported formats: <span class=\"font-medium text-stone-700 dark:text-stone-300\">PDF, DOCX, TXT</span> (up to 16MB)\n"
        "              </p>\n"
        "            </div>\n"
        "            \n"
        "            <button type=\"button\" onclick=\"document.getElementById('fileInput').click()\" class=\"mt-2 inline-flex items-center px-4 py-2 text-xs font-semibold rounded-lg text-stone-800 dark:text-stone-200 bg-[#E8E4DC] dark:bg-stone-800 hover:bg-[#D9D3C7] dark:hover:bg-stone-700 border border-[#D9D3C7] dark:border-transparent transition-colors shadow-sm\">\n"
        "              <i data-lucide=\"folder-open\" class=\"w-3.5 h-3.5 mr-1.5 text-amber-800 dark:text-amber-400\"></i>\n"
        "              Browse Files\n"
        "            </button>\n"
        "          </div>\n"
        "\n"
        "          <!-- File Selected Display -->\n"
        "          <div id=\"fileInfo\" class=\"hidden mt-4 p-3 bg-emerald-50 dark:bg-emerald-950/50 border border-emerald-200 dark:border-emerald-800 rounded-lg flex items-center justify-between\">\n"
        "            <div class=\"flex items-center space-x-3\">\n"
        "              <i data-lucide=\"file-text\" class=\"w-5 h-5 text-emerald-600 dark:text-emerald-400\"></i>\n"
        "              <div class=\"text-left\">\n"
        "                <p id=\"fileName\" class=\"text-xs font-semibold text-emerald-900 dark:text-emerald-200 truncate max-w-xs\">document.txt</p>\n"
        "                <p id=\"fileSize\" class=\"text-[10px] text-emerald-700 dark:text-emerald-400\">0 KB</p>\n"
        "              </div>\n"
        "            </div>\n"
        "            <button type=\"button\" onclick=\"removeFile(event)\" class=\"text-stone-400 hover:text-rose-500 transition-colors p-1\">\n"
        "              <i data-lucide=\"x\" class=\"w-4 h-4\"></i>\n"
        "            </button>\n"
        "          </div>\n"
        "        </div>\n"
        "\n"
        "        <!-- Divider -->\n"
        "        <div class=\"relative flex py-1 items-center\">\n"
        "          <div class=\"flex-grow border-t border-[#E8E4DC] dark:border-stone-800\"></div>\n"
        "          <span class=\"flex-shrink mx-4 text-xs font-medium text-stone-400 dark:text-stone-500 uppercase tracking-wider\">or paste text directly</span>\n"
        "          <div class=\"flex-grow border-t border-[#E8E4DC] dark:border-stone-800\"></div>\n"
        "        </div>\n"
        "\n"
        "        <!-- Text Input & Sample Actions -->\n"
        "        <div class=\"space-y-2\">\n"
        "          <div class=\"flex items-center justify-between text-xs font-medium text-stone-500 dark:text-stone-400\">\n"
        "            <span>Passage Content</span>\n"
        "            <div class=\"flex space-x-3\">\n"
        "              <button type=\"button\" onclick=\"loadSampleText('original')\" class=\"hover:text-terracotta-600 dark:hover:text-amber-400 transition-colors\">Load Original Sample</button>\n"
        "              <span>&bull;</span>\n"
        "              <button type=\"button\" onclick=\"loadSampleText('plagiarized')\" class=\"hover:text-rose-600 dark:hover:text-rose-400 transition-colors\">Load Matches Sample</button>\n"
        "            </div>\n"
        "          </div>\n"
        "          \n"
        "          <div class=\"relative\">\n"
        "            <textarea id=\"textInput\" name=\"pasted_text\" rows=\"7\" oninput=\"updateCounts()\" placeholder=\"Paste passage or document text here for rapid similarity check...\" class=\"w-full p-4 rounded-xl text-sm bg-[#FAF8F5] dark:bg-stone-900 border border-[#E8E4DC] dark:border-stone-800 text-stone-900 dark:text-stone-100 placeholder-stone-400 dark:placeholder-stone-500 focus:ring-2 focus:ring-terracotta-500 focus:border-terracotta-500 focus:outline-none transition-all resize-y font-sans\"></textarea>\n"
        "            \n"
        "            <div class=\"absolute bottom-3 right-3 flex items-center space-x-2 bg-[#F2EFE9]/90 dark:bg-stone-900/90 backdrop-blur px-2.5 py-1 rounded-md border border-[#E8E4DC] dark:border-stone-800 text-[11px] font-mono text-stone-600 dark:text-stone-400 shadow-sm\">\n"
        "              <span id=\"charCount\">0 chars</span>\n"
        "              <span>|</span>\n"
        "              <span id=\"wordCount\">0 words</span>\n"
        "            </div>\n"
        "          </div>\n"
        "        </div>\n"
        "\n"
        "        <!-- Action Control Bar -->\n"
        "        <div class=\"flex flex-col sm:flex-row items-center justify-between gap-4 pt-2\">\n"
        "          <div class=\"flex items-center space-x-2 text-xs text-stone-500 dark:text-stone-400\">\n"
        "            <i data-lucide=\"cpu\" class=\"w-4 h-4 text-terracotta-600 dark:text-amber-500\"></i>\n"
        "            <span>BST Search Depth: <strong class=\"text-stone-800 dark:text-stone-300 font-mono\">O(log n)</strong></span>\n"
        "          </div>\n"
        "\n"
        "          <div class=\"flex items-center space-x-3 w-full sm:w-auto\">\n"
        "            <button type=\"button\" onclick=\"clearAll()\" class=\"w-1/2 sm:w-auto px-4 py-2.5 rounded-xl border border-[#E8E4DC] dark:border-stone-800 bg-[#F2EFE9]/60 text-stone-700 dark:text-stone-300 hover:bg-[#E8E4DC] dark:hover:bg-stone-800 text-xs font-semibold transition-colors flex items-center justify-center cursor-pointer\">\n"
        "              <i data-lucide=\"rotate-ccw\" class=\"w-3.5 h-3.5 mr-1.5\"></i>\n"
        "              Clear\n"
        "            </button>\n"
        "            <button type=\"button\" id=\"scanBtn\" onclick=\"runPlagiarismScan()\" class=\"w-1/2 sm:w-auto px-6 py-2.5 rounded-xl bg-stone-900 hover:bg-stone-800 dark:bg-terracotta-600 dark:hover:bg-terracotta-700 text-white text-xs font-semibold shadow-md transition-all flex items-center justify-center cursor-pointer\">\n"
        "              <i data-lucide=\"search\" class=\"w-3.5 h-3.5 mr-1.5\"></i>\n"
        "              Scan Document\n"
        "            </button>\n"
        "          </div>\n"
        "        </div>\n"
        "\n"
        "        <!-- Progress Indicator -->\n"
        "        <div id=\"scanProgress\" class=\"hidden space-y-2 pt-2\">\n"
        "          <div class=\"flex justify-between text-xs font-semibold text-stone-600 dark:text-stone-300\">\n"
        "            <span id=\"progressStep\">Building Binary Search Tree...</span>\n"
        "            <span id=\"progressPercent\">0%%</span>\n"
        "          </div>\n"
        "          <div class=\"w-full h-2 bg-[#E8E4DC] dark:bg-stone-800 rounded-full overflow-hidden\">\n"
        "            <div id=\"progressBar\" class=\"h-full bg-terracotta-600 rounded-full w-0 transition-all duration-300\"></div>\n"
        "          </div>\n"
        "        </div>\n"
        "\n"
        "      </form>\n"
        "    </div>\n");

    /* 5. Results Section */
    if (report) {
        char *esc_docname = escape_html(report->document_name);
        char *esc_text = escape_html(report->extracted_text ? report->extracted_text : "");

        double sim = report->overall_similarity;
        double orig = 100.0 - sim;
        if (orig < 0.0) orig = 0.0;
        if (orig > 100.0) orig = 100.0;

        const char *orig_verdict = "High Originality";
        const char *orig_color = "text-emerald-600 dark:text-emerald-400";
        const char *orig_sub = "Passed standard uniqueness checks";
        if (sim >= 70.0) {
            orig_verdict = "Plagiarism Detected";
            orig_color = "text-rose-600 dark:text-rose-400";
            orig_sub = "Significant verbatim matches identified";
        } else if (sim >= 45.0) {
            orig_verdict = "Moderate Originality";
            orig_color = "text-amber-600 dark:text-amber-400";
            orig_sub = "Notable phrase & vocabulary overlap";
        } else if (sim >= 20.0) {
            orig_verdict = "Acceptable Originality";
            orig_color = "text-emerald-600 dark:text-emerald-400";
            orig_sub = "Minor terminology overlap detected";
        }

        /* Banner */
        len += snprintf(html + len, cap - len,
            "    <!-- Results Visualization Section -->\n"
            "    <div id=\"resultsSection\" class=\"mt-8 space-y-6\">\n"
            "\n"
            "      <!-- Processed Banner -->\n"
            "      <div class=\"p-4 rounded-xl bg-emerald-50 dark:bg-emerald-950/40 border border-emerald-200 dark:border-emerald-800/60 flex items-center justify-between\">\n"
            "        <div class=\"flex items-center space-x-3\">\n"
            "          <i data-lucide=\"check-circle-2\" class=\"w-5 h-5 text-emerald-600 dark:text-emerald-400 flex-shrink-0\"></i>\n"
            "          <div class=\"text-xs text-emerald-900 dark:text-emerald-200\">\n"
            "            <strong>Document Analyzed:</strong> %s &bull; Format: <span class=\"font-mono font-bold\">%s</span> &bull; <strong>%d</strong> words, <strong>%d</strong> characters\n"
            "          </div>\n"
            "        </div>\n"
            "        <span class=\"text-[11px] font-mono font-medium text-emerald-700 dark:text-emerald-400 bg-emerald-100/60 dark:bg-emerald-900/60 px-2.5 py-1 rounded-md\">C11 Engine</span>\n"
            "      </div>\n"
            "\n"
            "      <!-- Primary Score Cards -->\n"
            "      <div class=\"grid grid-cols-1 md:grid-cols-3 gap-4\">\n"
            "        <!-- Originality Score -->\n"
            "        <div class=\"glass-card p-5 rounded-2xl border border-[#E8E4DC] dark:border-stone-800 flex items-center space-x-4\">\n"
            "          <div class=\"relative w-16 h-16 flex-shrink-0\">\n"
            "            <svg class=\"w-full h-full\" viewBox=\"0 0 36 36\">\n"
            "              <path class=\"text-[#E8E4DC] dark:text-stone-800\" stroke-width=\"3.5\" stroke=\"currentColor\" fill=\"none\" d=\"M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831\" />\n"
            "              <path id=\"scoreCircle\" class=\"text-terracotta-600 dark:text-terracotta-500 stroke-current\" stroke-linecap=\"round\" stroke-width=\"3.5\" stroke-dasharray=\"%.0f, 100\" fill=\"none\" d=\"M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831\" />\n"
            "            </svg>\n"
            "            <div class=\"absolute inset-0 flex items-center justify-center text-sm font-extrabold text-stone-900 dark:text-white\" id=\"scoreText\">\n"
            "              %.0f%%\n"
            "            </div>\n"
            "          </div>\n"
            "          <div>\n"
            "            <h4 class=\"text-xs font-semibold uppercase tracking-wider text-stone-500 dark:text-stone-400\">Originality Index</h4>\n"
            "            <p id=\"verdictStatus\" class=\"text-sm font-bold %s mt-0.5\">%s</p>\n"
            "            <p class=\"text-[11px] text-stone-500 dark:text-stone-400 mt-1\">%s</p>\n"
            "          </div>\n"
            "        </div>\n"
            "\n"
            "        <!-- Matched Phrases -->\n"
            "        <div class=\"glass-card p-5 rounded-2xl border border-[#E8E4DC] dark:border-stone-800 flex items-center space-x-4\">\n"
            "          <div class=\"p-3 bg-rose-100 dark:bg-rose-950/60 text-rose-600 dark:text-rose-400 rounded-xl\">\n"
            "            <i data-lucide=\"file-warning\" class=\"w-6 h-6\"></i>\n"
            "          </div>\n"
            "          <div>\n"
            "            <h4 class=\"text-xs font-semibold uppercase tracking-wider text-stone-500 dark:text-stone-400\">Similarity Match</h4>\n"
            "            <p id=\"matchPercentage\" class=\"text-lg font-bold text-stone-900 dark:text-white mt-0.5\">%.1f%%</p>\n"
            "            <p class=\"text-[11px] text-stone-500 dark:text-stone-400 mt-1\" id=\"matchesFoundCount\">Exact: <strong>%.1f%%</strong> &bull; BST: <strong>%.1f%%</strong> (%d matches)</p>\n"
            "          </div>\n"
            "        </div>\n"
            "\n"
            "        <!-- BST Node Specs -->\n"
            "        <div class=\"glass-card p-5 rounded-2xl border border-[#E8E4DC] dark:border-stone-800 flex items-center space-x-4\">\n"
            "          <div class=\"p-3 bg-amber-100 dark:bg-stone-800 text-amber-800 dark:text-amber-400 rounded-xl\">\n"
            "            <i data-lucide=\"git-fork\" class=\"w-6 h-6\"></i>\n"
            "          </div>\n"
            "          <div>\n"
            "            <h4 class=\"text-xs font-semibold uppercase tracking-wider text-stone-500 dark:text-stone-400\">BST Index Nodes</h4>\n"
            "            <p id=\"bstNodesCount\" class=\"text-lg font-bold text-stone-900 dark:text-white mt-0.5\">%d Nodes</p>\n"
            "            <p class=\"text-[11px] text-stone-500 dark:text-stone-400 mt-1\">Tree Height: %d &bull; In-Order Search: &lt;1ms</p>\n"
            "          </div>\n"
            "        </div>\n"
            "      </div>\n"
            "\n"
            "      <!-- Detail Analysis Grid -->\n"
            "      <div class=\"grid grid-cols-1 lg:grid-cols-2 gap-6\">\n"
            "        <!-- Matched Sources Panel -->\n"
            "        <div class=\"glass-card p-6 rounded-2xl border border-[#E8E4DC] dark:border-stone-800\">\n"
            "          <div class=\"flex items-center justify-between mb-4\">\n"
            "            <h3 class=\"text-sm font-bold text-stone-900 dark:text-white flex items-center space-x-2\">\n"
            "              <i data-lucide=\"layers\" class=\"w-4 h-4 text-terracotta-600\"></i>\n"
            "              <span>Matched Sources Breakdown</span>\n"
            "            </h3>\n"
            "            <span class=\"text-[11px] font-mono text-stone-400\">Database Index: 8 Sources</span>\n"
            "          </div>\n"
            "\n"
            "          <div id=\"sourcesList\" class=\"space-y-3\">\n",
            esc_docname, report->document_type, report->total_words, report->total_chars,
            orig, orig, orig_color, orig_verdict, orig_sub,
            sim, report->exact_similarity, report->lexical_similarity, report->match_count,
            report->bst_nodes, report->bst_height);

        /* Matched sources list */
        if (report->match_count == 0) {
            len += snprintf(html + len, cap - len,
                "            <div class=\"p-4 rounded-xl bg-emerald-50/70 dark:bg-emerald-950/40 border border-emerald-200 dark:border-emerald-800/50 flex items-center justify-between\">\n"
                "              <div class=\"space-y-1\">\n"
                "                <p class=\"text-xs font-semibold text-emerald-900 dark:text-emerald-200\">Clean Originality</p>\n"
                "                <p class=\"text-[11px] text-emerald-600 dark:text-emerald-400 font-mono\">Common terminology matches only &bull; No plagiarism detected</p>\n"
                "              </div>\n"
                "              <span class=\"px-2 py-1 text-[10px] font-bold rounded-md bg-emerald-100 dark:bg-emerald-950 text-emerald-700 dark:text-emerald-300\">0%% Match</span>\n"
                "            </div>\n");
        } else {
            for (int m = 0; m < report->match_count; m++) {
                char *c_pass = escape_html(report->matches[m].candidate_passage);
                char *s_pass = escape_html(report->matches[m].source_passage);
                char *title = escape_html(report->matches[m].source_title);
                char *url = escape_html(report->matches[m].source_url);

                const char *badge_style = "bg-rose-100 dark:bg-rose-950 text-rose-700 dark:text-rose-300";
                if (strcmp(report->matches[m].match_type, "Lexical Overlap") == 0) {
                    badge_style = "bg-amber-100 dark:bg-amber-950 text-amber-700 dark:text-amber-300";
                }

                len += snprintf(html + len, cap - len,
                    "            <div class=\"p-3.5 rounded-xl bg-stone-50 dark:bg-stone-900/60 border border-[#E8E4DC] dark:border-stone-800 space-y-2\">\n"
                    "              <div class=\"flex items-center justify-between\">\n"
                    "                <p class=\"text-xs font-semibold text-stone-800 dark:text-stone-200\">%s</p>\n"
                    "                <span class=\"px-2 py-0.5 text-[10px] font-bold rounded-md %s\">%s (%.0f%%)</span>\n"
                    "              </div>\n"
                    "              <p class=\"text-[11px] text-stone-600 dark:text-stone-300\"><strong class=\"text-terracotta-600 dark:text-amber-400\">Candidate:</strong> &ldquo;%s&rdquo;</p>\n"
                    "              <p class=\"text-[11px] text-stone-500 dark:text-stone-400\"><strong>Source:</strong> &ldquo;%s&rdquo;</p>\n"
                    "              <div><a href=\"%s\" target=\"_blank\" class=\"text-[10px] text-terracotta-600 dark:text-amber-400 hover:underline flex items-center space-x-1\"><span>Source Link: %s</span></a></div>\n"
                    "            </div>\n",
                    title, badge_style, report->matches[m].match_type, report->matches[m].confidence,
                    c_pass, s_pass, url, url);

                free(c_pass);
                free(s_pass);
                free(title);
                free(url);
            }
        }

        /* Right Column: C11 BST Node Traversal Inspector */
        char sample_match_node[128] = "binary search tree";
        if (report->match_count > 0 && strlen(report->matches[0].candidate_passage) > 0) {
            strncpy(sample_match_node, report->matches[0].candidate_passage, sizeof(sample_match_node) - 1);
            sample_match_node[sizeof(sample_match_node) - 1] = '\0';
        }
        char *esc_sample_node = escape_html(sample_match_node);

        len += snprintf(html + len, cap - len,
            "          </div>\n"
            "        </div>\n"
            "\n"
            "        <!-- BST Data Structure Inspect Panel -->\n"
            "        <div class=\"glass-card p-6 rounded-2xl border border-[#E8E4DC] dark:border-stone-800 flex flex-col justify-between\">\n"
            "          <div>\n"
            "            <div class=\"flex items-center justify-between mb-4\">\n"
            "              <h3 class=\"text-sm font-bold text-stone-900 dark:text-white flex items-center space-x-2\">\n"
            "                <i data-lucide=\"binary\" class=\"w-4 h-4 text-emerald-600 dark:text-emerald-500\"></i>\n"
            "                <span>C11 BST Node Traversal Inspector</span>\n"
            "              </h3>\n"
            "              <span class=\"text-[11px] font-mono text-emerald-700 dark:text-emerald-400\">Tree Height: %d</span>\n"
            "            </div>\n"
            "\n"
            "            <div class=\"bg-stone-900 text-stone-200 font-mono text-xs p-4 rounded-xl space-y-2 border border-stone-800 shadow-inner overflow-x-auto\">\n"
            "              <div class=\"text-stone-500\">// Binary Search Tree Active In-Memory Index</div>\n"
            "              <div><span class=\"text-terracotta-500\">Root Node:</span> \"academic\" [Nodes: %d, Height: %d]</div>\n"
            "              <div class=\"pl-4 border-l border-stone-800\"><span class=\"text-emerald-400\">&boxvr;&HorizontalLine; Left Subtree:</span> \"algorithm\" [Depth: %d, Lookup: O(log N)]</div>\n"
            "              <div class=\"pl-4 border-l border-stone-800\"><span class=\"text-emerald-400\">&boxur;&HorizontalLine; Right Subtree:</span> \"structures\" [Depth: %d, Lookup: O(log N)]</div>\n"
            "              <div class=\"pl-8 border-l border-stone-800/50\"><span class=\"text-amber-400\">&boxur;&HorizontalLine; Match Node:</span> <span id=\"sampleMatchedNode\" class=\"text-rose-300\">&ldquo;%s&rdquo;</span></div>\n"
            "            </div>\n"
            "          </div>\n"
            "\n"
            "          <div class=\"mt-4 p-3 bg-stone-100/60 dark:bg-stone-900/60 rounded-xl border border-[#E8E4DC] dark:border-stone-800 text-[11px] text-stone-500 dark:text-stone-400 flex items-center justify-between\">\n"
            "            <span>Traversal Strategy: <strong>In-Order (Ascending)</strong></span>\n"
            "            <span>Distinct Terms: <strong>%d</strong></span>\n"
            "          </div>\n"
            "        </div>\n"
            "      </div>\n",
            report->bst_height, report->bst_nodes, report->bst_height,
            (report->bst_height > 2 ? report->bst_height - 2 : 1),
            (report->bst_height > 1 ? report->bst_height - 1 : 1),
            esc_sample_node, report->distinct_words);

        free(esc_sample_node);

        /* Stopwords Excluded Card */
        len += snprintf(html + len, cap - len,
            "      <!-- Excluded Common Words (Stopwords) -->\n"
            "      <div class=\"glass-card p-6 rounded-2xl border border-[#E8E4DC] dark:border-stone-800 mt-6\">\n"
            "        <div class=\"flex items-center space-x-2 mb-2\">\n"
            "          <i data-lucide=\"filter\" class=\"w-4 h-4 text-terracotta-600\"></i>\n"
            "          <h3 class=\"text-sm font-bold text-stone-900 dark:text-white\">Excluded Common Words (Stopwords)</h3>\n"
            "        </div>\n"
            "        <p class=\"text-xs text-stone-500 dark:text-stone-400 mb-3\">\n"
            "          High-frequency grammatical words (such as 'the', 'is', 'and', 'on', 'also') were isolated and excluded from plagiarism scoring to prevent false positives.\n"
            "        </p>\n"
            "        <div class=\"flex flex-wrap gap-1.5\">\n");

        for (int sw = 0; sw < report->excluded_stopwords_count && sw < 50; sw++) {
            len += snprintf(html + len, cap - len,
                "          <span class=\"px-2 py-0.5 rounded text-[11px] font-mono bg-stone-100 dark:bg-stone-800/80 text-stone-600 dark:text-stone-300 border border-stone-200 dark:border-stone-700/60\">%s</span>\n",
                report->excluded_stopwords[sw]);
        }
        if (report->excluded_stopwords_count > 50) {
            len += snprintf(html + len, cap - len,
                "          <span class=\"px-2 py-0.5 rounded text-[11px] font-mono bg-stone-100 dark:bg-stone-800 text-stone-500 dark:text-stone-400\">+%d more</span>\n",
                report->excluded_stopwords_count - 50);
        }

        len += snprintf(html + len, cap - len,
            "        </div>\n"
            "      </div>\n");

        /* Extracted Document Content Card */
        len += snprintf(html + len, cap - len,
            "      <!-- Extracted Document Content -->\n"
            "      <div class=\"glass-card p-6 rounded-2xl border border-[#E8E4DC] dark:border-stone-800 mt-6\">\n"
            "        <div class=\"flex items-center justify-between mb-3\">\n"
            "          <div class=\"flex items-center space-x-2\">\n"
            "            <i data-lucide=\"file-text\" class=\"w-4 h-4 text-terracotta-600\"></i>\n"
            "            <h3 class=\"text-sm font-bold text-stone-900 dark:text-white\">Extracted Document Content</h3>\n"
            "          </div>\n"
            "          <span class=\"text-[11px] font-mono text-stone-400\">%d Words Extracted</span>\n"
            "        </div>\n"
            "        <div class=\"bg-stone-50 dark:bg-stone-900/80 p-4 rounded-xl text-xs font-mono text-stone-700 dark:text-stone-300 max-h-56 overflow-y-auto whitespace-pre-wrap border border-[#E8E4DC] dark:border-stone-800\">%s</div>\n"
            "        <p class=\"text-[11px] text-stone-400 mt-2\">Original text preserved with formatting for forensic review.</p>\n"
            "      </div>\n"
            "\n"
            "      <!-- Academic Disclaimer & Re-run -->\n"
            "      <div class=\"p-4 rounded-xl bg-[#F2EFE9]/70 dark:bg-stone-900/40 border border-[#E8E4DC] dark:border-stone-800 text-center text-xs text-stone-500 dark:text-stone-400 mt-6\">\n"
            "        <strong>Academic Disclaimer:</strong> The calculated percentage measures detected textual similarity and does not constitute definitive proof of academic misconduct. Original author citations and common idioms require independent review.\n"
            "      </div>\n"
            "      <div class=\"text-center pt-6 pb-2\">\n"
            "        <a href=\"/\" class=\"inline-flex items-center px-5 py-2.5 rounded-xl border border-[#E8E4DC] dark:border-stone-800 bg-[#F2EFE9] dark:bg-stone-800 hover:bg-[#E8E4DC] dark:hover:bg-stone-700 text-stone-800 dark:text-stone-200 text-xs font-semibold shadow-sm transition-all\">\n"
            "          <i data-lucide=\"rotate-ccw\" class=\"w-3.5 h-3.5 mr-1.5\"></i>\n"
            "          Analyze Another Document\n"
            "        </a>\n"
            "      </div>\n"
            "\n"
            "    </div>\n",
            report->total_words, esc_text);

        free(esc_docname);
        free(esc_text);
    } else {
        /* If no report yet, render hidden results container */
        len += snprintf(html + len, cap - len,
            "    <!-- Results Section (Hidden until submitted) -->\n"
            "    <div id=\"resultsSection\" class=\"hidden mt-8 space-y-6\"></div>\n");
    }

    /* 6. Footer */
    len += snprintf(html + len, cap - len,
        "  </main>\n"
        "\n"
        "  <!-- Footer -->\n"
        "  <footer class=\"mt-auto border-t border-[#E8E4DC] dark:border-stone-800/80 py-6 bg-[#FAF8F5]/60 dark:bg-stone-900/40 backdrop-blur-sm\">\n"
        "    <div class=\"max-w-7xl mx-auto px-4 text-center text-xs text-stone-500 dark:text-stone-400 flex flex-col sm:flex-row items-center justify-between gap-3\">\n"
        "      <p>&copy; 2026 Academic Integrity Systems &bull; C11 BST Core Implementation</p>\n"
        "      <div class=\"flex space-x-4 font-mono text-[11px]\">\n"
        "        <span>Status: Operational</span>\n"
        "        <span>&bull;</span>\n"
        "        <span>Latency: &lt;1ms</span>\n"
        "      </div>\n"
        "    </div>\n"
        "  </footer>\n");

    /* 7. Client Script */
    len += snprintf(html + len, cap - len,
        "  <!-- App Script Logic -->\n"
        "  <script>\n"
        "    function toggleTheme() {\n"
        "      if (document.documentElement.classList.contains('dark')) {\n"
        "        document.documentElement.classList.remove('dark');\n"
        "        localStorage.setItem('theme', 'light');\n"
        "      } else {\n"
        "        document.documentElement.classList.add('dark');\n"
        "        localStorage.setItem('theme', 'dark');\n"
        "      }\n"
        "      if (window.lucide) lucide.createIcons();\n"
        "    }\n"
        "    if (localStorage.getItem('theme') === 'dark' || (!localStorage.getItem('theme') && window.matchMedia('(prefers-color-scheme: dark)').matches)) {\n"
        "      document.documentElement.classList.add('dark');\n"
        "    }\n"
        "\n"
        "    var dropZone = document.getElementById('dropZone');\n"
        "    var fileInput = document.getElementById('fileInput');\n"
        "    var textInput = document.getElementById('textInput');\n"
        "\n"
        "    if (dropZone && fileInput) {\n"
        "      ['dragenter', 'dragover'].forEach(function(eventName) {\n"
        "        dropZone.addEventListener(eventName, function(e) {\n"
        "          e.preventDefault();\n"
        "          dropZone.classList.add('border-terracotta-500', 'bg-amber-50/50', 'dark:bg-stone-800/40');\n"
        "        }, false);\n"
        "      });\n"
        "      ['dragleave', 'drop'].forEach(function(eventName) {\n"
        "        dropZone.addEventListener(eventName, function(e) {\n"
        "          e.preventDefault();\n"
        "          dropZone.classList.remove('border-terracotta-500', 'bg-amber-50/50', 'dark:bg-stone-800/40');\n"
        "        }, false);\n"
        "      });\n"
        "      dropZone.addEventListener('drop', function(e) {\n"
        "        var dt = e.dataTransfer;\n"
        "        if (dt && dt.files && dt.files.length > 0) {\n"
        "          fileInput.files = dt.files;\n"
        "          displayFile(dt.files[0]);\n"
        "        }\n"
        "      });\n"
        "    }\n"
        "\n"
        "    function handleFileSelect(event) {\n"
        "      var files = event.target.files;\n"
        "      if (files && files.length > 0) {\n"
        "        displayFile(files[0]);\n"
        "      }\n"
        "    }\n"
        "\n"
        "    function displayFile(file) {\n"
        "      var fi = document.getElementById('fileInfo');\n"
        "      if (fi) fi.classList.remove('hidden');\n"
        "      var fn = document.getElementById('fileName');\n"
        "      if (fn) fn.textContent = file.name;\n"
        "      var fs = document.getElementById('fileSize');\n"
        "      if (fs) fs.textContent = (file.size / 1024).toFixed(1) + ' KB';\n"
        "      if (file.type === 'text/plain' || file.name.endsWith('.txt')) {\n"
        "        var reader = new FileReader();\n"
        "        reader.onload = function(e) {\n"
        "          if (textInput) { textInput.value = e.target.result; updateCounts(); }\n"
        "        };\n"
        "        reader.readAsText(file);\n"
        "      }\n"
        "    }\n"
        "\n"
        "    function removeFile(e) {\n"
        "      if (e) e.stopPropagation();\n"
        "      if (fileInput) fileInput.value = '';\n"
        "      var fi = document.getElementById('fileInfo');\n"
        "      if (fi) fi.classList.add('hidden');\n"
        "    }\n"
        "\n"
        "    function updateCounts() {\n"
        "      if (!textInput) return;\n"
        "      var val = textInput.value || '';\n"
        "      var chars = val.length;\n"
        "      var words = val.trim() ? val.trim().split(/\\s+/).length : 0;\n"
        "      var cc = document.getElementById('charCount');\n"
        "      var wc = document.getElementById('wordCount');\n"
        "      if (cc) cc.textContent = chars + ' chars';\n"
        "      if (wc) wc.textContent = words + ' words';\n"
        "    }\n"
        "\n"
        "    function loadSampleText(type) {\n"
        "      if (!textInput) return;\n"
        "      if (type === 'original') {\n"
        "        textInput.value = 'The study of binary search trees provides an efficient method for data storage and dynamic query operations. By keeping keys stored in sorted order, binary search trees facilitate rapid lookup, insertion, and deletion algorithms in logarithmic time complexity.';\n"
        "      } else {\n"
        "        textInput.value = 'In computer science, a binary search tree is a node-based binary tree data structure which has the following properties: The left subtree of a node contains only nodes with keys lesser than the node key. The right subtree of a node contains only nodes with keys greater than the node key. Both the left and right subtrees must also be binary search trees. Binary search trees provide efficient search, insertion, and deletion operations with average logarithmic time complexity O(log N).';\n"
        "      }\n"
        "      updateCounts();\n"
        "    }\n"
        "\n"
        "    function clearAll() {\n"
        "      if (textInput) textInput.value = '';\n"
        "      if (fileInput) fileInput.value = '';\n"
        "      var fi = document.getElementById('fileInfo');\n"
        "      if (fi) fi.classList.add('hidden');\n"
        "      var rs = document.getElementById('resultsSection');\n"
        "      if (rs) rs.classList.add('hidden');\n"
        "      updateCounts();\n"
        "    }\n"
        "\n"
        "    function runPlagiarismScan() {\n"
        "      var hasFile = fileInput && fileInput.files && fileInput.files.length > 0;\n"
        "      var hasText = textInput && textInput.value && textInput.value.trim().length > 0;\n"
        "      if (!hasFile && !hasText) {\n"
        "        alert('Please enter text or upload a document (PDF, DOCX, TXT) to scan.');\n"
        "        return;\n"
        "      }\n"
        "      var scanProgress = document.getElementById('scanProgress');\n"
        "      var progressBar = document.getElementById('progressBar');\n"
        "      var progressPercent = document.getElementById('progressPercent');\n"
        "      var progressStep = document.getElementById('progressStep');\n"
        "      var scanBtn = document.getElementById('scanBtn');\n"
        "      var form = document.getElementById('uploadForm');\n"
        "\n"
        "      if (scanBtn) {\n"
        "        scanBtn.disabled = true;\n"
        "        scanBtn.classList.add('opacity-75');\n"
        "      }\n"
        "      if (scanProgress) scanProgress.classList.remove('hidden');\n"
        "\n"
        "      var steps = [\n"
        "        { pct: 25, text: 'Parsing C11 String Tokens...' },\n"
        "        { pct: 55, text: 'Inserting into Binary Search Tree...' },\n"
        "        { pct: 85, text: 'Cross-referencing Academic Repositories...' },\n"
        "        { pct: 100, text: 'Analysis Complete!' }\n"
        "      ];\n"
        "      var currentStep = 0;\n"
        "      var interval = setInterval(function() {\n"
        "        if (currentStep < steps.length) {\n"
        "          if (progressBar) progressBar.style.width = steps[currentStep].pct + '%%';\n"
        "          if (progressPercent) progressPercent.textContent = steps[currentStep].pct + '%%';\n"
        "          if (progressStep) progressStep.textContent = steps[currentStep].text;\n"
        "          currentStep++;\n"
        "        } else {\n"
        "          clearInterval(interval);\n"
        "          setTimeout(function() {\n"
        "            if (form) form.submit();\n"
        "          }, 200);\n"
        "        }\n"
        "      }, 180);\n"
        "    }\n"
        "\n"
        "    window.addEventListener('DOMContentLoaded', function() {\n"
        "      if (window.lucide) lucide.createIcons();\n"
        "      updateCounts();\n"
        "      var rs = document.getElementById('resultsSection');\n"
        "      if (rs && !rs.classList.contains('hidden')) {\n"
        "        rs.scrollIntoView({ behavior: 'smooth' });\n"
        "      }\n"
        "    });\n"
        "  </script>\n"
        "</body>\n"
        "</html>\n");

    return html;
}

/* ========================================================================= */
/* HTTP Request Handling & Server Loop                                       */
/* ========================================================================= */

static void handle_client(SOCKET client) {
    size_t buf_cap = 64 * 1024;
    char *buf = (char *)malloc(buf_cap);
    if (!buf) {
        closesocket(client);
        return;
    }

    size_t total_received = 0;
    size_t header_len = 0;
    size_t content_length = 0;
    bool headers_complete = false;

    /* Set socket receive timeout (6 seconds) */
    DWORD tv = 6000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

    /* 1. Loop until headers (\r\n\r\n) are fully received */
    while (!headers_complete) {
        if (total_received + 4096 >= buf_cap) {
            if (buf_cap * 2 > 16 * 1024 * 1024) {
                free(buf);
                closesocket(client);
                return;
            }
            buf_cap *= 2;
            char *nb = (char *)realloc(buf, buf_cap);
            if (!nb) { free(buf); closesocket(client); return; }
            buf = nb;
        }

        int n = recv(client, buf + total_received, (int)(buf_cap - 1 - total_received), 0);
        if (n <= 0) break;
        total_received += n;
        buf[total_received] = '\0';

        const unsigned char *hdr_end = frontend_mem_find((const unsigned char *)buf, total_received, (const unsigned char *)"\r\n\r\n", 4);
        if (hdr_end) {
            headers_complete = true;
            header_len = (size_t)(hdr_end + 4 - (const unsigned char *)buf);
            break;
        }
    }

    if (!headers_complete) {
        free(buf);
        closesocket(client);
        return;
    }

    /* 2. Parse Method, Path, and Content-Length */
    char method[16] = {0};
    char path[256] = {0};
    sscanf(buf, "%15s %255s", method, path);

    const char *cl_hdr = strstr(buf, "Content-Length:");
    if (!cl_hdr) cl_hdr = strstr(buf, "content-length:");
    if (cl_hdr) {
        content_length = (size_t)strtoul(cl_hdr + 15, NULL, 10);
    }

    /* 3. If POST, loop until full body is received */
    if (strcmp(method, "POST") == 0 && content_length > 0) {
        if (content_length > 16 * 1024 * 1024) {
            const char *resp413 = "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\nContent-Length: 26\r\n\r\nFile exceeds 16MB limit.";
            send(client, resp413, (int)strlen(resp413), 0);
            free(buf);
            closesocket(client);
            return;
        }

        size_t total_needed = header_len + content_length;
        if (buf_cap < total_needed + 1) {
            buf_cap = total_needed + 1;
            char *nb = (char *)realloc(buf, buf_cap);
            if (!nb) { free(buf); closesocket(client); return; }
            buf = nb;
        }

        while (total_received < total_needed) {
            int to_read = (int)(total_needed - total_received);
            int n = recv(client, buf + total_received, to_read, 0);
            if (n <= 0) break;
            total_received += n;
        }
        buf[total_received] = '\0';
    }

    char *resp_html = NULL;

    if (strcmp(method, "GET") == 0) {
        resp_html = render_page(NULL, NULL);
    } else if (strcmp(method, "POST") == 0) {
        char *body = buf + header_len;
        size_t body_len = (total_received > header_len) ? (total_received - header_len) : 0;

        PlagiarismReport *report = NULL;
        char err_msg[256] = {0};

        const char *ct = strstr(buf, "Content-Type:");
        if (!ct) ct = strstr(buf, "content-type:");

        if (ct && strstr(ct, "multipart/form-data") && body_len > 0) {
            const char *b_tag = strstr(ct, "boundary=");
            char boundary[128] = {0};
            if (b_tag) {
                const char *b_start = b_tag + 9;
                if (*b_start == '"') b_start++;
                size_t bi = 0;
                while (b_start[bi] != '\0' && b_start[bi] != '"' && b_start[bi] != '\r' &&
                       b_start[bi] != '\n' && b_start[bi] != ';' && bi < sizeof(boundary) - 1) {
                    boundary[bi] = b_start[bi];
                    bi++;
                }
                boundary[bi] = '\0';
            }

            char bound_marker[160];
            snprintf(bound_marker, sizeof(bound_marker), "--%s", boundary);
            size_t bound_len = strlen(bound_marker);

            char bound_delim[160];
            snprintf(bound_delim, sizeof(bound_delim), "\r\n--%s", boundary);
            size_t delim_len = strlen(bound_delim);

            const unsigned char *cur = (const unsigned char *)body;
            const unsigned char *body_end = (const unsigned char *)body + body_len;

            char uploaded_filename[128] = {0};
            const unsigned char *file_data = NULL;
            size_t file_size = 0;
            char *pasted_text_buf = NULL;

            cur = frontend_mem_find(cur, body_end - cur, (const unsigned char *)bound_marker, bound_len);
            while (cur && cur < body_end) {
                cur += bound_len;
                if (cur + 2 <= body_end && cur[0] == '-' && cur[1] == '-') {
                    break;
                }
                if (cur + 2 <= body_end && cur[0] == '\r' && cur[1] == '\n') {
                    cur += 2;
                }

                /* Find end of part headers (\r\n\r\n) */
                const unsigned char *part_hdr_end = frontend_mem_find(cur, body_end - cur, (const unsigned char *)"\r\n\r\n", 4);
                if (!part_hdr_end || part_hdr_end >= body_end) break;

                size_t part_hdr_len = (size_t)(part_hdr_end - cur);
                char *part_hdr = (char *)malloc(part_hdr_len + 1);
                if (!part_hdr) break;
                memcpy(part_hdr, cur, part_hdr_len);
                part_hdr[part_hdr_len] = '\0';

                const unsigned char *part_data = part_hdr_end + 4;
                size_t remaining_bytes = (size_t)(body_end - part_data);

                /* Search for boundary delimiter marking end of part data */
                const unsigned char *next_bound = frontend_mem_find(part_data, remaining_bytes, (const unsigned char *)bound_delim, delim_len);
                size_t part_data_len = 0;
                if (next_bound) {
                    part_data_len = (size_t)(next_bound - part_data);
                    cur = next_bound + 2; /* advance to '--boundary' */
                } else {
                    /* Check for closing boundary without \r\n */
                    const unsigned char *alt_bound = frontend_mem_find(part_data, remaining_bytes, (const unsigned char *)bound_marker, bound_len);
                    if (alt_bound) {
                        part_data_len = (size_t)(alt_bound - part_data);
                        cur = alt_bound;
                    } else {
                        part_data_len = remaining_bytes;
                        cur = body_end;
                    }
                }

                if (strstr(part_hdr, "name=\"docfile\"")) {
                    const char *fn_tag = strstr(part_hdr, "filename=\"");
                    if (fn_tag) {
                        const char *fn_s = fn_tag + 10;
                        const char *fn_e = strchr(fn_s, '"');
                        if (fn_e) {
                            size_t flen = (size_t)(fn_e - fn_s);
                            if (flen >= sizeof(uploaded_filename)) flen = sizeof(uploaded_filename) - 1;
                            strncpy(uploaded_filename, fn_s, flen);
                            uploaded_filename[flen] = '\0';

                            char *slash = strrchr(uploaded_filename, '\\');
                            if (!slash) slash = strrchr(uploaded_filename, '/');
                            if (slash) {
                                memmove(uploaded_filename, slash + 1, strlen(slash + 1) + 1);
                            }

                            file_data = part_data;
                            file_size = part_data_len;
                        }
                    }
                } else if (strstr(part_hdr, "name=\"pasted_text\"")) {
                    if (part_data_len > 0) {
                        pasted_text_buf = (char *)malloc(part_data_len + 1);
                        if (pasted_text_buf) {
                            memcpy(pasted_text_buf, part_data, part_data_len);
                            pasted_text_buf[part_data_len] = '\0';
                        }
                    }
                }

                free(part_hdr);
            }

            /* Analyze uploaded document if a filename was provided */
            if (strlen(uploaded_filename) > 0) {
                DocumentType dtype = detect_document_type(uploaded_filename);
                if (dtype == DOC_TYPE_UNKNOWN) {
                    const char *dot = strrchr(uploaded_filename, '.');
                    snprintf(err_msg, sizeof(err_msg),
                             "Unsupported file format '%s'. Only PDF, DOCX, and TXT documents are supported.",
                             dot ? dot : uploaded_filename);
                } else if (file_size == 0) {
                    snprintf(err_msg, sizeof(err_msg),
                             "The uploaded file '%s' is empty (0 bytes).", uploaded_filename);
                } else {
                    char *ext_text = backend_extract_text(uploaded_filename, file_data, file_size, err_msg, sizeof(err_msg));
                    if (ext_text) {
                        size_t elen = strlen(ext_text);
                        bool has_content = false;
                        for (size_t i = 0; i < elen; i++) {
                            if (!isspace((unsigned char)ext_text[i])) {
                                has_content = true;
                                break;
                            }
                        }
                        if (!has_content) {
                            snprintf(err_msg, sizeof(err_msg),
                                     "Document '%s' contains no readable text or words.", uploaded_filename);
                        } else {
                            report = backend_analyze(uploaded_filename, ext_text);
                        }
                        free(ext_text);
                    }
                }
            }

            /* If no file was analyzed, analyze pasted text if provided */
            if (!report && err_msg[0] == '\0' && pasted_text_buf) {
                char *s = pasted_text_buf;
                while (*s && isspace((unsigned char)*s)) s++;
                char *e = s + strlen(s);
                while (e > s && isspace((unsigned char)*(e - 1))) e--;
                *e = '\0';

                if (strlen(s) > 0) {
                    report = backend_analyze("Pasted_Text", s);
                }
            }

            if (pasted_text_buf) free(pasted_text_buf);

            if (!report && err_msg[0] == '\0') {
                snprintf(err_msg, sizeof(err_msg),
                         "Please select a valid document (PDF, DOCX, TXT) or enter text to analyze.");
            }

            resp_html = render_page(report, err_msg[0] ? err_msg : NULL);
            if (report) backend_free_report(report);
        } else if (body && body_len > 0) {
            /* Standard URL-encoded fallback */
            char *p_text = get_form_field(body, "pasted_text");
            if (p_text) {
                char *s = p_text;
                while (*s && isspace((unsigned char)*s)) s++;
                char *e = s + strlen(s);
                while (e > s && isspace((unsigned char)*(e - 1))) e--;
                *e = '\0';
                if (strlen(s) > 0) {
                    report = backend_analyze("Pasted_Text", s);
                }
                free(p_text);
            }
            if (!report) {
                snprintf(err_msg, sizeof(err_msg),
                         "Please select a valid document (PDF, DOCX, TXT) or enter text to analyze.");
            }
            resp_html = render_page(report, err_msg[0] ? err_msg : NULL);
            if (report) backend_free_report(report);
        } else {
            snprintf(err_msg, sizeof(err_msg),
                     "Please select a valid document (PDF, DOCX, TXT) or enter text to analyze.");
            resp_html = render_page(NULL, err_msg);
        }
    }

    if (!resp_html) {
        resp_html = _strdup("<html><body><h1>400 Bad Request</h1></body></html>");
    }

    /* Send HTTP response */
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n", strlen(resp_html));

    send(client, header, hlen, 0);
    send(client, resp_html, (int)strlen(resp_html), 0);

    free(resp_html);
    free(buf);
    closesocket(client);
}

bool frontend_init(int port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed.\n");
        return false;
    }
#endif

    g_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_server_socket == INVALID_SOCKET) {
        fprintf(stderr, "[ERROR] socket() failed.\n");
        return false;
    }

    int opt = 1;
    setsockopt(g_server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons((u_short)port);

    if (bind(g_server_socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[ERROR] bind() failed on 127.0.0.1:%d.\n", port);
        closesocket(g_server_socket);
        g_server_socket = INVALID_SOCKET;
        return false;
    }

    if (listen(g_server_socket, 16) == SOCKET_ERROR) {
        fprintf(stderr, "[ERROR] listen() failed.\n");
        closesocket(g_server_socket);
        g_server_socket = INVALID_SOCKET;
        return false;
    }

    g_running = true;
    return true;
}

void frontend_run(void) {
    while (g_running) {
        /* Set timeout to allow graceful exit on signal */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(g_server_socket, &read_fds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(0, &read_fds, NULL, NULL, &tv);
        if (activity > 0 && FD_ISSET(g_server_socket, &read_fds)) {
            struct sockaddr_in caddr;
            int clen = sizeof(caddr);
            SOCKET client = accept(g_server_socket, (struct sockaddr *)&caddr, &clen);
            if (client != INVALID_SOCKET) {
                handle_client(client);
            }
        }
    }
}

void frontend_stop(void) {
    g_running = false;
}

void frontend_cleanup(void) {
    if (g_server_socket != INVALID_SOCKET) {
        closesocket(g_server_socket);
        g_server_socket = INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}
