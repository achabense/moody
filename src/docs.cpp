// For better clang-format result. Inspired by:
// https://stackoverflow.com/questions/24879417/avoiding-the-first-newline-in-a-c11-raw-string-literal
static constexpr const char* skip_nl(const char* str) {
    while (*str == '\r' || *str == '\n') {
        ++str;
    }
    return str;
}

// TODO: many parts are outdated and need to be rewritten.
// TODO: add a section to record all shortcuts in the program.
// TODO: add a section to explain 'Identify' and 'Paste' with examples.

constexpr const char* doc_about = R"(
This program is for exploring "MAP rules". In short, MAP rules are 2-state rules in the range-1 Moore neighborhood. They are a superset of isotropic rules and life-like rules, the most famous one being Conway's Game of Life.

Here are two examples. The first one is an isotropic rule, while the second rule only satisfies "C4" symmetry (4-fold rotational symmetry).
MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
MAP7KV6wLHQiAHIPICBCAhlIqKAhAuKAFBoYmCFEAACIUzbAIAsAsCBJoAANhiIBEBSUICEMQiQFgRBgAJKgAA4gA

The two rules were found in 2021 along with some other discoveries. They were selected from many random rules generated by a much simpler program I had created at the time, which was poorly written and never made public, and abandoned finally. Years later, I realized I had to bring it to completion, so here it is.
)";

constexpr const char* doc_overview = R"(
At any time, the "current rule" is shown in the right panel (which is an editable torus space). As you see, it is the Game of Life rule initially.

The '<| Prev/Next |>' at the top (not the one in this window) represents the record for the current rule. You can switch to previously tested rules with it. The program manages several sequences of rules all in the form of 'First Prev/Next Last'. When a sequence is activated, the left/right arrow keys will begin to serve as the shortcuts for 'Prev/Next' for convenience.
The MAP-string for the current rule is shown after the sequence. You can right-click the text to copy to the clipboard. The paths in the 'Files' window can be copied in the same way.

In the right panel, you can select area with right button and press 'C' (no need for 'Ctrl') to copy the pattern as RLE-string to the clipboard, or hover on the window and press 'V' to paste the pattern (left-click to decide where to paste). The full list of operations and relevant settings are recorded in the 'Range ops' window.
(When pasting patterns, there are several pasting modes to select in the 'Range ops' window. For example, if the pattern should be pasted in the white background, you'd need to set 'Paste mode' to 'Copy' or 'And'.)

In these documents, as well as those opened in 'Files' or 'Clipboard', you can left-click the rule-string to replace the current rule, or right-click to select lines and left-click (or press 'C') to copy the text.
For example, here is an RLE blob (a "rocket" in the Day & Night rule; should be pasted into black background) - you can firstly click the header line to load the rule, and then copy the following lines (from '3bo...' to the '!' mark) and paste to see the effect.
x = 7, y = 14, rule = MAPARYBFxZoF34WaBd+aIF+6RZoF35ogX7paIF+6YEX6ZcWaBd+aIF+6WiBfumBF+mXaIF+6YEX6ZeBF+mXF3+Xfw
3bo3b$2b3o2b$b5ob$ob3obo$2b3o2b$2b3o2b$ob3obo$ob3obo$b5ob$b
5ob$3bo3b$7b$2b3o2b$2bobo2b!

The left panel provides ways to analyze and update the current rule based on a series of subsets. For detailed descriptions see the next section ('Subset, mask and rule operations'). In short:
The subsets that the current rule belongs to will be marked with light-green borders. For example, the Game of Life rule is an outer-totalistic rule (which also satisfies all symmetries), so every button in the 'Native symmetry' line, and the first button in the next line will be highlighted.
To explore new rules, you need to firstly specify a "working set", which is the set you are going to explore. You can select multiple subsets - the program will calculate the intersection of them as the working set. For example, if you select 'All' (isotropic rules; selected by default) and 'Comp' (self-complementary rules), you are going to explore the rules that are both isotropic and self-complementary. Then you need to select a "mask" (masking rule) to guide how to observe the current rule and generate new rules.
To get new rules:
The 'Traverse' window provides a way to iterate through all rules in the working set (which is not practical if the working set is very large).
The program also has the 'Random' window to generate random rules in the working set.
In the random-access section, the values of the current rule are viewed through the mask and grouped by the working set. By clicking a group you will flip all values of the current rule in that group. By turning on 'Preview' you can see the effect without replacing the current rule.
)";

