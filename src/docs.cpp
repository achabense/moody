// TODO: simplest way to use this program... (randomize, and saving the rules...)

const char* const doc_about =
    R"(In these documents (as well as the ones opened via 'Load file' or 'Clipboard'), you can left-click the rules to set them to... and right-click the lines to copy the text (drag to select multiple lines). For more details see the "Program I/O" section.

...
MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
MAP7KV6wLHQiAHIPICBCAhlIqKAhAuKAFBoYmCFEAACIUzbAIAsAsCBJoAANhiIBEBSUICEMQiQFgRBgAJKgAA4gA)";

const char* const doc_overview =
    R"(TODO: about I/O...
Many additional operations (shortcuts etc) are recorded in the tooltips for (?) and (...).

At any time, the program has a rule shown in the right plane (which an editable torus space). This is called the "current rule" in the program. As you see, it's the Game-of-Life rule initially.

The current rule is shown as a MAP-string taking up a single line in the topmost section, which can be copied to the clipboard by right-clicking the text. The paths in the 'Load file' window are copyable in the same way.

The current rule can be replaced by loading from these documents (as well as the ones opened via 'Load file' or 'Clipboard'), or "edited" in the left plane...
The trace for the current rule are recorded for undoing/redoing, the functionality being provided via '<| Prev/Next |>'. The program manages several sequences in the same form. When any of them is activated, the left/right arrow keys are bound to it for convenience.)

In the right plane, you can select area and 'C' (no need for ctrl) to save the pattern as RLE-string to the clipboard, and 'V' to paste from the clipboard (click in the space to decide where to paste).
(Notice that when copying ... (white background))...

In these documents, as well as those opened in ...., you can lift-click the rule-string ....
........
For example, here is an RLE blob (which is a "rocket" in the Day & Night rule). You can firstly click the rule to set the current rule to it, and then copy the following lines (up to the '!' mark, with or without the header line (x = ...))
x = 7, y = 14, rule = MAPARYBFxZoF34WaBd+aIF+6RZoF35ogX7paIF+6YEX6ZcWaBd+aIF+6WiBfumBF+mXaIF+6YEX6ZeBF+mXF3+Xfw
3bo3b$2b3o2b$b5ob$ob3obo$2b3o2b$2b3o2b$ob3obo$ob3obo$b5ob$b
5ob$3bo3b$7b$2b3o2b$2bobo2b!

The left plane provides ways to analyze and modify the current rule...
The subsets the current rule belongs to are marked with light-green borders. You can select multiple sets, which means you are going to explore the rules in it...
mask...
...
Preview mode...
Notice that to save the rule you always need to firstly ...
For detailed descriptions see the next section ("Subset, mask and rule operations").

The program also has a ... see the .... (lock-cap)
)";

const char* const doc_workings =
    R"(This section describes the exact workings of subsets, masks and major rule operations.

In this program, a MAP rule is located by a series of subsets. These subsets can uniformly be composed in the form of:
S = (M, P) (if not empty), where:
1. M is a MAP rule specified to belong to S, and P is a partition of all cases.
2. A rule R belongs to S iff in each group in P, the values of the rule are either all-the-same or all-the-different from those in M.
'2.' can also be defined in terms of XOR operations ~ (R ^ M) has either all 0 or all 1 in each group in P.

For example, an isotropic rule must be either all-0 or all-1 in the following group, or in other words, the values for these cases must be either all-the-same or all-the-different from the all-zero rule(00000....).
100 110 011 001 000 000 000 000
100 000 000 001 100 000 001 000
000 000 000 000 100 110 001 011

