// TODO: add a section to record all operations in the program?

const char* const doc_about =
    R"(In these documents (as well as the ones opened in 'Load file' or 'Clipboard'), you can left-click the rules to load them, or right-click the lines to copy the text (drag to select multiple lines).

This program is for exploring "MAP rules". The project originated from a trivial program I made in 2021, which was poorly written and never made public. Still, I managed to find some interesting discoveries with it (by looking through many, many randomized rules). Here are two of them.

MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
MAP7KV6wLHQiAHIPICBCAhlIqKAhAuKAFBoYmCFEAACIUzbAIAsAsCBJoAANhiIBEBSUICEMQiQFgRBgAJKgAA4gA

The project was then abandoned for many years. Last year I felt an urgency to bring it to completion. Thankfully it's mostly finished now.
)";

// TODO: about MAP rules...
// TODO: about the lock & capture feature...
const char* const doc_overview =
    R"(At any time, the program has a rule shown in the right plane (which is an editable torus space; the operations are recorded in the tooltips (...)). This is later called the "current rule". As you see, it is the Game of Life rule initially.

The MAP-string for the current rule is shown at the top taking up a single line. You can right-click the text to save to the clipboard. The paths in the 'Load file' window can be copied in the same way.
The program keeps the record for the current rule. You can undo/redo via '<| Prev/Next |>' (above the MAP-string). The program manages several sequences of rules in the form of 'First Prev/Next Last'. When a sequence is activated, the left/right arrow keys will be bound to its 'Prev/Next' for convenience.

In the right plane, you can right-click to select area and press 'C' (no need for 'Ctrl') to save the pattern as RLE-string to the clipboard, or press 'V' to paste the pattern from the clipboard (left-click to decide where to paste).
(When pasting patterns to white background, you'd need to set 'Background' to 1 in the 'Range operations' window.)

In these documents, as well as those opened in 'Load file' or 'Clipboard', you can left-click the rule-string to replace the current rule, or right-click to copy the lines to the clipboard (drag to select multiple lines).
For example, here is an RLE blob (a "rocket" in the Day & Night rule) - you can firstly click the header line to load the rule, and then copy the following lines (from '3bo...' up to the '!' mark) to paste and see the effect.
x = 7, y = 14, rule = MAPARYBFxZoF34WaBd+aIF+6RZoF35ogX7paIF+6YEX6ZcWaBd+aIF+6WiBfumBF+mXaIF+6YEX6ZeBF+mXF3+Xfw
3bo3b$2b3o2b$b5ob$ob3obo$2b3o2b$2b3o2b$ob3obo$ob3obo$b5ob$b
5ob$3bo3b$7b$2b3o2b$2bobo2b!

The left plane provides ways to analyze and modify the current rule based on a series of subsets. For detailed descriptions see the next section ('Subset, mask and rule operations'). In short:
The subsets that the current rule belongs to will be marked with light-green borders.
You need to firstly specify a "working set", which is the set you are going to explore. You can select multiple subsets - the program will calculate the intersection of them as the working set. For example, if you select 'All' (isotropic rules; selected by default) and 'S.c.' (self-complementary rules), you are going to explore the rules that are both isotropic and self-complementary.
Then you need to select a "mask" (masking rule) to guide how to observe the current rule and generate new rules.
To modify the current rule:
'Randomize' generates random rules in the working set with specified "distance" (number of groups where two rules have different values) to the masking rule.
'<00.. Prev/Next 11..>' generates rules based on the mask and current rule, so that the current rule will iterate through the whole working set - firstly the masking rule, then all rules with distance = 1 to the masking rule, then 2, ..., until max distance.
(The current rule should belong to the working set to enable 'Prev/Next'.)
In the random-access section, the values of the current rule are viewed through the mask and grouped by the working set. By clicking a group you will flip all values of the current rule in that group. By turning on 'Preview' you are able to see the effect without replacing the current rule.

The program also has a way to generate rules ensuring certain value constraints (allowing for certain patterns). For example, in this program it's easy to find rules like this:
MAPARYSZhYAPEgSaBCgCAAAgABAEsAIAIgASIDgAIAAgAASQAIAaACggACAAICAAIAASICogIAAAACAAAAAAAAAAA
See the 'Lock and capture' section for details.
)";

const char* const doc_workings =
    R"(This section describes the exact workings of subsets, masks and major rule operations. If you are not familiar with this program, I'd recommend firstly checking the 'Rules in different subsets' section to get some sense about what can be found with this program.

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
For example, here are some rules that have distance = 1 (in the isotropic set) to the Game of Life rule. In this program it's fairly easy to find such rules.
MAPARYXbhZofOgWaH7oaIDogBZofuhogOiAaIDoAIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAAACAAIAAAAAAAA
MAPARYXfhbofugWaH7oaIDogDZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWan7oaIDogBZofuhogOiAaIDogIgAgAAWaH7oeIDogGiA6ICAAIAAaMDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6YSAAIQAaIDogIAAgACAAIQAAAAAAA

