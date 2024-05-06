const char* const doc_about =
    R"(In these documents (as well as the ones opened in 'Load file' or 'Clipboard'), you can left-click the rules to load them, or right-click the lines to copy the text (drag to select multiple lines).

This program is for exploring "MAP rules". The project originated from a trivial program I made in 2021, which was poorly written and never made public. Still, I managed to find some interesting discoveries with it (by looking through many, many randomized rules). Here are two of them.

MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
MAP7KV6wLHQiAHIPICBCAhlIqKAhAuKAFBoYmCFEAACIUzbAIAsAsCBJoAANhiIBEBSUICEMQiQFgRBgAJKgAA4gA

The project was then abandoned for many years. Last year I felt an urgency to bring it to completion. Thankfully it's mostly finished now.
)";

const char* const doc_overview =
    R"(At any time, the program has a rule shown in the right plane (which is an editable torus space; the operations are recorded in the tooltips (...)). This is later called the "current rule". As you see, it is the Game-of-Life rule initially.

The MAP-string for the current rule is shown at the top taking up a single line. You can right-click the text to save to the clipboard. The paths in the 'Load file' window can be copied in the same way.
The program keeps the record for the current rule. You can undo/redo via '<| Prev/Next |>' (above the MAP-string). The program manages several sequences of rules in the form of 'First Prev/Next Last'. When a sequence is activated, the left/right arrow keys are bound to 'Prev/Next' for convenience.

In the right plane, you can right-click to select area and press 'C' (no need for 'Ctrl') to save the pattern as RLE-string to the clipboard, and press 'V' to paste the pattern from the clipboard (left-click to decide where to paste).
(Notice that when pasting patterns to white background, you'd need to set 'Background' to 1 in the 'Range operations' window.)

In these documents, as well as those opened in 'Load file' or 'Clipboard', you can left-click the rule-string to replace the current rule, or right-click to copy the lines to the clipboard (drag to select multiple lines).
For example, here is an RLE blob (a "rocket" in the Day & Night rule) - you can firstly click the header line to load the rule, and then copy the following lines (up to the '!' mark, with or without the header line) to paste and see the effect.
x = 7, y = 14, rule = MAPARYBFxZoF34WaBd+aIF+6RZoF35ogX7paIF+6YEX6ZcWaBd+aIF+6WiBfumBF+mXaIF+6YEX6ZeBF+mXF3+Xfw
3bo3b$2b3o2b$b5ob$ob3obo$2b3o2b$2b3o2b$ob3obo$ob3obo$b5ob$b
5ob$3bo3b$7b$2b3o2b$2bobo2b!

The left plane provides ways to analyze and modify the current rule based on a series of subsets. For detailed descriptions see the next section ("Subset, mask and rule operations"). In short:

The subsets that the current rule belongs to will be marked with light-green borders.
To edit the rule, you need to firstly specify a "working set", which is the set you are going to explore. You can select multiple subsets - the program will calculate the intersection of them as the working set. For example, if you select 'All' (isotropic rules; selected by default) and 'S.c.' (self-complementary rules), you are going to explore the rules that are both isotropic and self-complementary.
Then you need to select a "mask" (masking rule) to guide how to observe the current rule and generate new rules.
To modify the current rule:
'Randomize' generates randomized rules in the working set with specified "distance" (number of groups where two rules have different values) to the masking rule.
'<00.. Prev/Next 11..>' generates rules based on the mask and current rule, so that the current rule will iterate through the whole working set - firstly the masking rule, then all rules with distance = 1 to the masking rule, then 2, ..., until max distance.
(The current rule should belong to the working set to enable 'Prev/Next'.)
In the random-access section, the values of the current rule are viewed through the mask and grouped by the working set. By clicking a group you will flip all values in that group. By turning on 'Preview mode' you are able to see the effect without replacing the current rule.

The program also has a way to generate rules ensuring certain value constraints (allowing for certain patterns). For example, in this program it's easy to find rules like this:
MAPARYSZhYAPEgSaBCgCAAAgABAEsAIAIgASIDgAIAAgAASQAIAaACggACAAICAAIAASICogIAAAACAAAAAAAAAAA
See the "Lock and capture" section for details.
)";