// clang-format off
// TODO: add back when the related parts are re-implemented...
/*
The program also has a way to generate rules ensuring certain value constraints (allowing for certain patterns). For example, in this program it's easy to find rules like this:
MAPARYSZhYAPEgSaBCgCAAAgABAEsAIAIgASIDgAIAAgAASQAIAaACggACAAICAAIAASICogIAAAACAAAAAAAAAAA
See the 'Lock and capture' section for details.
)";
*/
// clang-format on

// TODO: this part is horrible... reduce the usage of math-styled annotations...
constexpr const char* doc_workings = R"(
This section describes the exact workings of subsets, masks and major rule operations. If you are not familiar with this program, I'd recommend firstly checking the 'Rules in different subsets' section to get some sense about what can be found with this program.

The program works with a series of subsets, each representing certain properties. For example, a rule is isotropic iff it belongs to the isotropic subset ('Native symmetry/All').
These subsets can uniformly be composed in the form of:
S = (M, P) (if not empty), where:
1. M is a MAP rule specified to belong to S, and P is a partition of all cases.
2. A rule R belongs to S iff in each group in P, the values of the rule are either all-the-same or all-the-different from those in M. This can also be defined in terms of XOR-masking operations ~ (R ^ M) is either all-0 or all-1 in each group in P.

For example, an isotropic rule must have the same value in the following group, or in other words, the values for these cases must be either all-the-same or all-the-different from the all-zero rule(00000....).
100 110 011 001 000 000 000 000
100 000 000 001 100 000 001 000
000 000 000 000 100 110 001 011

As a consequence, taking non-empty S = (M, P), there are:
1. If P has k groups, then there are 2^k rules in S.
2. For any two rules in S, in each group in P, the values of the two rules must be either all-the-same or all-the-different from each other, just like their relation with M. As a result, from any rule in S (certainly including M), by flipping all values in some groups of P, you are able to get to any other rules in the set - in this sense, it does not matter which rule serves as M in S.
3. It's natural to define the "distance" between the two rules (in S) as the number of groups where the two rules have different values.
For example, here are some rules that have distance = 1 (in the isotropic set) to the Game of Life rule. In this program it's fairly easy to find such rules.
MAPARYXbhZofOgWaH7oaIDogBZofuhogOiAaIDoAIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAAACAAIAAAAAAAA
MAPARYXfhbofugWaH7oaIDogDZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWan7oaIDogBZofuhogOiAaIDogIgAgAAWaH7oeIDogGiA6ICAAIAAaMDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6YSAAIQAaIDogIAAgACAAIQAAAAAAA

Obviously the entire MAP ruleset can be composed in the same way. And finally, it can be proven that, the intersection (&) of such subsets must be of the same structure (if not empty). Therefore, the above conclusions apply to any combination of these sets, and in the program you can combine different subsets freely.


