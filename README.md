Schrödinger's Tcl
=================

It was obvious when I read https://groups.google.com/forum/#!topic/comp.lang.tcl/pvd2WJTPerA
that the one thing missing from Tcl was a way of making return values differ
depending on whether you look at them.

So schrotcl demonstrates a way of returning a *different value* from a C command
in Tcl depending on whether Tcl is actually going to process the value in any
way, or whether it's going to go straight to the REPL output!

In short, let's try and make these two calls print different things:
```
% puts [schro]
output1
% schro
output2
```

(obviously this is a terrible idea)

Demo
----

```tcl
$ git clone https://github.com/aidanhs/schrotcl.git
$ cd schrotcl
schrotcl $ make prep # needed once only
[...]
schrotcl $ $(make env) # whenever you open a new terminal
schrotcl $ make # whenever you make changes
[...]
schrotcl $ tclsh8.6
% load schro.so
% set x [schro]
box
% puts [schro]
box
% proc p {} {schro}
% p
box
% puts [p]
box
% eval schro
box
% schro
cat!
```

Implementation
--------------

This is somewhat interesting. You can't use frames to differentiate between
`puts [schro]` and `schro`, as they're both in the same frame. In fact, in the
general case for a programming language, this is impossible - it would depend on
using an oracle to predict what's going to happen to the value you return.

Happily, in Tcl we can cheat! There are three vital points - 1. Tcl
compiles lines passed into the repl to bytecode, 2. Tcl bytecode is a stack
machine and 3. Tcl has a pointer to the current execution location in the
bytecode (note that point 1 only became true in Tcl 8.6 (I believe) so this
code won't work before then).

Using these three pieces of information we can dig up the bytecode and see if
the next thing Tcl does with the value is return it - if so, and we're at the
top level of the stack and there are no recursive evals, we're returning to
something wrapping Tcl - probably the REPL!