const char* const doc_workings =
    R"(This section describes the exact workings of subsets, masks and major rule operations. If you are not familiar with this program, I'd recommend firstly checking the "Rules in different subsets" section to get some sense about what can be found with this program.

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
3. It's natural to define the "distance" between the two rules (in S) as the number of groups where they have different values.
For example, here are some rules that all have distance = 1 (in the isotropic set) to the Game-of-Life rule. In this program it's fairly easy to find such rules.
MAPARYXbhZofOgWaH7oaIDogBZofuhogOiAaIDoAIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAAACAAIAAAAAAAA
MAPARYXfhZofugWan7oaIDogBZofuhogOiAaIDogIgAgAAWaH7oeIDogGiA6ICAAIAAaMDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6YSAAIQAaIDogIAAgACAAIQAAAAAAA

Obviously the whole MAP ruleset can be composed in the same way. And finally, it can be proven that, the intersection (&) of such subsets must be of the same structure (if not empty). Therefore, the above conclusions apply to any combinations of these sets, and in the program you can combine different subsets freely.


With these backgrounds, it will be much clearer to explain what happens in the program:
1. For the selected subsets, the program will firstly calculate their intersection (with the whole MAP set) as the working set W = (M, P).
(If nothing is selected, the working set is the MAP set itself.)
2. Then you need to decide a rule M' (called "mask") to guide how to "observe" the current rule and generate new rules. To allow for further editions, M' must belong to W.
(W.M is immutable, but is exposed as 'Native', so that there is at least one viable mask.)