As a consequence, taking non-empty S = (M, P), there are:
>> If P has k groups, then there are 2^k rules in S.
>> For any two rules in S, in each group in P, the values of the two rules must be either all-the-same or all-the-different from each other, just like their relation with M. As a result, from any rule in S (certainly including M), by flipping all values in some groups of P, you are able to get to any other rules in the set - in this sense, it does not matter which rule serves as M in S. Conversely, if rule R does not belong to S, then by flipping the values you essentially get rules in S' = (R, S.P).
>> It's natural to define the "distance" between the two rules (in S) as the number of groups where they have different values.
For example, here are some rules that all have distance = 1 (in the isotropic set) to the Game-of-Life rule. In this program it's fairly easy to find such rules.
MAPARYXbhZofOgWaH7oaIDogBZofuhogOiAaIDoAIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAAACAAIAAAAAAAA
MAPARYXfhZofugWan7oaIDogBZofuhogOiAaIDogIgAgAAWaH7oeIDogGiA6ICAAIAAaMDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6YSAAIQAaIDogIAAgACAAIQAAAAAAA

Obviously the whole MAP ruleset can be composed in the same way. And finally, it can be proven that, the intersection of such subsets must be of the same structure (if not empty). Therefore, the above conclusions apply to any combinations of these sets, and in the program you can combine different subsets freely.

With these backgrounds, it will be much clearer to explain what happens in the program:
0. For the selected subsets, the program calculates their intersection (with the whole MAP set) as the working set W = (M, P).
1. Then you need to decide a mask M' to actively measure the rules. To allow for further editions, M' must belong to W.
W.M is immutable but is exposed as 'Native', so that there is at least a viable rule in the set.
2. The current rule C is XOR-masked by the M', into a sequence of 0/1. The masked values are presented in the random-access section.... The rule belongs to the working set (in other words, every selected subset) iff, the masked values has ......
3. '<00.. Prev/Next 11..>' generates new rules based on the relation of C and M', in such a way that:
'<00..' sets the current rule to M', and '11..>' sets the current rule to the one ...
By 'Next', the current rule will be ...... 'Prev' does the same thing with exactly the reverse order.
4. 'Randomize' generates randomized rules in W with specified distance to M'.
5. The random-access section displays the masked values...
By left-clicking the button you get a rule with each value in the group flipped. As a result, if the current rule C already belongs to W, the result will still belong to W. Otherwise, the operation essentially gets rules in (C, W.P).
As a result, the result is only dependent on the partition of the working set...
... The design is intentional...

TODO: move to another section?
If the working set is small enough (it has only a few groups), the most direct way to explore the set is simply to check every rule in it.

For example, try selecting both 'S.c.' and 'Tot(+s)' (the self-complementary and inner-totalistic rules). In this case, both 'Zero' and 'Identity' do not work, so you may need to select the 'Native' mask.
There are only 5 groups ~ 2^5=32 rules in the set, so it's fairly reasonable to check all of them. By clicking '<00..' you start from M', which happens to be the "voting" rule (for reference, see: https://conwaylife.com/wiki/OCA:Vote).
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w

If the working set is large, then it becomes infeasible to test all rules. Typically, we can:
1. Try randomized rules. (If the W.P has k groups, then the more close the specified distance is to k/2, the more likely the result will be unrelated to the masking rule.)
2. If there are rules (the current rule) in the set known to be special/promising, we can check rules that are close to it.
2.1. We can set the mask to the current rule ('<< Cur'), then try 'Randomize' with a small distance, or we can also do 'Prev/Next' - the whole-set traversal starts with the rules that are closest to the masking rule (distance = 1), meaning that ...
2.2. By turning on the 'Preview mode' for the random-access section, we can test rules that...

Sometimes we may also want to jump outside of the predefined subsets. This can be done via random-access edition, and may lead to surprising discoveries. See the "Atypical rules" section for more info.)";

#if 0
const char* const doc_concepts =
    R"(
A lock is an array associated with a MAP rule, that marks some parts of the rule as contributor for something to happen.
Essentially this defines another kind of subsets R[L] - a MAP rule "is compatible with" (belongs to) it iff the rule has the same value as R for any "locked" case.

