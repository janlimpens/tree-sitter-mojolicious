#include "tree_sitter/parser.h"
#include <stdlib.h>
#include <string.h>

enum TokenType {
    CONTENT,
    LINE_CODE_START,
    LINE_OUTPUT_RAW_START,
    LINE_OUTPUT_START,
    LINE_COMMENT_START,
    LINE_END,
};

typedef struct {
    bool at_line_start;
} Scanner;

void *tree_sitter_mojolicious_external_scanner_create(void) {
    Scanner *s = malloc(sizeof(Scanner));
    s->at_line_start = true; // beginning of file counts as line start
    return s;
}

void tree_sitter_mojolicious_external_scanner_destroy(void *payload) {
    free(payload);
}

unsigned tree_sitter_mojolicious_external_scanner_serialize(void *payload, char *buffer) {
    Scanner *s = payload;
    buffer[0] = s->at_line_start ? 1 : 0;
    return 1;
}

void tree_sitter_mojolicious_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    Scanner *s = payload;
    s->at_line_start = length > 0 && buffer[0] != 0;
}

// Called at the end of a line directive to consume \n and arm at_line_start.
static bool scan_line_end(Scanner *s, TSLexer *lexer) {
    if (lexer->lookahead == '\r') lexer->advance(lexer, false);
    if (lexer->lookahead == '\n') {
        lexer->advance(lexer, false);
        lexer->result_symbol = LINE_END;
        s->at_line_start = true;
        return true;
    }
    // EOF also terminates a line directive
    if (lexer->lookahead == 0) {
        lexer->result_symbol = LINE_END;
        s->at_line_start = true;
        return true;
    }
    return false;
}

// Called when at_line_start is true and lookahead is '%'.
// The content scanner already consumed the leading indent and stopped here,
// so we are positioned exactly at '%'.
static bool scan_line_directive_start(Scanner *s, TSLexer *lexer, const bool *valid_symbols) {
    // lookahead is guaranteed to be '%' by the caller
    lexer->advance(lexer, false); // consume '%'

    // %> is a block-close, not a line directive
    if (lexer->lookahead == '>') return false;

    if (lexer->lookahead == '=') {
        lexer->advance(lexer, false); // consume first '='
        if (lexer->lookahead == '=' && valid_symbols[LINE_OUTPUT_RAW_START]) {
            lexer->advance(lexer, false); // consume second '='
            lexer->result_symbol = LINE_OUTPUT_RAW_START;
            s->at_line_start = false;
            return true;
        }
        if (valid_symbols[LINE_OUTPUT_START]) {
            lexer->result_symbol = LINE_OUTPUT_START;
            s->at_line_start = false;
            return true;
        }
        return false;
    }

    if (lexer->lookahead == '#' && valid_symbols[LINE_COMMENT_START]) {
        lexer->advance(lexer, false);
        lexer->result_symbol = LINE_COMMENT_START;
        s->at_line_start = false;
        return true;
    }

    if (valid_symbols[LINE_CODE_START]) {
        lexer->result_symbol = LINE_CODE_START;
        s->at_line_start = false;
        return true;
    }

    return false;
}

// Consumes raw HTML content until:
//   - '<' followed by '%'  (block directive start)
//   - '\n' followed by optional indent followed by '%'  (line directive start)
// In the second case the '\n' and indent are included in the content token so
// that the HTML injection sees properly indented text; the scanner then sets
// at_line_start = true so the line-directive scanner can pick up the '%'.
static bool scan_content(Scanner *s, TSLexer *lexer) {
    // If we are at line start and the very next char is '%', yield to the
    // line-directive scanner — do not eat it as content.
    if (s->at_line_start && lexer->lookahead == '%') return false;

    bool has_content = false;

    while (lexer->lookahead != 0) {
        if (lexer->lookahead == '<') {
            // Peek: if followed by '%' this is a block-directive start.
            // Mark the end BEFORE '<' so the content token stops here.
            lexer->mark_end(lexer);
            lexer->advance(lexer, false);
            if (lexer->lookahead == '%') {
                if (!has_content) return false;
                lexer->result_symbol = CONTENT;
                return true;
            }
            // Not a directive — '<' is ordinary content.
            has_content = true;
            s->at_line_start = false;
            continue;
        }

        if (lexer->lookahead == '\n') {
            lexer->advance(lexer, false);
            has_content = true;

            // Consume any leading whitespace / indent on the next line.
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                lexer->advance(lexer, false);
            }

            if (lexer->lookahead == '%') {
                // The indent is part of this content token; stop before '%'.
                lexer->mark_end(lexer);
                s->at_line_start = true;
                lexer->result_symbol = CONTENT;
                return true;
            }

            // Regular content continues; update mark to include the indent.
            lexer->mark_end(lexer);
            s->at_line_start = false;
            continue;
        }

        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        has_content = true;
        s->at_line_start = false;
    }

    if (has_content) {
        lexer->result_symbol = CONTENT;
        return true;
    }
    return false;
}

bool tree_sitter_mojolicious_external_scanner_scan(
    void *payload,
    TSLexer *lexer,
    const bool *valid_symbols
) {
    Scanner *s = payload;

    // LINE_END is only valid inside a line directive; handle it first.
    if (valid_symbols[LINE_END]) {
        return scan_line_end(s, lexer);
    }

    // Line directive start: only when we are at line start and see '%'.
    if (s->at_line_start && lexer->lookahead == '%') {
        bool any_valid =
            valid_symbols[LINE_CODE_START] ||
            valid_symbols[LINE_OUTPUT_START] ||
            valid_symbols[LINE_OUTPUT_RAW_START] ||
            valid_symbols[LINE_COMMENT_START];
        if (any_valid) {
            return scan_line_directive_start(s, lexer, valid_symbols);
        }
    }

    if (valid_symbols[CONTENT]) {
        return scan_content(s, lexer);
    }

    return false;
}
