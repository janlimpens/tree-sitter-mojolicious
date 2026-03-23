#include "tree_sitter/parser.h"

enum TokenType {
    CONTENT,
};

void *tree_sitter_mojolicious_external_scanner_create(void) { return NULL; }
void tree_sitter_mojolicious_external_scanner_destroy(void *p) { (void)p; }
unsigned tree_sitter_mojolicious_external_scanner_serialize(void *p, char *b) { (void)p; (void)b; return 0; }
void tree_sitter_mojolicious_external_scanner_deserialize(void *p, const char *b, unsigned n) { (void)p; (void)b; (void)n; }

bool tree_sitter_mojolicious_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    (void)payload;
    if (!valid_symbols[CONTENT]) return false;

    bool has_content = false;

    while (lexer->lookahead != 0) {
        if (lexer->lookahead == '<') {
            // Stop before <% (block directive start); include bare < in content.
            lexer->mark_end(lexer);
            lexer->advance(lexer, false);
            if (lexer->lookahead == '%') {
                if (!has_content) return false;
                lexer->result_symbol = CONTENT;
                return true;
            }
            has_content = true;
            continue;
        }
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        has_content = true;
    }

    if (has_content) {
        lexer->result_symbol = CONTENT;
        return true;
    }
    return false;
}