For the current rule C:
3. 'Randomize' generates randomized rules in W with specified distance to M'.
4. '<00.. Prev/Next 11..>' generates new rules based on C and M', in such a way that C will iterate through all rules in W:
'<00..' sets C to M', and '11..>' sets C to the rule with values different from M' in all cases.
'Next' generates rules based on C and M', so that C will become the "next" rule in such a sequence: starting from M' ('<00..'), then all rules having distance = 1 to M', then distance = 2, ..., until '11..>'. 'Prev' does the same thing reversely.
(Notice that if C already belongs to W, it can serve as a valid mask (M') via '<< Cur'.)

5. In the random-access section, the values of C are viewed through M' (XOR-masked by M') as a sequence of 0/1, and grouped by W.P. If C belongs to W, the masked values in each group must be either all-0 or all-1, so it's enough to represent each group with one of cases in it.
6. By left-clicking a group you will get a rule with all values in that group flipped. Therefore, if C already belongs to W, the result will still belong to W. Otherwise, the operation actually gets rules in (C, W.P). In other words, the operation defines S' = (C, W.P), which is W itself if C already belongs to W.
(The masking rule has no effect on the result of random-access edition.)
(With 'Preview mode' turned on, the program is able to present a "slice" of all rules that have distance = 1 to C in S'.)


Here are some use cases.

If the working set is small enough (having only a few groups), the most direct way to explore the set is to check every rule in it.
Take 'S.c. & Tot(+s)' (the self-complementary and inner-totalistic rules) for example. There are only 5 groups ~ 2^5=32 rules in the set, so it's fairly reasonable to check all of them. Typically, it does not matter which rule serves as the mask if you decide to iterate through the whole working set. However, in this case, neither 'Zero' nor 'Identity' works, so you'd need to stick to the 'Native' mask. By clicking '<00..' you will start from M', which happens to be the "voting" rule:
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w
Then you can click 'Next' to iterate. (For convenience, after clicking '<00..', the left/right arrow keys will be bound to 'Prev/Next'.) The next rule will be:
MAPgAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////g

If the working set is large, then it's infeasible to test all rules. In these cases, aside from getting randomized rules ('Randomize' with arbitrary distance), if there are interesting/promising rules known to belong to the set, you can try to inspect rules that are close to them. Based on the current rule, this can be done in three ways:
1. (After '<< Cur') 'Randomize' with a small distance.
2. (After '<< Cur') 'Next' still works - the iteration will start from the nearest rules (those with distance = 1).
3. (Random-access) 'Preview mode' provides a direct view of all rules with distance = 1 to the current rule.

For example, here is the same rule in the "About this program" section:
MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
It turns out that there exist a lot of amazing rules close to it (in the isotropic set). As a lot of rules involve large oscillators or spaceships, I'd recommend using the 'Next' approach to avoid missing important discoveries. Below is one of the rules with distance = 1 to it.
MAP+sASUIjICkiAgAiAEKBAhrIGFgAUbAAAoAChgnAAAw6AAkAIgKCAlAQgIAAgIQBboCqhEQAAkFQAARIDAQQRBA
Once you find another interesting rule, you can move on starting from it instead. Here is another rule that has distance = 1 to the above one.
x = 5, y = 27, rule = MAP+sASUIjICmiAgAiAEKBAhrIGFiAUbCCAoAChgnAAAw6AAkAIgKCAlAQgIAAgIQBboCqhEQAAkFQAARIDAQQRBA
2ob2o$obobo$b3ob$obobo$2ob2o$5o$5o$5o$5o$5o$5o$5o$5o$5o$5o$
5o$5o$5o$5o$5o$5o$5o$2ob2o$obobo$b3ob$obobo$2ob2o!
(The pattern is to be pasted into white background. To enable this, set 'Background' to 1 in the 'Range operations' window. The pattern will split into two huge spaceships.)

By "wandering" in the working set in this way, you can collect a series of rules that are close from each other.


Sometimes you may also want to jump outside of the predefined subsets. This can be done via random-access edition, and may lead to surprising discoveries. See the "Atypical rules" section for more info.
)";

const char* const doc_rules =
    R"(The following rules are selected from different subsets. You can click a rule and then 'Recognize' to select all the subsets the rule belongs to.
(Notice that 'Recognize' cannot reflect the relations between different subsets.)


---- Rules with native symmetry
Isotropic ('All', selected by default):
MAPgAQFFAQABgAQAFRABgAAAgAAAAAAAAASRAhAggQAAioCFA4AAAAIEBKKzIDQBIAADAQIEEAAkHABAIAAAAAAAQ
MAP/vj70upsmjLqi43n7oQwBt6ogHzgl26yvYiBohgzzqj4/MUi0YS9FLcNSGNF6DlJ50KB3MAzsOAWRNpFDDINBA
MAPBSEBKiGAcMxBVCdvQAH//ySAf8+AAd1aAEE/DwAT728JCDX/DgF9/6VEf34MAX7bAAB3/QkTVX3Mkf57g397Xw
MAPyC6AMHTtIsiwIAEF8IQAkX7sMoHssoEkcRAggcAgFECiqgEAIIQFgyGlAEOlyRkgK4ggBgIggQgWBUh0BYQuSA

'-' alone ('|' (alone) is essentially the same as '-'):
MAPUwUFEETEhAeCBAFowogSBgEAAkAyAAAAggMAgDEFSQoEAgFJBEAQEQjEgACIk4DA0BggAGAEQNGgkMkIEBQRig
MAPIKAkAIAAABAJAAgABFACAoCCoAABIQAAAAAGBAJABIFACEADAAIAAgAAlBVDABAAACEQCBQIQEhAAgEMJCAAEQ

'\' alone ('/' (alone) is essentially the same as '\'):
MAPEUwRQHEGwsIwQQlGQBgAFETB8AEAARBIEmABoA4aKABgjHPAk6gQ5DwAAcQAABKQoGAyZQsCgBggwBAIieYiCQ
MAPQAjggAIIgAAgAAAACQAAAAAAgAAggAAAKIQAAAAAAAAAAAIAAiEgAAgACIAICAEgACAAggAARAAKkAAEAABAAA

'C2' alone:
Notice how "sparse" this rule is:
MAP2AAAAQAAAQCAAAAQAAAAAAAAAAAAAAAAAAAAQAAAAACAAAAAAAACAAAAAAAAAAAAgAAAAAAACAAAAACAAACAAA
MAPAj6qGRYEbAUJCY78vDHdV3RZxOaGCikQ1abIFMMgJBp3BiKINUAoCWLoKJrShCqIYswRCaAJjzpKQhA4KBiQyA

'C4' alone:
'C4' is a strict subset of 'C2'. In 'C4', sometimes (for example, by getting randomized rules) you will find rules where there are complex dynamics, but everything dies finally:
MAPAkMwkQDI20gEBSC4F/gYtzNEmgAVCB0ookwgwMEGAA0FExCAo8gCgFw4ACAqEgALNCnhuUQcmQlgahCx2ACRHg
It's highly likely that there exist rules with interesting oscillators or spaceships nearby. For example, the following rule has (C4) distance = 1 to the above one, but has huge spaceships:
MAPAkMwkwDo20gEBSC4F/gItzNkmkA1iBkookwgwMEGgA0FExCAo8gigFw4AAAqEgALNCnhsUQcmQlgahCx2ACRHg
It will be very helpful for finding such rules if you turn on 'Preview mode' in the random-access plane.

Another pair of examples:
MAPA0wFMBFTd2EGdnFywDNkKEYRDqgbKPiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TvYilLBtImEJIhUoA
MAPA0wFEBFTV2EGdnFywDNkKEYRDigbKHiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TvYilLBtImEJIhUoA

'| & -':
Both '| & -' and '\ & /' (see below) are strict subsets of 'C2' (which is not true reversely - a rule that belongs to 'C2' may not belong to any of '|, -, \ or /').
MAPIAJIChAAUMgAELFhAQCBgIAASWAABCgAAEZCAIIQhAAAAaOBAgApgAAQAAICIRAAAJAYhBwCAAAACWhIQQRIAA
MAP7AKSABAAABLAAwAAAQBgSKACABgQIEISEhCAIKkEAKCIAQAoEQAASIAAAAAAEgAAEuGAAAIEIKAASgAAWAEAAA

'\ & /':
MAPAhghuAgQgywsgCBAwliAgA0RiiAAAOCQgggwwAQABABBCEUgFAgeESlGQEIUAACBAEQCUKCIkWBkAMCRIAABRA
MAPAxEkiDlAgEIC2mcMDTWcNhDDmAcEHBZKjUUNInwAZqgRQggGb5AAtAJqmisCKGlJYJIlOJh5EFnCyBoFohglFA


---- Rules with state symmetry (the self-complementary rules)
Self-complementary rules are supported in the program via 'S.c.'.

There do exist self-complementary rules that allow for spaceships and oscillators. For example, the following rule is found using the 'Lock & Capture' feature in this program:
MAPARcTZhegPEwRdxPuFCBIzyBmF8A8+4g3RMD7A+03nz8DBhNIPyD83RPuIMP8F5n7DO3714g3EXfNw/oXmTcXfw


---- Totalistic rules
Totalistic rules are strict subset of isotropic rules.

Outer-totalistic ('Tot'):
MAPARYAARZoARcWaAEXaIEXfxZoARdogRd/aIEXf4EXf/8WaAEXaIEXf2iBF3+BF3//aIEXf4EXf/+BF3//F3///g
MAPgAAAAQAAARcAAAEXAAEXfwAAARcAARd/AAEXfwEXf/8AAAEXAAEXfwABF38BF3//AAEXfwEXf/8BF3//F3///g

Inner-totalistic ('Tot(+s)'):
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////g

Though conceptually "simpler", totalistic rules have no additional symmetry properties than common isotropic rules. However, it's a good idea to "refine" totalistic rules in larger subsets like isotropic set. Take the voting rule for example:
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w
The following rule has distance = 1 to the voting rule in the isotropic set.
MAPIIAAAYABARcAAQEXARcXf4ABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w


---- Rules emulating hexagonal neighborhood
The subsets at the last line emulate symmetries in the hexagonal tiling. For example, the two kinds of spaceships in the following rule are conceptually the same thing in the corresponding hexagonal rule (in the real hexagonal tiling).
MAPEUQAIiIARGYRRAAiIgBEZgCIVSIAAO6IAIhVIgAA7ogAADPuiABEiAAAM+6IAESIiABmAAAAAACIAGYAAAAAAA

Isotropic ('All', not to be confused with real isotropic rules):
MAPEVWIADNmABERVYgAM2YAERHuABF37gAAEe4AEXfuAAARdwAA7u4RABF3AADu7hEA/6oARMwRIoj/qgBEzBEiiA
MAPEUQRRCLuIhERRBFEIu4iETOqAJlmiBFEM6oAmWaIEURVZgARzIiZIlVmABHMiJki7gCIZgARZpnuAIhmABFmmQ
MAPEURmqiKIzO4RRGaqIojM7iKI/yIAALvuIoj/IgAAu+5EAP/diABE7kQA/92IAETuiAB3IgAARACIAHciAABEAA

'a-d' alone ('q-c' and 'w-x' (alone) are essentially the same as 'a-d'):
MAPIhFmZgCZRAAiEWZmAJlEAAAAqu5ERKoAAACq7kREqgCZEcz/ABFVAJkRzP8AEVUAIqoAIgC7IgAiqgAiALsiAA
MAPAMwARIiIIgAAzABEiIgiAETdEYgAM0QARN0RiAAzRAAiMwAAAHeIACIzAAAAd4gAEWZEAABmAAARZkQAAGYAAA

'a|q' alone ('q|w' and 'w|d' (alone) are essentially the same as 'a|q'):
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

'a-d & q|w':
MAPESIRAESZAGYRIhEARJkAZncARDOZACKIdwBEM5kAIoh3mSJEAABViHeZIkQAAFWI/zOZVVWIMwD/M5lVVYgzAA
You may also try 'q-c & w|d' or 'w-x & a|q'.

'a-d & q-c & w-x':
Both 'a-d & q-c & w-x' and 'a|q & q|w & w|d' (see below) are strict subsets of 'C3'.
MAPiAAAAAAAACKIAAAAAAAAIgAAACIAMwAAAAAAIgAzAAAAERFVAGYAuwAREVUAZgC7ESIiMxFVACIRIiIzEVUAIg

'a|q & q|w & w|d':
MAPAGYRImbuRO4AZhEiZu5E7mYRZpmqAP9VZhFmmaoA/1VmzGb/EQCZM2bMZv8RAJkz7gBmMwCIVe7uAGYzAIhV7g


---- Rules emulating von-Neumann neighborhood
Von-Neumann neighborhood is compatible with native symmetries - you can combine 'Von' with native symmetry subsets directly.

Isotropic ('Von & All'):
MAPzADMAAD/AADMAMwAAP8AADMzAAAzzAAAMzMAADPMAADMAMwAAP8AAMwAzAAA/wAAMzMAADPMAAAzMwAAM8wAAA
MAP/8wz/8wA/wD/zDP/zAD/AMwAzMwAAMwAzADMzAAAzAD/zDP/zAD/AP/MM//MAP8AzADMzAAAzADMAMzMAADMAA

C2 ('Von & C2'):
MAPADMAMwD/M/8AMwAzAP8z/wAzM8z//8zMADMzzP//zMwAMwAzAP8z/wAzADMA/zP/ADMzzP//zMwAMzPM///MzA
MAPzP8zAMwzAADM/zMAzDMAADMzzAD/zAAAMzPMAP/MAADM/zMAzDMAAMz/MwDMMwAAMzPMAP/MAAAzM8wA/8wAAA

Interestingly, there also exist voting rules in von-Neumann and hexagonal neighborhood:
MAPAAAAMwAzM/8AAAAzADMz/wAzM/8z////ADMz/zP///8AAAAzADMz/wAAADMAMzP/ADMz/zP///8AMzP/M////w
MAPAAAAEQAREXcAAAARABERdwAREXcRd3f/ABERdxF3d/8AERF3EXd3/wAREXcRd3f/EXd3/3f///8Rd3f/d////w


---- Misc
You can specify the neighborhood by yourself - 'q/w/e/a/s/d/z/x/c' are for rules whose values are "independent" of the corresponding cell. For example, 'Hex' is just a convenient way to represent 'e & z'.

'q & a & z & -':
As the values are independent of 'q', 'a' and 'z', "infomation" can only pass from right to left:
MAPIjP/7iIz/+4zAN3/MwDd/92Zu//dmbv/Zrvud2a77nciM//uIjP/7jMA3f8zAN3/3Zm7/92Zu/9mu+53Zrvudw
MAPAMwzzADMM8wRqnfuEap37iJm3XciZt13RAD/M0QA/zMAzDPMAMwzzBGqd+4RqnfuImbddyJm3XdEAP8zRAD/Mw

'w & a & d & x & All (isotropic)':
MAPoKAAAKCgAAAFBQAABQUAAKCgAACgoAAABQUAAAUFAAAFBQAABQUAAF9fAABfXwAABQUAAAUFAABfXwAAX18AAA
MAP+vqlpfr6paWgoFVVoKBVVfr6paX6+qWloKBVVaCgVVWgoA8PoKAPDwAA+voAAPr6oKAPD6CgDw8AAPr6AAD6+g
MAP+vqlpfr6paWlpVBQpaVQUPr6paX6+qWlpaVQUKWlUFClpQoKpaUKCl9foKBfX6CgpaUKCqWlCgpfX6CgX1+goA

It's reluctant to regard 's' as "independent of the center cell". However, there are a lot of strange rules in the 's & S.c.' subset.
's & S.c. & /':
MAPBQEFARUeFR4HQQdBHz0fPQcBBwEHBQcFBwEHASUFJQVfW19bfx9/H18fXx9/H38fQwdDB30ffR+HV4dXf19/Xw
MAPAUABQFUVVRUZkRmRNZ41nlEAUQAVFRUVogGiARYRFhF3l3eXf7p/uldXV1f/df91hlOGU3ZndmdXVVdV/X/9fw

's & S.c. & -':
MAPACEAIYCBgIFVVVVVX3dfdxGdEZ0BVQFVd313fUN/Q38BPQE9QRFBEVV/VX9Gd0Z3EQURBVVVVVV+/n7+e/97/w
MAPEgASAAEJAQk1bzVvHVYdVlVFVUUZERkR9hH2EX1/fX8BQQFBd5B3kHdnd2ddVV1VlUeVRwlTCVNvf29//7f/tw


Finally, it's possible to get non-trivial rules that do not belong to any well-defined subsets (no symmetries, no independencies). See the "Atypical rules" section for details.
)";