You can find a lot of interesting rules without "lock". However, some rules are unlikely discoverable by chance without this feature. Here is an example - an isotropic and self-complementary rule where gliders occur naturally:
MAPARYSZxZtPVoUYRG2cMoGoxdsEtJst5ppcpLka9c/q58GKgMUKdi2sWmmEsm0t8kXOp+s8ZJ3edelQ0mXGbeXfw [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]
MAPARYTZxZsPVoQYRH2UMoGoxdkEtIst5p7coLka9c/r78CCgMUKdi+sSGmEsu0t9kXOp+s9ZB3efelQ8mXGTeXfw

You will learn about how to find rules like this in the "Lock and capture" section.)";
#endif

const char* const doc_subsets =
    R"(The following rules are selected from different subsets. You can click the 'Recognize' button to select the subsets they belong to.
The best way to get familiar with a subset is to generate randomized rules in it. As to the relation between the subsets, .............


--- None
This is typically what you will get in the default density:
MAPYaxTu9YJm9UZsagD9KrzcclQXH5nLwLTGALPMYhZeR6QeYRX6y7WoAw4DDpCQnTjY7k71qW7iQtvjMBxLGNBBg

MAPBRAACkiAAUiATAhmCQkIEYMBgCBDCBFHUqQgJUQAAEAiAAmANgIkRAAISUA0ADgAAZAQAmAMFTCJgAAAYEYEgA

It's possible to get non-trivial rules that do not belong to any well-defined subsets. See the "Atypical rules" section for details.

--- Rules with native symmetry
...............


Isotropic ('All'):
MAPgAQFFAQABgAQAFRABgAAAgAAAAAAAAASRAhAggQAAioCFA4AAAAIEBKKzIDQBIAADAQIEEAAkHABAIAAAAAAAQ
MAP/vj70upsmjLqi43n7oQwBt6ogHzgl26yvYiBohgzzqj4/MUi0YS9FLcNSGNF6DlJ50KB3MAzsOAWRNpFDDINBA


'-' alone ('|' is essentially the same as '-'):
MAPQCAkAAAAABCJAAgABFACAgCCoAABIQAAAAAGBAJABIFACEADAAIAAgAAlBVDABAAACEQCBQIQEhAAgEMJCAAEQ
MAPIKAkAIAAABAJAAgABFACAoCCoAABIQAAAAAGBAJABIFACEADAAIAAgAAlBVDABAAACEQCBQIQEhAAgEMJCAAEQ


'\' alone ('/' is essentially the same as '\'):
MAPEUwRQHEGwsIwQQlGQBgAFETB8AEAARBIEmABoA4aKABgjHPAk6gQ5DwAAcQAABKQoGAyZQsCgBggwBAIieYiCQ
MAPQAjggAIIgAAgAAAACQAAAAAAgAAggAAAKIQAAAAAAAAAAAIAAiEgAAgACIAICAEgACAAggAARAAKkAAEAABAAA


'| & -':
MAPIAJIChAAUMgAELFhAQCBgIAASWAABCgAAEZCAIIQhAAAAaOBAgApgAAQAAICIRAAAJAYhBwCAAAACWhIQQRIAA
MAP7AKSABAAABLAAwAAAQBgSKACABgQIEISEhCAIKkEAKCIAQAoEQAASIAAAAAAEgAAEuGAAAIEIKAASgAAWAEAAA


'\ & /':
MAPAhghuAgQgywsgCBAwliAgA0RiiAAAOCQgggwwAQABABBCEUgFAgeESlGQEIUAACBAEQCUKCIkWBkAMCRIAABRA
MAPAhEkiDkAgEICmmcMDTWcNgDDmAcEHBZKjUUNInwAZqgRQggGZxAAtAJqmisCKGlJQJIlOJh5EFlCyBoFohglFA

Both '| & -' and '\ & /' belong to the 'C2' subset (which is NOT true reversely - a rule that belongs to 'C2' may not belong to any of '|, -, \ or /').