With these backgrounds, it will be much clearer to explain how things work in the program:
1. For the selected subsets, the program will firstly calculate their intersection (with the entire MAP set) as the working set W = (M, P).
(If nothing is selected, the working set is the MAP set itself.)
2. Then you need to decide another rule M' in W (called "mask") to guide how to "observe" the current rule and generate new rules.
(W.M is immutable, but is exposed as 'Fallback', so that there is at least one viable mask.)
(Also, if the current rule already belongs to W, it can serve as a valid mask (M') by clicking '<< Cur'.)

For the current rule C:
3. The 'Traverse' window lists all rules in W with ascending distances relative to M' - starting from M', then all rules with distance = 1 to it, then 2, 3, ... until the largest distance.
4. The 'Random' window generates random rules in W with specified distance to M'.
5. In the random-access section, the values of C are viewed through M' (XOR-masked by M') as a sequence of 0/1, and grouped by W.P. If C belongs to W, the masked values in each group must be either all-0 or all-1, so it's enough to represent each group with one of cases in it.
(The numbers of groups of W.P, and groups in W.P where the values of C are different(1)/same(0) than M', are shown in the form of 'Groups:k (1:x 0:y)'. 'x' has the same meaning as the distance.)
6. By left-clicking a group you will get a rule with all values in that group flipped. Therefore, if C already belongs to W, the result will still belong to W. Otherwise, the operation actually gets rules in (C, W.P). In other words, the operation defines S' = (C, W.P), which is W itself if C already belongs to W.
(The masking rule has no effect on the result of random-access editing.)
(With 'Preview' turned on, the program is able to present a "slice" of all rules that have distance = 1 to C in S'.)


Here are some use cases.

If the working set is small enough (having only a few groups), the most direct way to explore the set is to check every rule in it.
Take 'Comp & Tot(+s)' (self-complementary & inner-totalistic rules) for example. There are only 5 groups ~ 2^5=32 rules in the set, so it's fairly reasonable to check all of them using the 'Traverse' window. Typically, it does not matter which rule serves as the mask if you decide to iterate through the entire working set. However, in this case, neither 'Zero' nor 'Identity' works. Suppose you are using the 'Fallback' mask, in the traverse window, by clicking '<00..' you will start from M', which happens to be the "voting" rule:
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w
And the next rule should be:
MAPgAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////g

If the working set is large, then it's infeasible to test all rules. In these cases, aside from getting arbitrary random rules (with 'Random' window), if there are interesting/promising rules known to belong to the set, you can try to inspect rules that are close to them. Based on the current rule, this can be done in three ways:
1. (After '<< Cur') The sequence in the 'Traverse' window still works, as it begins from the nearest rules to the current rule.
2. (After '<< Cur') In the 'Random' window, you can get random rules with a small distance to the current rule.
3. In the random-access section, 'Preview' provides a direct view of all rules with distance = 1 to the current rule.

For example, here is the same rule in the 'About this program' section:
MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
It turns out that there exist a lot of amazing rules close to it (in the isotropic set). Below is one of the rules with distance = 1 to it. (As a lot of rules involve large oscillators or spaceships, to avoid missing important discoveries you can press 'X' on the preview windows to enter overriding mode. The full list of preview-window operations is recorded in 'Settings'.)
MAP+sASUIjICkiAgAiAEKBAhrIGFgAUbAAAoAChgnAAAw6AAkAIgKCAlAQgIAAgIQBboCqhEQAAkFQAARIDAQQRBA
Once you find another interesting rule, you can move on starting from it instead. Here is another rule that has distance = 1 to the above one.
x = 5, y = 27, rule = MAP+sASUIjICmiAgAiAEKBAhrIGFiAUbCCAoAChgnAAAw6AAkAIgKCAlAQgIAAgIQBboCqhEQAAkFQAARIDAQQRBA
2ob2o$obobo$b3ob$obobo$2ob2o$5o$5o$5o$5o$5o$5o$5o$5o$5o$5o$
5o$5o$5o$5o$5o$5o$5o$2ob2o$obobo$b3ob$obobo$2ob2o!
(The pattern is to be pasted into white background, and will split into two huge spaceships.)
)";

// clang-format off
// By "wandering" in the working set in this way, you can collect a series of rules that are close from each other.
// Sometimes you may also want to jump outside of the predefined subsets. This can be done via random-access editing, and may lead to surprising discoveries. See the 'More about random-access editing' section for more info.
// clang-format on

// TODO: replace some examples with more interesting ones.
constexpr const char* doc_rules = R"(
The following rules are selected from different subsets. For each preview window you can press 'Z' to see which subsets the rule belongs to; the full list of preview-window operations is recorded in 'Settings'. If you are interested in a specific rule, you can click its rule string for more actions (for example, 'Match' to select related subsets).


@@Symmetric rules

Isotropic ('All', selected by default):
MAPgAQFFAQABgAQAFRABgAAAgAAAAAAAAASRAhAggQAAioCFA4AAAAIEBKKzIDQBIAADAQIEEAAkHABAIAAAAAAAQ
MAP/vj70upsmjLqi43n7oQwBt6ogHzgl26yvYiBohgzzqj4/MUi0YS9FLcNSGNF6DlJ50KB3MAzsOAWRNpFDDINBA
MAPBSEBKiGAcMxBVCdvQAH//ySAf8+AAd1aAEE/DwAT728JCDX/DgF9/6VEf34MAX7bAAB3/QkTVX3Mkf57g397Xw
MAPyC6AMHTtIsiwIAEF8IQAkX7sMoHssoEkcRAggcAgFECiqgEAIIQFgyGlAEOlyRkgK4ggBgIggQgWBUh0BYQuSA

'-' alone ('|' (alone) is similar to '-'):
MAPUwUFEETEhAeCBAFowogSBgEAAkAyAAAAggMAgDEFSQoEAgFJBEAQEQjEgACIk4DA0BggAGAEQNGgkMkIEBQRig
MAPIKAkAIAAABAJAAgABFACAoCCoAABIQAAAAAGBAJABIFACEADAAIAAgAAlBVDABAAACEQCBQIQEhAAgEMJCAAEQ

'\' alone ('/' (alone) is similar to '\'):
MAPEUwRQHEGwsIwQQlGQBgAFETB8AEAARBIEmABoA4aKABgjHPAk6gQ5DwAAcQAABKQoGAyZQsCgBggwBAIieYiCQ
MAPQAjggAIIgAAgAAAACQAAAAAAgAAggAAAKIQAAAAAAAAAAAIAAiEgAAgACIAICAEgACAAggAARAAKkAAEAABAAA

'C2' alone:
Notice how "sparse" this rule is:
MAP2AAAAQAAAQCAAAAQAAAAAAAAAAAAAAAAAAAAQAAAAACAAAAAAAACAAAAAAAAAAAAgAAAAAAACAAAAACAAACAAA
MAPAj6qGRYEbAUJCY78vDHdV3RZxOaGCikQ1abIFMMgJBp3BiKINUAoCWLoKJrShCqIYswRCaAJjzpKQhA4KBiQyA

'C4' alone:
'C4' is a strict subset of 'C2'. Often in 'C4', you will find rules with patterns that exhibit complex dynamics but disappear finally:
MAPAkMwkQDI20gEBSC4F/gYtzNEmgAVCB0ookwgwMEGAA0FExCAo8gCgFw4ACAqEgALNCnhuUQcmQlgahCx2ACRHg
It turns out that, there usually exist rules with interesting oscillators or spaceships close to such rules. For example, the following rule has (C4) distance = 1 to the above one, but has huge spaceships:
MAPAkMwkwDo20gEBSC4F/gItzNkmkA1iBkookwgwMEGgA0FExCAo8gigFw4AAAqEgALNCnhsUQcmQlgahCx2ACRHg
It will be very helpful for finding such rules if you turn on 'Preview' for the random-access section.

Another pair of examples:
MAPA0wFMBFTd2EGdnFywDNkKEYRDqgbKPiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TvYilLBtImEJIhUoA
MAPA0wFEBFTV2EGdnFywDNkKEYRDigbKHiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TvYilLBtImEJIhUoA

'| & -':
Both '| & -' and '\ & /' (see below) are strict subsets of 'C2'. (This is not true reversely - a rule that belongs to 'C2' may not belong to any of '|', '-', '\' or '/'.)
MAPIAJIChAAUMgAELFhAQCBgIAASWAABCgAAEZCAIIQhAAAAaOBAgApgAAQAAICIRAAAJAYhBwCAAAACWhIQQRIAA
MAP7AKSABAAABLAAwAAAQBgSKACABgQIEISEhCAIKkEAKCIAQAoEQAASIAAAAAAEgAAEuGAAAIEIKAASgAAWAEAAA

'\ & /':
MAPAhghuAgQgywsgCBAwliAgA0RiiAAAOCQgggwwAQABABBCEUgFAgeESlGQEIUAACBAEQCUKCIkWBkAMCRIAABRA
MAPAxEkiDlAgEIC2mcMDTWcNhDDmAcEHBZKjUUNInwAZqgRQggGb5AAtAJqmisCKGlJYJIlOJh5EFnCyBoFohglFA


@@Self-complementary rules

Self-complementary rules ('Comp') can work naturally with many other subsets. For example, the following rule is an isotropic self-complementary rule ('All & Comp').
MAPARcTZhegPEwRdxPuFCBIzyBmF8A8+4g3RMD7A+03nz8DBhNIPyD83RPuIMP8F5n7DO3714g3EXfNw/oXmTcXfw
As you see, both black and white "gliders" occur easily in this rule. The rule was found using a feature removed in v0.9.7 (due to some design flaws). The feature will be re-supported in the future, hopefully in the next version.


@@Totalistic rules

Outer-totalistic ('Tot'):
Outer-totalistic rules are a strict subset of isotropic rules.
MAPARYAARZoARcWaAEXaIEXfxZoARdogRd/aIEXf4EXf/8WaAEXaIEXf2iBF3+BF3//aIEXf4EXf/+BF3//F3///g
MAPgAAAAQAAARcAAAEXAAEXfwAAARcAARd/AAEXfwEXf/8AAAEXAAEXfwABF38BF3//AAEXfwEXf/8BF3//F3///g

Inner-totalistic ('Tot(+s)'):
Inner-totalistic rules are further a strict subset of outer-totalistic rules.
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////g

Though conceptually "simpler", totalistic rules have no additional symmetry properties than common isotropic rules. However, it's a good idea to check their neighboring rules in larger sets like isotropic set. Take the voting rule for example:
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w
The following rule has distance = 1 to the voting rule in the isotropic set ('All').
MAPIIAAAYABARcAAQEXARcXf4ABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w
The voting rule is also self-complementary; the following rule has distance = 1 to it in 'All & Comp'.
MAPAAEAAAEBABcAEQEHARcXfwABARcBFxd/AVcXPxd/f/8AAQEXAxcVfwEXF38Xf3//ARcXfx9/d/8X/39///9//w

The program also supports totalistic rules in hexagonal and von-Neumann neighborhoods (see below). Except for outer-totalistic hex rules (which has 14 groups, i.e. 16*1024 rules), they are small enough to be traversed easily. Noticeably, there is a similar voting rule in hexagonal neighborhood (which is an inner-totalistic rule):
MAPAAAAEQAREXcAAAARABERdwAREXcRd3f/ABERdxF3d/8AERF3EXd3/wAREXcRd3f/EXd3/3f///8Rd3f/d////w


@@Rules emulating hexagonal neighborhood

For every rule in the 'Hex' subset, when viewed through hexagonal projection ('6'), the dynamics will behave as if produced by a real hexagonal rule. For convenience, these rules are referred to as hexagonal rules directly.

The subsets at the last line emulate symmetries in the hexagonal space. For example, the following (isotropic) rule has spaceships of different shapes moving in different directions - however, if you hover on the preview window and press '6' to apply the projection, you will find they turn out to be the same thing (with the same shape) in the projected space.
MAPEUQAIiIARGYRRAAiIgBEZgCIVSIAAO6IAIhVIgAA7ogAADPuiABEiAAAM+6IAESIiABmAAAAAACIAGYAAAAAAA
Also, if you identify those spaceships in the right panel (select a spaceship and press 'I'), you will find that they all have the same period.

Isotropic ('All', not to be confused with real isotropic rules):
MAPEVWIADNmABERVYgAM2YAERHuABF37gAAEe4AEXfuAAARdwAA7u4RABF3AADu7hEA/6oARMwRIoj/qgBEzBEiiA
MAPEUQRRCLuIhERRBFEIu4iETOqAJlmiBFEM6oAmWaIEURVZgARzIiZIlVmABHMiJki7gCIZgARZpnuAIhmABFmmQ
MAPEURmqiKIzO4RRGaqIojM7iKI/yIAALvuIoj/IgAAu+5EAP/diABE7kQA/92IAETuiAB3IgAARACIAHciAABEAA

'All & Comp':
Self-complementary hexagonal rules are a relatively small set (13 groups, i.e. 8*1024 rules), and is a possible candidate for 'Traverse'.
MAP7rv/zN1mqpnuu//M3Waqmd1mqpm7zEQi3WaqmbvMRCK73cwiZqqZRLvdzCJmqplEZqqZRMwAIohmqplEzAAiiA
MAPEUQRVSJmMwARRBFVImYzABGqAMwz/1V3EaoAzDP/VXcRVQAzzP+qdxFVADPM/6p3/zOZu1V33Xf/M5m7VXfddw

'a-d' alone ('q-c' and 'w-x' (alone) are similar to 'a-d'):
MAPIhFmZgCZRAAiEWZmAJlEAAAAqu5ERKoAAACq7kREqgCZEcz/ABFVAJkRzP8AEVUAIqoAIgC7IgAiqgAiALsiAA
MAPAMwARIiIIgAAzABEiIgiAETdEYgAM0QARN0RiAAzRAAiMwAAAHeIACIzAAAAd4gAEWZEAABmAAARZkQAAGYAAA

'a|q' alone ('q|w' and 'w|d' (alone) are similar to 'a|q'):
MAPAAAAAKoAiAAAAAAAqgCIAAAAIhEiRBEiAAAiESJEESKIEYgAqgAARIgRiACqAABEqgARIrsiAACqABEiuyIAAA
MAPIncAVUQidwAidwBVRCJ3AO5EAEREAAAR7kQAREQAABEAqgCIAEQidwCqAIgARCJ3RACImSIR/2ZEAIiZIhH/Zg

'C2' alone (not to be confused with native 'C2'; however, notice this is a subset of native 'C2'):
MAPzIgAAIgAACLMiAAAiAAAIkQRZgAAAACIRBFmAAAAAIjuAGYRAAAAAO4AZhEAAAAAABEAETMAVQAAEQARMwBVAA

'C3' alone:
MAPRABVAIgAmd1EAFUAiACZ3YgAqu5VdyIAiACq7lV3IgAAEVURAGaIqgARVREAZoiqESJEZlVEqgARIkRmVUSqAA
MAPqsxEACIRiACqzEQAIhGIAAAAmQAAAACIAACZAAAAAIiIIgAAAABEAIgiAAAAAEQAiEQAACIAAACIRAAAIgAAAA

'C6' alone:
'C6' is a strict subset of both 'C2' and 'C3'.
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ
MAPEVURRDO7IjMRVRFEM7siMzPdEZkAuxHMM90RmQC7EcxERABV/1XdIkREAFX/Vd0iu92qZrvuZoi73apmu+5miA
('C6' is very close to hexagonal isotropy. Only a few groups are different; set the working set as 'All' and superset as 'C6' to see the difference.)

'a-d & q|w' ('q-c & w|d' and 'w-x & a|q' are similar to 'a-d & q|w'):
MAPESIRAESZAGYRIhEARJkAZncARDOZACKIdwBEM5kAIoh3mSJEAABViHeZIkQAAFWI/zOZVVWIMwD/M5lVVYgzAA

'a-d & q-c & w-x':
Both 'a-d & q-c & w-x' and 'a|q & q|w & w|d' (see below) are strict subsets of 'C3'.
MAPiAAAAAAAACKIAAAAAAAAIgAAACIAMwAAAAAAIgAzAAAAERFVAGYAuwAREVUAZgC7ESIiMxFVACIRIiIzEVUAIg

'a|q & q|w & w|d':
MAPAGYRImbuRO4AZhEiZu5E7mYRZpmqAP9VZhFmmaoA/1VmzGb/EQCZM2bMZv8RAJkz7gBmMwCIVe7uAGYzAIhV7g


@@Rules emulating von-Neumann neighborhood

Von-Neumann neighborhood works naturally with native symmetries - you can combine 'Von' with native symmetry subsets directly. (Von-Neumann rules are also a strict subset of 'Hex', so the hexagonal projection ('6') applies to them as well.)

Isotropic ('Von & All'):
MAPzADMAAD/AADMAMwAAP8AADMzAAAzzAAAMzMAADPMAADMAMwAAP8AAMwAzAAA/wAAMzMAADPMAAAzMwAAM8wAAA
MAP/8wz/8wA/wD/zDP/zAD/AMwAzMwAAMwAzADMzAAAzAD/zDP/zAD/AP/MM//MAP8AzADMzAAAzADMAMzMAADMAA

C2 ('Von & C2'):
MAPADMAMwD/M/8AMwAzAP8z/wAzM8z//8zMADMzzP//zMwAMwAzAP8z/wAzADMA/zP/ADMzzP//zMwAMzPM///MzA
MAPzP8zAMwzAADM/zMAzDMAADMzzAD/zAAAMzPMAP/MAADM/zMAzDMAAMz/MwDMMwAAMzPMAP/MAAAzM8wA/8wAAA


@@Miscellaneous

You can specify the neighborhood by yourself - 'q/w/e/a/s/d/z/x/c' are for rules whose values are "independent" of the corresponding cell. For example, 'Von' is actually equal to 'q & e & z & c' (independent of 'q', 'e', 'z' and 'c'), and 'Hex' is equal to 'e & z'.

'q & a & z & -':
As the values are independent of 'q', 'a' and 'z', "infomation" can only pass from right to left:
MAPIjP/7iIz/+4zAN3/MwDd/92Zu//dmbv/Zrvud2a77nciM//uIjP/7jMA3f8zAN3/3Zm7/92Zu/9mu+53Zrvudw
MAPAMwzzADMM8wRqnfuEap37iJm3XciZt13RAD/M0QA/zMAzDPMAMwzzBGqd+4RqnfuImbddyJm3XdEAP8zRAD/Mw

Aside from von-Neumann rules, 'w & a & d & x' works naturally with native symmetries as well.
'w & a & d & x & All (isotropic)':
MAP+vqlpfr6paWlpVBQpaVQUPr6paX6+qWlpaVQUKWlUFClpQoKpaUKCl9foKBfX6CgpaUKCqWlCgpfX6CgX1+goA
MAPBQWgoAUFoKBfXwAAX18AAAUFoKAFBaCgX18AAF9fAABfXwAAX18AAP//AAD//wAAX18AAF9fAAD//wAA//8AAA
MAPBQWgoAUFoKBfXwoKX18KCgUFoKAFBaCgX18KCl9fCgpfX1BQX19QUP//AAD//wAAX19QUF9fUFD//wAA//8AAA

It's reluctant to regard 's' as "independent of the center cell". However, there are a lot of strange rules in the 's & Comp' subset.
's & Comp & /':
MAPBQEFARUeFR4HQQdBHz0fPQcBBwEHBQcFBwEHASUFJQVfW19bfx9/H18fXx9/H38fQwdDB30ffR+HV4dXf19/Xw
MAPAUABQFUVVRUZkRmRNZ41nlEAUQAVFRUVogGiARYRFhF3l3eXf7p/uldXV1f/df91hlOGU3ZndmdXVVdV/X/9fw

's & Comp & -':
MAPACEAIYCBgIFVVVVVX3dfdxGdEZ0BVQFVd313fUN/Q38BPQE9QRFBEVV/VX9Gd0Z3EQURBVVVVVV+/n7+e/97/w
MAPEgASAAEJAQk1bzVvHVYdVlVFVUUZERkR9hH2EX1/fX8BQQFBd5B3kHdnd2ddVV1VlUeVRwlTCVNvf29//7f/tw


Finally, it's possible to get non-trivial rules that do not belong to any pre-defined subset. See the next section ('More about random-access editing') for details.
)";

constexpr const char* doc_random_access = R"(
This section covers more aspects about random-access editing (as explained in 'Subset, mask and rule operations'). To recap, for the current rule C and working set W = (M, P), the operation flips all the values in a group in W.P, and therefore, the result belongs to S' = (C, W.P), which is W itself if C already belongs to W.

For the sets in the form of (M, P), if a set S1 is a subset of another set S2, its partition must be strictly "coarser" than that of S2. In other words, each group in S1.P must fully cover one or several groups in S2.P. For example, the isotropic set is a subset of '-', '|', ..., 'C4' etc, so its partition is strictly coarser than theirs.
(In the program, the working set is a subset of the ones that turn dull-blue, and any set is a subset of the entire MAP set.)

Based on this, when performing random-access editing in W, suppose the current rule belongs to S != W:
1. If S is a strict subset of W (for example, the current rule is totalistic, while W is the isotropic set):
As the rule also belongs to W, the result of random-access editing will belong to W as well (but may no longer belong to S). In this case, the operation can be considered "refining" the rule in a broader context (set). There have been some examples in the previous sections. For example, when talking about "distance", the rules with distance = 1 to the Game of Life rule in the isotropic set are gotten this way.
(This includes editing any rule in the MAP set (which the default one if you select no subset) - the random-access editing will always flip the value for a single case.)

2. If conversely, W is a strict subset of S (for example, W is the isotropic set, while the current rule only belongs to 'C4'):
The result may not belong to W (as the current rule may not belong to W), but must still belong to S. This is because S.P is a refinement of W.P, so flipping a group in W.P has the same effect as flipping one or several groups in S.P.
3. Otherwise, if S and W are irrelevant, you may consider the result as a random MAP rule.


Case 1 is especially useful.

Here is the same hex-C6 rule shown in the 'Rules in different subsets' section. Hex-C6 is a subset of hex-C2, which is a subset of native-C2, which is further a subset of the entire MAP set.
The later rules are all "refined" from this rule. (You can easily find such rules with the help of preview mode.) Before moving on, I'd recommend setting it as the masking rule ('<< Cur'), and setting a large step for the preview windows (in 'Settings') as well as the main window (as the dynamics of these rules are complex and relatively slow).
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ

It turns out that, there are a lot of amazing rules with only native-C2 symmetry close to the C6 rule. Here are two rules that have distance = 1 to it in the native-C2 subset:
MAPEUQxVSLdM4gRRBFVIt0ziCK7oswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ
MAPEUQRVSLdM4gQRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHERIiqIpnMEQ

And the following rules have only a single case different from the C6 rule (distance = 1 in the MAP set; no symmetry):
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHIRIiqIpnMEQ
MAPEUQRVSLdM4gRRBFVMt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ
Just like the symmetric rules, in these rules, the state will converge to irregular-shaped spaceships. However, these complex spaceships do not exist in any symmetric rules.
After all, the subsets supported in the program take up only an extremely small part of all MAP rules - the largest subset in this program is '|' (or '-, \, /'), which has 288 groups, meaning it takes up only 2^(288-512) ~ 2^-224 of all possible MAP rules.
)";

#if 0
constexpr const char* doc_lock_and_capture = R"(
The program has a way to ...
All of the following features are controlled by the 'Lock & capture' tag.

A constraint is a "lock" associated with a MAP rule, that marks some parts of the rule as components... if a rule implies all possible patterns...
The "lock" part is not meaningful on its own.
(format...)

So this represents another category of subsets - ...

Mainly to accommodate for the concept of "capture"...
(Closed-capture) Suppose there is a single spaceship flying in the (torus) space. You know it will roam in the space indefinitely - nothing else will happen, and will invoke only a subset of all cases.... (And if you record the invocations, from a certain point there will no longer be ...).
Any rule that have the same locked values will allow for such a spaceship...

In short, this feature can help you know about the "uniqueness" of a pattern, and generate rules that satisfy the constraint...
(For example ... patterns that covers all cases 512/512)
Suppose there are k groups in the working set, and a capture locks q groups. (the possibility; dependent on the working set)...

When you enable the lock, some parts of the program will behave differently:
The introduction of "lock" introduces a new relation between subsets...
Rule-generation will work as if the working set is intersected with the constraint - the locked groups are skipped: 'Randomize' will now generate ... Traversal...
In the random-access section, the locked groups are ... right-click ....
(About dull-green ring and red ring (totalistic rule has dull green ring but does not work with self-complementary...))

Capture - open vs closed
Closed-capture is useful when the pattern has clear boundary and is foreseeable (spaceships, oscillators and still-life in the pure background)
(bounding-box)
It's meaningless to capture an arbitrary area in the rule with highly-active dynamics...

Below is a glider that travels "southwest".
By pressing 'P'... (about how "capture" works ...)
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]