#if 0
// TODO: finish...
const char* const doc_atypical =
    R"(Typically you will explore rules in the well-defined subsets supported by the program. These subsets, however, take up only an extremely small part of all MAP rules. For example, the largest subset in this program is the native 'C2' rules, which has 272 groups, meaning it takes up only 2^(272-512) ~ 2^-240 of all possible MAP rules.

... and shows some "atypical" use cases of random-access edition......
This section is based on random-access edition, and shows what may be gotten when the edition takes place when the working set is the MAP-set, .......... For sanity....

Here is the same hex-C6 rule shown in the "Rules in different subsets" section.
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ

The following part is based on this rule. Before moving on:
1. Turn on 'Preview mode' in the rule-edition plane.
2. Set a large pace for the preview windows (in 'Settings') and the main window.

Notice that it also belongs to the native 'C2' subset, which is inevitable as hex-C6 is actually a strict subset of native C2. By selecting only native-C2, we bring it to a wider context...
MAPEUQxVSLdM4gRRBFVIt0ziCK7oswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ

By the way, for an arbitrary native-C2 rule, if you do random-access flippings in hex-C6, the result will still belong to native-C2
(... as native-C2.P is a refinement of hex-C6.P, so you will effectively flip (one or) multiple groups of C2 at the same time.) ... The same applies to, for example, random-flipping groups of native-isotropic rules upon C4 rules....


