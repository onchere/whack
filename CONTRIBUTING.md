TiPs
====
- Development should be guided by/centered around the grammar.
- With the grammar, remember naming something gives you power over it (let the parser combinator do the AST 'decomposition' for you).
- If you encounter left-recursion in your grammar, add a level of indirection by breaking up your grammar rule into smaller parts.
- Grammar file is currently 'locked'. Don't add/subtract unless it's broken.
- Use bottom-up design approach. Avoid unnecessary abstractions.
- For global state during code generation, prefer to use IR metadata.