Open-capture.... (agar-spaceship, spaceship guns etc)
(parity...)

MAPARYSZxZtPVoUYRG2cMoGoxdsEtJst5ppcpLka9c/q58GKgMUKdi2sWmmEsm0t8kXOp+s8ZJ3edelQ0mXGbeXfw [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]
MAPARYTZxZsPVoQYRH2UMoGoxdkEtIst5p7coLka9c/r78CCgMUKdi+sSGmEsu0t9kXOp+s9ZB3efelQ8mXGTeXfw

Selecting self-complementary and isotropic subset...
"Randomize"... though every rule satisfies the constraints, this does not mean the captured thing is easy to emerge naturally...
MAPARcSZhehPEwRdxeuNABMzyBsF8BsoYg3RND/A80Xmz8DJhdMPwD03RPuesn8F8n7DM3/04oXEXfNw3oXmbcXfw
It will be much easier to find similar rules based on existing ones...
MAPARcTZhegPEwRdxPuFCBIzyBmF8A8+4g3RMD7A+03nz8DBhNIPyD83RPuIMP8F5n7DO3714g3EXfNw/oXmTcXfw

Lock-enhancement
(The glider in one direction -> in all directions...) (in the isotropic set, the two constrains have the same effect).... in the MAP set...

"Static constraints"
This is a feature similar to "Capture" to help find still-life patterns...
)";
#else
constexpr const char* doc_lock_and_capture = "This section is not finished yet :(";
#endif

extern constexpr const char* docs[][2]{{"About this program", skip_nl(doc_about)},
                                       {"Overview", skip_nl(doc_overview)},
                                       {"Subset, mask and rule operations", skip_nl(doc_workings)},
                                       {"Rules in different subsets", skip_nl(doc_rules)},
                                       {"More about random-access editing", skip_nl(doc_random_access)},
                                       // {"Lock and capture", skip_nl(doc_lock_and_capture)},
                                       {/* null terminator */}};