MAPEUQ1VSLdo5gRRBFVItUziCK7oswiCBlEoruizCIAEURVCEQRkYiqIl0AzBEZiKoiRESIKiqRzJHMTACqKpnMEQ
.......
)";
#else
const char* const doc_atypical = "This section is not finished yet :(";
#endif

#if 0
// TODO: rewrite...
const char* const doc_concepts =
    R"(
MAPARYSZxZtPVoUYRG2cMoGoxdsEtJst5ppcpLka9c/q58GKgMUKdi2sWmmEsm0t8kXOp+s8ZJ3edelQ0mXGbeXfw [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]
MAPARYTZxZsPVoQYRH2UMoGoxdkEtIst5p7coLka9c/r78CCgMUKdi+sSGmEsu0t9kXOp+s9ZB3efelQ8mXGTeXfw)";

const char* const doc_IO =
    R"(Some strings will be marked with grey borders when you hover on them. You can right-click on them to copy the text to the clipboard. For example, the current rule (with or without the lock) can be copied this way. The path in the "Load file" is also copyable.

In these documents, you can right-click the lines to copy them (drag to select multiple lines). The line(s) will be displayed as a single piece of copyable string in the popup, then you can right-click that string to copy to the clipboard. As you will see, this document ends with an RLE-pattern blob. ......

