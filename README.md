maxii-keyboard
==============


What
----

This is a keyboard firmware package wot I hacked together to make a keyboard
from a Zurich junk shop work.

I had to make a number of modifications to the board, which means this probably
won't be of use to anyone else except as an example. Roughly, these are:

  - Removal of C9 from the CY8CKIT-059 dev board --- this was causing undue
    stray capacitance in P0[4]. It doesn't seem to be possible to disable it in
    software.
  