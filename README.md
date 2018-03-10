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
  
  - Dissembling the Alpha Lock latching keyswitch to remove the clicker ---
    this causes it to become a momentary switch, just like all the others. That
    allows me to map it in software. It's sad to lose the latch mechanism but I
    need the Ctrl key to go there.

  - The two blank keys on the right of the board are wired in to probe line 8
    (which is normally only used for the DOWN key) in columns P0[5] and P0[6]
    respectively, allowing them to be used as ordinary keys. (This allows the
    top right key to be DELETE.) Normally these keys are brought out to the
    edge connector as raw switches and aren't electrically connected to the
    rest of the board.