There are two recognizable formats that you can left-click directly to set as the current rule:
A MAP rule will be highlighted in green when hovered:
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
A rule-lock pair will be highlighted in yellow when hovered:
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA]

The rules are read on a per-line basis (mainly because it's hard to render the text/locate with cursors cross physical lines etc). If there are more than one rules in a line only the first rule will be recognized. (This might be improved in the future.)

If you left-click a rule-lock pair, both the rule and lock part will be overwritten.
Otherwise (if it's a plain rule), the program will firstly test whether the rule "fits" into the locked part, and will clear the lock if the rule does not fit in. The rule part is assigned normally. This will be useful when you read a list of rules (the first of which is a pair, and the following are plain rules generated from it).
When you click on a rule-lock pair, the working state will be set to it. This makes it easy to know that the following several rules are of the same origin, and you don't have to go back to the original lock if you want to generate new rules based on the lock. If you don't need the lock you can always clear it by the "Clear lock" button.

)";
#endif

// TODO: finish...
#if 0
const char* const doc_lock_and_capture = R"(...

About how the lock is set for plain rules when reading from these documents...

MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA]

---- Pattern capture
Below is a glider that travels "southwest". If you copy the following 3 lines and paste...
x = 12, y = 15
12b$12b$12b$12b$12b$12b$12b$4bo7b$3bo8b$3b3o6b$12b$12b$12b$
12b$12b!

