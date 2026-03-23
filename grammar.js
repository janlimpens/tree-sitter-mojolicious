module.exports = grammar({
  name: "mojolicious",

  // All whitespace handling is manual — template whitespace is content.
  extras: $ => [],

  externals: $ => [
    $.content,              // HTML text between directives
    $._line_code_start,     // %  (at line start, after optional indent)
    $._line_output_raw_start, // %== (at line start)
    $._line_output_start,   // %=  (at line start, not %==)
    $._line_comment_start,  // %#  (at line start)
    $._line_end,            // \n  (or EOF) after a line directive; resets at_line_start
  ],

  rules: {
    template: $ => repeat(choice(
      $.content,
      // Block directives — order matters: longer sigils first
      $.block_output_raw,
      $.block_output,
      $.block_code,
      $.block_comment,
      // Line directives — scanner distinguishes them
      $.line_output_raw,
      $.line_output,
      $.line_code,
      $.line_comment,
    )),

    // ── Block directives ─────────────────────────────────────────────────────
    // tree-sitter prefers longer string matches, so <%== beats <%= beats <%
    block_output_raw: $ => seq("<%==", optional($._block_body), "%>"),
    block_output:     $ => seq("<%=",  optional($._block_body), "%>"),
    block_code:       $ => seq("<%",   optional($._block_body), "%>"),
    block_comment:    $ => seq("<%#",  optional($._block_body), "%>"),

    // Body: anything that is not %>
    // %[^>] matches a bare % followed by a non-> char (avoids lookahead)
    _block_body: $ => token(/([^%]|%[^>])+/),

    // ── Line directives ──────────────────────────────────────────────────────
    // Scanner emits the start token (consumes leading indent + sigil).
    // line_content catches the rest of the line; _line_end catches the newline
    // and resets the at_line_start flag in scanner state.
    line_output_raw: $ => seq($._line_output_raw_start, optional($.line_content), $._line_end),
    line_output:     $ => seq($._line_output_start,     optional($.line_content), $._line_end),
    line_code:       $ => seq($._line_code_start,       optional($.line_content), $._line_end),
    line_comment:    $ => seq($._line_comment_start,    optional($.line_content), $._line_end),

    line_content: $ => token(/[^\r\n]+/),
  },
});