'C2' alone:
Notice how "sparse" this rule is:
MAP2AAAAQAAAQCAAAAQAAAAAAAAAAAAAAAAAAAAQAAAAACAAAAAAAACAAAAAAAAAAAAgAAAAAAACAAAAACAAACAAA
MAPAjaoGxYMbAUJCY78PLH9F3RZ5M6GChkQ1aZIFMMgpBp3BiKINUAoUWLoKJrChCqIYsgRKaAJj3pKQhA4KBiQyA


'C4' alone:
'C4' is a strict subset of 'C2'. In 'C4', oftentimes (for example, by getting randomized rules) you will find rules with complex dynamics but where everything dies finally:
MAPAkMwkQDI20gEBSC4F/gYtzNEmgAVCB0ookwgwMEGAA0FExCAo8gCgFw4ACAqEgALNCnhuUQcmQlgahCx2ACRHg
It's highly likely that there exist rules with interesting oscillators or spaceships close such rules. For example, the following rule has (C4) distance = 1 to the above one, but has huge spaceships:
MAPAkMwkwDo20gEBSC4F/gItzNkmkA1iBkookwgwMEGgA0FExCAo8gigFw4AAAqEgALNCnhsUQcmQlgahCx2ACRHg

Another pair of example:
MAPA0wFMBFTd2EGdnFywDNkKEYRDqgbKPiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TvYilLBtImEJIhUoA
MAPA0wFEBFTV2EGdnFywDNkKEYRDigbKHiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TvYilLBtImEJIhUoA

--- Rules with state symmetry (the self-complementary rules)
('S.c.' ...)
Spaceships and oscillators do exists ... (found using the 'Lock & Capture' feature in this program)...
MAPARcTZhegPEwRdxPuFCBIzyBmF8A8+4g3RMD7A+03nz8DBhNIPyD83RPuIMP8F5n7DO3714g3EXfNw/oXmTcXfw

MAPgAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////g


/////////////////////////////////////////////
(Outer-) Totalistic:
Aah! The Game-of-Life rule.
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA



--- Native symmetry, but in Von-Neumann neighborhood

Isotropic:
MAPzAAzzAAzzMzMADPMADPMzAAz/wAz/wAzADP/ADP/ADPMADPMADPMzMwAM8wAM8zMADP/ADP/ADMAM/8AM/8AMw
MAPAAD/zAAzzP8AAP/MADPM/wAz/zMz/zP/ADP/MzP/M/8AAP/MADPM/wAA/8wAM8z/ADP/MzP/M/8AM/8zM/8z/w

--- Hexagonal neighborhood

a-d & q-c & w-x:
MAPiAAAAAAAACKIAAAAAAAAIgAAACIAMwAAAAAAIgAzAAAAERFVAGYAuwAREVUAZgC7ESIiMxFVACIRIiIzEVUAIg

a|q & q|w & w|d:
MAPZoiZZogAZohmiJlmiABmiIgAd6oAAJnMiAB3qgAAmcyIAHeZAADMqogAd5kAAMyqAACIIgAARAAAAIgiAABEAA

C2:

C3:

C6:
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ
This is an amazing rule.

MAP7ohmmYgAmWbuiGaZiACZZogAmWYAEWaIiACZZgARZoiIAJlmABFmiIgAmWYAEWaIABFmiBFmiAAAEWaIEWaIAA


MAPAAAAEQAREXcAAAARABERdwAREXcRd3f/ABERdxF3d/8AERF3EXd3/wAREXcRd3f/EXd3/3f///8Rd3f/d////w
)";

const char* const doc_atypical =
    R"( TODO: These are totally based on random-access flippings; should give more explanations for random-access in "workings" section...


Typically you will explore rules in the well-defined subsets supported by the program. These subsets, however, take up only an extremely small part of all MAP rules. For example, the largest subset in this program is the native 'C2' rules, which has 272 groups, meaning it takes up only 2^(272-512) ~ 2^-240 of all possible MAP rules.

