// Referents of Debug.Scripts in other compartments always survive per-compartment GC.

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var arr = [];
dbg.hooks = {debuggerHandler: function (frame) { arr.push(frame.script); }};
g.eval("for (var i = 0; i < 100; i++) Function('debugger;')();");
assertEq(arr.length, 100);

gc(g);

for (var i = 0; i < arr.length; i++)
    assertEq(arr[i].live, true);  // XXX FIXME replace with something that touches the script

gc();