Obviously the whole MAP ruleset can be composed in the same way. And finally, it can be proven that, the intersection (&) of such subsets must be of the same structure (if not empty). Therefore, the above conclusions apply to any combination of these sets, and in the program you can combine different subsets freely.


With these backgrounds, it will be much clearer to explain what happens in the program:
1. For the selected subsets, the program will firstly calculate their intersection (with the whole MAP set) as the working set W = (M, P).
(If nothing is selected, the working set is the MAP set itself.)
2. Then you need to decide a rule M' in W (called "mask") to guide how to "observe" the current rule and generate new rules.
(W.M is immutable, but is exposed as 'Native', so that there is at least one viable mask.)
(If the current rule already belongs to W, it can serve as a valid mask (M') via '<< Cur'.)

For the current rule C:
3. 'Randomize' generates random rules in W with specified distance to M'.
4. '<00.. Prev/Next 11..>' generates new rules based on C and M', in such a way that C will iterate through all rules in W:
'<00..' sets C to M', and '11..>' sets C to the rule with values different from M' in all cases.
'Next' generates rules based on C and M', so that C will become the "next" rule in such a sequence: starting from M' ('<00..'), then all rules having distance = 1 to M', then distance = 2, ..., until '11..>'. 'Prev' does the same thing reversely.
5. In the random-access section, the values of C are viewed through M' (XOR-masked by M') as a sequence of 0/1, and grouped by W.P. If C belongs to W, the masked values in each group must be either all-0 or all-1, so it's enough to represent each group with one of cases in it.
(The numbers of groups of W.P, and groups in W.P where the values of C are different(1)/same(0) than M', are shown in the form of 'Groups:k (1:x 0:y)'. 'x' has the same meaning as the distance.)
6. By left-clicking a group you will get a rule with all values in that group flipped. Therefore, if C already belongs to W, the result will still belong to W. Otherwise, the operation actually gets rules in (C, W.P). In other words, the operation defines S' = (C, W.P), which is W itself if C already belongs to W.
(The masking rule has no effect on the result of random-access editing.)
(With 'Preview' turned on, the program is able to present a "slice" of all rules that have distance = 1 to C in S'.)


Here are some use cases.

If the working set is small enough (having only a few groups), the most direct way to explore the set is to check every rule in it.
Take 'S.c. & Tot(+s)' (the self-complementary and inner-totalistic rules) for example. There are only 5 groups ~ 2^5=32 rules in the set, so it's fairly reasonable to check all of them. Typically, it does not matter which rule serves as the mask if you decide to iterate through the whole working set. However, in this case, neither 'Zero' nor 'Identity' works, so you'd need to stick to the 'Native' mask. By clicking '<00..' you will start from M', which happens to be the "voting" rule:
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w
Then you can click 'Next' to iterate. (For convenience, after clicking '<00..', the left/right arrow keys will be bound to 'Prev/Next'.) The next rule will be:
MAPgAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////g

If the working set is large, then it's infeasible to test all rules. In these cases, aside from getting random rules ('Randomize' with arbitrary distance), if there are interesting/promising rules known to belong to the set, you can try to inspect rules that are close to them. Based on the current rule, this can be done in three ways:
1. (After '<< Cur') 'Randomize' with a small distance.
2. (After '<< Cur') 'Next' still works - the iteration will start from the nearest rules (those with distance = 1).
3. (Random-access) 'Preview' provides a direct view of all rules with distance = 1 to the current rule.

For example, here is the same rule in the 'About this program' section:
MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
It turns out that there exist a lot of amazing rules close to it (in the isotropic set). As a lot of rules involve large oscillators or spaceships, I'd recommend using the 'Next' approach to avoid missing important discoveries. Below is one of the rules with distance = 1 to it.
MAP+sASUIjICkiAgAiAEKBAhrIGFgAUbAAAoAChgnAAAw6AAkAIgKCAlAQgIAAgIQBboCqhEQAAkFQAARIDAQQRBA
Once you find another interesting rule, you can move on starting from it instead. Here is another rule that has distance = 1 to the above one.
x = 5, y = 27, rule = MAP+sASUIjICmiAgAiAEKBAhrIGFiAUbCCAoAChgnAAAw6AAkAIgKCAlAQgIAAgIQBboCqhEQAAkFQAARIDAQQRBA
2ob2o$obobo$b3ob$obobo$2ob2o$5o$5o$5o$5o$5o$5o$5o$5o$5o$5o$
5o$5o$5o$5o$5o$5o$5o$2ob2o$obobo$b3ob$obobo$2ob2o!
(The pattern is to be pasted into white background. To enable this, set 'Background' to 1 in the 'Range operations' window. The pattern will split into two huge spaceships.)

By "wandering" in the working set in this way, you can collect a series of rules that are close from each other.


Sometimes you may also want to jump outside of the predefined subsets. This can be done via random-access editing, and may lead to surprising discoveries. See the 'More about random-access editing' section for more info.
)";

const char* const doc_rules =
    R"(The following rules are selected from different subsets. You can click a rule and then 'Match' to select all the subsets the rule belongs to.
(Notice that 'Match' cannot reflect the relations between different subsets.)


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
'C4' is a strict subset of 'C2'. In 'C4', sometimes (for example, by getting random rules) you will find rules where there are complex dynamics, but everything dies finally:
MAPAkMwkQDI20gEBSC4F/gYtzNEmgAVCB0ookwgwMEGAA0FExCAo8gCgFw4ACAqEgALNCnhuUQcmQlgahCx2ACRHg
It's highly likely that there exist rules with interesting oscillators or spaceships nearby. For example, the following rule has (C4) distance = 1 to the above one, but has huge spaceships:
MAPAkMwkwDo20gEBSC4F/gItzNkmkA1iBkookwgwMEGgA0FExCAo8gigFw4AAAqEgALNCnhsUQcmQlgahCx2ACRHg
It will be very helpful for finding such rules if you turn on 'Preview' in the random-access plane.

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


Finally, it's possible to get non-trivial rules that do not belong to any well-defined subsets (no symmetries, no independencies). See the next section ('More about random-access editing') for details.
)";

const char* const doc_random_access =
    R"(This section covers more aspects about random-access editing (as explained in 'Subset, mask and rule operations'). To recap, for the current rule C and working set W = (M, P), the operation flips all the values in a group in W.P, and therefore, the result belongs to S' = (C, W.P), which is W itself if C already belongs to W.

For the sets in the form of (M, P), if a set S1 is a subset of another set S2, its partition must be strictly "coarser" than that of S2. In other words, each group in S1.P must fully cover one or several groups in S2.P. For example, the isotropic set is a subset of '-', '|', ..., 'C4' etc, so its partition is strictly coarser than theirs.
(In the program, the working set is a subset of the ones that turn dull-blue, and any set is a subset of the whole MAP set.)

Based on this, when performing random-access editing in W, suppose the current rule belongs to S != W:
1. If S is a strict subset of W (for example, the current rule is totalistic, while W is the isotropic set):
As the rule also belongs to W, the result of random-access editing will belong to W as well (but may no longer belong to S). In this case, the operation can be considered "refining" the rule in a broader context (set). There have been some examples in the previous sections. For example, when talking about "distance", the rules with distance = 1 to the Game of Life rule in the isotropic set are gotten this way.
(This includes editing any rule in the MAP set (which the default one if you select no subset) - the random-access editing will always flip the value for a single case.)

2. If conversely, W is a strict subset of S (for example, W is the isotropic set, while the current rule only belongs to 'C4'):
The result may not belong to W (as the current rule may not belong to W), but must still belong to S. This is because S.P is a refinement of W.P, so flipping a group in W.P has the same effect as flipping one or several groups in S.P.
3. Otherwise, if S and W are irrelevant, you may consider the result as a random MAP rule.


Case 1 is especially useful.

Here is the same hex-C6 rule shown in the 'Rules in different subsets' section. Hex-C6 is a subset of hex-C2, which is a subset of native-C2, which is further a subset of the whole MAP set.
The later rules are all "refined" from this rule. (You can easily find such rules with the help of preview mode.) Before moving on, I'd recommend setting it as the masking rule ('<< Cur'), and setting a large pace for the preview windows (in 'Settings') as well as the main window (as the dynamics of these rules are complex and relatively slow).
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
const char* const doc_lock_and_capture = R"(The program has a way to ...
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
const char* const doc_lock_and_capture = "This section is not finished yet :(";
#endif

extern const char* const docs[][2]{{"About this program", doc_about},
                                   {"Overview", doc_overview},
                                   {"Subset, mask and rule operations", doc_workings},
                                   {"Rules in different subsets", doc_rules},
                                   {"More about random-access editing", doc_random_access},
                                   {"Lock and capture", doc_lock_and_capture},
                                   {/* null terminator */}};
