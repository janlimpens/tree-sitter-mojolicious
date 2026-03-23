module.exports = grammar({
  name: "mojolicious",

  externals: $ => [
    $.content, // everything that is not a block directive
  ],

  extras: $ => [],

  rules: {
    template: $ => repeat(choice(
      $.content,
      $.block_output_raw, // <%==  must come before block_output
      $.block_output,     // <%=
      $.block_code,       // <%
      $.block_comment,    // <%#
    )),

    block_output_raw: $ => seq("<%==", optional($._block_body), "%>"),
    block_output:     $ => seq("<%=",  optional($._block_body), "%>"),
    block_code:       $ => seq("<%",   optional($._block_body), "%>"),
    block_comment:    $ => seq("<%#",  optional($._block_body), "%>"),

    // %[^>] handles bare % without lookahead
    _block_body: $ => token(/([^%]|%[^>])+/),
  },
});
