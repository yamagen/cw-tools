# emit 0.5.1 DOT number fix

Graphviz DOT does not reliably accept an unquoted negative number written in
scientific notation, for example:

    z=-2.6946750088283494e-05

`emit` 0.5.1 quotes floating-point DOT attribute values (`z`, `cw`, `idf`,
`fontsize`, and `penwidth`). JSON output remains numeric. This changes only DOT
serialization; statistical values and filtering are unchanged.