(vs - ; the bounding-box is not enough for capture...)
x = 3, y = 3
bob$o2b$3o!

By pressing 'P'... (about how "capture" works ...)
As a result, you will get:
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]

Selecting self-complementary and isotropic subset...
(About dull-green ring and red ring (totalistic rule has dull green ring but does not work with self-complementary...))

The "Zero" mask does not fit at this time (as it's not a self-complementary rule). As a result, select "Identity" mask...

"Randomize"... though every rule satisfies the lock (which means given the same initial state the capture uses, the result will be the same) this does not mean the captured thing is easy to [emerge] naturally...

Finally we find:
MAPARcSZhehPEwRdxeuNABMzyBsF8BsoYg3RND/A80Xmz8DJhdMPwD03RPuesn8F8n7DM3/04oXEXfNw3oXmbcXfw

It will be much easier to find similar rules based on this rule - you can set the rule as the mask ("Take current"), and set a low distance ... For example, ...
MAPARcTZhegPEwRdxPuFCBIzyBmF8A8+4g3RMD7A+03nz8DBhNIPyD83RPuIMP8F5n7DO3714g3EXfNw/oXmTcXfw

---- Open capture vs closed capture... TODO with examples...

---- Lock-enhancement
Let's go back to this lock.
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]

If you "Enhance" the lock in the isotropic subset, you will get:
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [//Z65r4SvEjQ4JCgABAQgIDRksSLAIwAwKDgAIDAwACKAIICqAKggICAAKCAAKAAiICoiKCIAACAIAAAIAAAAA]

In the isotropic subset the two lock have the same effect - the transformation operations will skip any groups that have locked cases. When you switch to a "wider" subset, however...
For example, if you clear the selected subsets (the largest set - the MAP ruleset itself) you will find the locked groups...
Below are two rules randomly generated in the MAP set
MAPABJSQAQAAgBgQBCghAIEAGAAUMEIAAAEAZIAEAJBEIFCAAAABCCAzQChEgEAAIAAQACCgICAkRCCAFISAoAIBA

MAPARcTZhYBPMgVYRCgAAAAyXBAUsEIYIgCQIrwgIQCgQQCIAIUaEDikAmBGICCQJgBSYGugNgDKACJEABhAlIAAA

---- Static constraints
This is a feature similar to "Capture" to help find still-life based patterns...
)";
#else
const char* const doc_lock_and_capture = "This section is not finished yet :(";
#endif

extern const char* const docs[][2]{{"About this program", doc_about},
                                   {"Overview", doc_overview},
                                   {"Subset, mask and rule operations", doc_workings},
                                   {"Rules in different subsets", doc_rules},
                                   {"Atypical rules", doc_atypical},
                                   {"Lock and capture", doc_lock_and_capture},
                                   {/* null terminator */}};
