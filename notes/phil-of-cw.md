## Philosophy

Pruning decisions should remain outside `emit` whenever possible.

A token, annotation, core node, low-frequency item, or isolated node that
appears unnecessary in one analysis may become important evidence in
another. If such decisions are built into `emit`, the software may
silently narrow the range of questions that future researchers can ask.

For this reason, `emit` should primarily convert analytical output into a
representation format. Research-specific selection and exclusion should
remain visible and reversible in the surrounding Unix pipeline.

This separation is not only a software-design choice. It preserves the
possibility of interpretations and research questions that the tool
developer did not anticipate.