This section shows what may be gotten with this program. For sanity....

Here is the same hex-C6 rule shown in the "Rules in different subsets" section. Notice that it also belongs to the native 'C2' subset, which is inevitable as hex-C6 is actually a strict subset of native C2.
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ

The following part is based on this rule. Before moving on, do:
1. Turn on 'Preview mode' in the rule-edition plane.
2. Set a large pace for the preview windows (in 'Settings') and the main window.

By selecting only native-C2, we bring it to a wider context.
MAPEUQxVSLdM4gRRBFVIt0ziCK7oswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ

By the way, for an arbitrary native-C2 rule, if you do random-access flippings in hex-C6, the result will still belong to native-C2
(... as native-C2.P is a refinement of hex-C6.P, so you will effectively flip (one or) multiple groups of C2 at the same time.) ... The same applies to, for example, random-flipping groups of native-isotropic rules upon C4 rules....


MAPEUQ1VSLdo5gRRBFVItUziCK7oswiCBlEoruizCIAEURVCEQRkYiqIl0AzBEZiKoiRESIKiqRzJHMTACqKpnMEQ
.......
)";

#if 0
// TODO: rewrite...
const char* const doc_IO =
    R"(Some strings will be marked with grey borders when you hover on them. You can right-click on them to copy the text to the clipboard. For example, the current rule (with or without the lock) can be copied this way. The path in the "Load file" is also copyable.

In these documents, you can right-click the lines to copy them (drag to select multiple lines). The line(s) will be displayed as a single piece of copyable string in the popup, then you can right-click that string to copy to the clipboard. As you will see, this document ends with an RLE-pattern blob. ......

There are two recognizable formats that you can left-click directly to set as the current rule:
A MAP rule will be highlighted in green when hovered:
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
A rule-lock pair will be highlighted in yellow when hovered:
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA]

The rules are read on a per-line basis (mainly because it's hard to render the text/locate with cursors cross physical lines etc). If there are more than one rules in a line only the first rule will be recognized. (This might be improved in the future.)

(About the "current rule"... TODO, missing...)

If you left-click a rule-lock pair, both the rule and lock part will be overwritten.
Otherwise (if it's a plain rule), the program will firstly test whether the rule "fits" into the locked part, and will clear the lock if the rule does not fit in. The rule part is assigned normally. This will be useful when you read a list of rules (the first of which is a pair, and the following are plain rules generated from it).
When you click on a rule-lock pair, the working state will be set to it. This makes it easy to know that the following several rules are of the same origin, and you don't have to go back to the original lock if you want to generate new rules based on the lock. If you don't need the lock you can always clear it by the "Clear lock" button.

For example, here is a list of ... TODO...

(TODO: navigation and binding)

Here is an RLE blob (which is a "rocket" in the Day & Night rule). You can firstly click the rule to set the current rule to it, and then copy the following lines (to the '!' mark, with or without the header line (x = ...))

x = 7, y = 14, rule = MAPARYBFxZoF34WaBd+aIF+6RZoF35ogX7paIF+6YEX6ZcWaBd+aIF+6WiBfumBF+mXaIF+6YEX6ZeBF+mXF3+Xfw
3bo3b$2b3o2b$b5ob$ob3obo$2b3o2b$2b3o2b$ob3obo$ob3obo$b5ob$b
5ob$3bo3b$7b$2b3o2b$2bobo2b!

... more details about pasting...


......
About links....

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
#endif

// TODO: the documents are currently unordered.
extern const char* const docs[][2]{{"About this program", doc_about},
                                   {"Overview", doc_overview},
                                   // {"Program I/O", doc_IO},
                                   {"Subset, mask and rule operations", doc_workings},
                                   {"Rules in different subsets", doc_subsets},
                                   {"Atypical rules", doc_atypical},
                                   {"Lock and capture", /*doc_lock_and_capture*/ "This section is not finished yet :("},
                                   {/* null terminator */}};
