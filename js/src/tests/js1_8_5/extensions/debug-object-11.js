// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Debuggers with enabled hooks should not be GC'd even if they are otherwise
// unreachable.

var g = newGlobal('new-compartment');
var actual = 0;
var expected = 0;

function f() {
    for (var i = 0; i < 20; i++) {
        var dbg = new Debug(g);
        dbg.hooks = {num: i, debuggerHandler: function (stack) { actual += this.num; }};
        expected += i;
    }
}

f();
gc(); gc(); gc();
g.eval("debugger;");
assertEq(actual, expected);

reportCompare(0, 0, 'ok');