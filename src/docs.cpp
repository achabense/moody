// TODO: simplest way to use this program... (randomize, and saving the rules...)

const char* const doc_about =
    R"(In these documents (as well as the ones opened via "Load file" or "Clipboard"), you can left-click the rules to see their effects and right-click the lines to copy the text (drag to select multiple lines). For more details see "Program I/O".

--- The sentiment
...
MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
I hope this is impressive enough. It was picked from many randomized rules many years ago ... (... background of this program)

MAP7KV6wLHQiAHIPICBCAhlIqKAhAuKAFBoYmCFEAACIUzbAIAsAsCBJoAANhiIBEBSUICEMQiQFgRBgAJKgAA4gA)";

const char* const doc_workings =
    R"(This section describes the exact workings of subsets, masks and the major rule operations.

In this program, a MAP rule is located by a series of subsets. These subsets are uniformly composable in the form of:
S = (M, P) (if not empty), where:
1. M is a MAP rule specified to belong to S, and P is a partition of all cases.
2. A rule R belongs to S iff in each group in P, the values of the rule are either all the same or all the different than those in M.
'2.' can also be defined in terms of XOR operations ~ (R ^ M) has either all 0 or all 1 in each group in P.

As a consequence, there are:

(When there are k groups in P, there are 2^k rules in S.)

>> 1. For any two (??any two??) rules that belong to the same subset S = (M, P), in each group in P, the two rules must have either all the same or all the different values, just like them to M. In this sense, it does not matter which rule serves as M in S.

>> 2. As a result, it's also natural to define the "distance" between the two rules (in S) as the number of groups where they have different values.
To ease the air, here are some rules that all have distance = 1 (in the isotropic set) to the Game-of-Life rule:
MAPARYXbhZofOgWaH7oaIDogBZofuhogOiAaIDoAIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAAACAAIAAAAAAAA
MAPARYXfhZofugWan7oaIDogBZofuhogOiAaIDogIgAgAAWaH7oeIDogGiA6ICAAIAAaMDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6YSAAIQAaIDogIAAgACAAIQAAAAAAA
In this program it's fairly easy to find such rules.

>> 3. If a rule R belongs to S = (M, P), by flipping all values in a group of R, you will always get another rule in S. Conversely, if R does not belong to S, then by flipping the values you essentially get rules in S' = (R, S.P). More generally, from any rule in S, by flipping all values in some groups, you are able to get to any rule in the set.

>> Finally, it can be proven that, the intersection of such subsets are of the same structure. That is, S1 & S2 -> (R', P') (if not empty), where R' is an arbitrary rule that belongs to both S1 and S2, and P' is ...
Therefore, the above conclusions apply to any combinations of these sets.
And obviously the whole MAP ruleset can be defined this way ..........

As a result, the program provides the ability to select the subsets freely (as long as the result is not empty) - the program will calculate the intersection of the selected subsets (), and ...
The working set, and the active mask (directly called "mask" in the program...)

The program decide to make M immutable and designate an "active" mask that do the real observation...

(The active mask)
In most situations at least one of 'Zero' or 'Identity' will work, that's actually because the supported subsets are either defined based on 'Zero' or 'Identity' mask. There do exist situations where neither works, ...


With the above background, it will be much clearer to explain what happens in the program:
0. For the selected subsets, the program calculates their intersection (with the whole MAP set) as the working set W = (M, P).
1. Then you need to decide a working mask M' to actively measure the rules. To allow for further editions, M' must belong to W.
M is exposed as 'Native', so that there is at least a viable rule in the set.
2. The current rule C is XOR-masked by the M', into a sequence of 0/1. The rule belongs to the working set (in other words, every selected subset) iff, the masked values has ......
3. '<00.. Prev/Next 11..>' generates new rules based on the relation of C and M'. The effect is: ...........
4. 'Randomize' generates randomized rules in W with specified distance to M'.
5. The random-access section displays the masked values...
By left-clicking the button you get a rule with each value in the group flipped. As a result, if the current rule C already belongs to W, the result will still belong to W. Otherwise, the operation essentially defines (C, W.P).
... The design is intentional...

If the working set is small enough (if it has only a few groups), the most direct way to explore the set is simply to check every rule in it.
For example, try selecting 'S.c.' and 'Tot(+s)' (self-complementary and inner-totalistic rules). In this case, both 'Zero' and 'Identity' do not work. ...
There are only 5 groups, which means there are only 2^5 = 32 rules in the set, so it's fairly reasonable to check all of them. By clicking '<00..' you start from M', which happens to be..........
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w


If the working set is large, then it becomes ...
Typically, we can:
1. If there are no existing rules, we select a mask M' to get randomized rules.
2. If there are rules in the set known to be special/promising, we can check rules that are ..
2.1. You can do '<< Cur' to set M' as C, then do ......
2.2. The random-access section has a 'Preview mode' ??? by turning it on, ...

)";

#if 0
const char* const doc_concepts =
    R"(The program is based on several key concepts. They are mask, subset, distance and lock.

------
TODO: whether to weaken this concept?
As MAP rules are two-state rules, every rule can serve as an "XOR mask" to "measure" other rules.
...

------
TODO: it's problematic to call these things "subset"... better name? category?
The "subset" in this program refers to a special type of subsets of MAP rules that can be defined as:
S = empty set or (R, P), where:
1. R is a MAP rule that is designated to belong to S.
2. P is a partition of all "cases".
3. A MAP rule belongs to S iff in each group the values are either all-same or all-different to R. (In other words, if XOR-masked by R, the masked values are the same in every group.)

As a result:
If P has K groups, then there are 2^K rules in S.
For any two rules in S, they have either all-same or all-different values in each group. (So actually, any rule in S can equally serve as the defining rule R.)

A lot of categories of rules (e.g. isotropic rules / totalistic rules / self-complementary rules etc) can be composed this way. And more importantly, it can be proven that the intersection of such subsets are of the same structure (empty set or (R', P')), so in the program you are free to combine any of the supported subsets as long as the result is not empty.
------
If two rules belong to the same subset S = (R, P), then in every group of P their values are either all-same or all-different. As a result, it's natural to define the distance (in subset S) as the number of groups where the two rules have different values.

For example, the following rules have distance = 1 to the Game-of-Life rule in the isotropic subset.
MAPARYXbhZofOgWaH7oaIDogBZofuhogOiAaIDoAIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAAACAAIAAAAAAAA
MAPARYXfhZofugWan7oaIDogBZofuhogOiAaIDogIgAgAAWaH7oeIDogGiA6ICAAIAAaMDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6YSAAIQAaIDogIAAgACAAIQAAAAAAA
------
A lock is an array associated with a MAP rule, that marks some parts of the rule as contributor for something to happen.
Essentially this defines another kind of subsets R[L] - a MAP rule "is compatible with" (belongs to) it iff the rule has the same value as R for any "locked" case.

You can find a lot of interesting rules without "lock". However, some rules are unlikely discoverable by chance without this feature. Here is an example - an isotropic and self-complementary rule where gliders occur naturally:
MAPARYSZxZtPVoUYRG2cMoGoxdsEtJst5ppcpLka9c/q58GKgMUKdi2sWmmEsm0t8kXOp+s8ZJ3edelQ0mXGbeXfw [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]
MAPARYTZxZsPVoQYRH2UMoGoxdkEtIst5p7coLka9c/r78CCgMUKdi+sSGmEsu0t9kXOp+s9ZB3efelQ8mXGTeXfw

You will learn about how to find rules like this in the "Lock and capture" section.)";

const char* const doc_workflow =
    R"(---- Overview
The current rule is shown in the right plane. At the top ...

---- Rule sequence
There are a series of widgets that manage a sequence of rules. They are organized as a group of buttons for "first prev/next last". When you click on any of them the left/right keys are bound to prev/next (so you can press the keys to trigger the button).
The first sequence you should be familiar with is the history of working state. With this you can perform undo/redo.

...

---- Subset recognition and selection

You may have noticed that in the left plane, some buttons are lighted up with bright-green rings. This means the rule belongs to the subsets represented by them. For example, the Game-of-Life rule is a totalistic rule (which is also isotropic rule). As a result, the second line (which represent native symmetry) and the first button in the third line are lighted up.

If you click the "Recognize" button, the subsets that the current rule belongs to are selected automatically...
Or, you can select the subsets manually to ...

(TODO: "working set"; rename "current" as "working" in edit_rule.cpp)
By selecting these subsets the program is actually performing the intersection of them. The subsets that are bright blue are explicitly selected; the subsets that are marked dull-blue ... The subsets that are marked red ... Feel free to combine the subsets at will to get familiar with their relations.

By default no subsets are selected, which means the universal set (which contains all MAP rules). There is no limit what you can do in the universal set, however, as there is no restrictions the outputs are also unlikely interesting. For example, ...

---- Mask selection

After selecting the subsets you are required to select a mask, which is an arbitrary rule ... based on which you can observe other rules (the current rule) and generate rules based on the mask...

There are four modes that you can select, they are:
1. Zero mask:
....
2. Identity mask:
....
3. Native mask...
Is guaranteed to belong to the [working set]. It is calculated by the program ...
... unless both Zero mask and Identity mask does not work.
(Example when the native mask is necessary).
4. Custom mask: which is an arbitrary rule you select to serve as a mask. It is a powerful tool to help find "nearby" rules...
....

The relation between a rule-lock and subset.
1. contained.
2. compatible.
3. incompatible.

For universal set ...

---- Rule-generation
There are 3 generation modes that cover most needs. All the 3 modes will skip locked groups, and will do nothing if there are no free groups.

-- Integral mode (<00.. [dec/inc] 11..>)
Treat the free groups as a sequence of 0/1 relative to the mask, and apply integral increment/decrement. With this mode you can iterate through all rules in a subset. It does not matter which mask is used (as long as it's valid) in this case.

Example:
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w (whether to add {} lock to ensure the effect?)
This rule belongs to both self-complementary rule and inner-totalistic rule. (whether to explain here?) If you do "Recognize", you wil find both "Zero" and "Identity" mask do not work (... native mask). It's a fairly small subset - only 5 groups exist, which means there are only 2^5 = 32 rules that belongs to the subset.
You can:
1. Select "Native" mask (or custom mask, if the current rule is the above one)
2. "<00.." to set the masked sequence to "0...", which means the result is the same as the masking rule.
3. "inc" until the sequence reached to the end (111...).

-- Permutative mode (<1.0. [prev/next] 0.1.>)
Treat the free groups as a sequence of 0/1 relative to the mask, and reshuffle (?permute) them without changing the distance to the mask. With this mode you can iterate through all rules that have the same distance to the masking rule.

This is especially useful when combining with custom mask. You can check all rules that have distance = 1 to an existing rule.
0. (Correct subset)
1. "Take current" to set the current rule to custom mask. The distance is now 0 (the rule to itself).
2. "inc" to increase the current rule by "1". The distance is now 1.
3. "next"... until the "1" reach to the end. (... About key-binding)

-- Randomization.
Get an arbitrary rule that belongs to the selected subsets, and whose distance to the masking rule (including those that are locked) is exactly / around "C" as set in the program.
(& Zero/Identity mask) ... Again, the masking rule can be the Custom rule ... which means if you have interesting rules you can do randomization with a small distance...

For example, suppose the "Zero" mask belongs to the subset (which is true in most cases), then randomize with dist = ... (effect)

Here is a useful tip: when doing randomize, you can bind the <- -> key to undo/redo and set a larger pace, then you can ...


In a small subset, ...
In a large subset...
Starting from one of Zero or Identity, (or Native if necessary) mask, get randomized rules until you find interesting ones. Then you can try to find nearby rules with permutative mode / randomization with small distance.

In some subsets, sometimes you may find rules like this:
(from rul33.txt; better example...)
MAPA0wFEBBTV2EGZnFywDNkKEYRDigbKHiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TPYilLBNImEJIhUoA
However (... -> dist=1)
MAPA0wFEBFTV2EGdnFywDNkKEYRDigbKHiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TvYilLBtImEJIhUoA

There is no need to stick to the original mask. You can switch to a new mask whenever you find a "better" rule. This way you are going to "wander" in the , with each important step "fixed" by the identity mask. This turns out to be able to help find a lot of interesting rules.
...
MAPA00FARBSVXAPZnFywDNmKEYRDigbKHqJ6GIkbgrKDhZnSAi93pIekGxBtsuJBFMGAPAc5THYSNLJNImEpAjUIA
x = 6, y = 4
4bob$6b$4obo$5bo!

---- Random-access edition
Each group is shown by one of the cases that belongs to it.

By left-clicking the buttons you flip the values for each case in the group, so the result is actually independent of which mask you have selected / whether there exists locks in the group / or whether the current rule belongs to the subsets or even compatible.
As a result...
In any case (whether !compatible or !contained), you will switch back if you click on the same button twice.
If the rule already belongs to the subsets ...
....
By right clicking the buttons ...
(use cases with examples...)
Turn-off the clobbering bit.
Lock some groups manually.
...


--- The "lock" and capture
The following "glider"-ish rule is apparently not found by chance. As you'd expect, there are ways to "reach" to rules like this in this program...
MAPARYSZhYAPEgQYBCgAAAAgABAEsAIAIgAQIDgAIAAgAACAAIAKACggACAAICAAIAACICogIAAAACAAAAAAAAAAA

Below is the Game-of-life rule, with some additional information about glider.
(... As to the first one vs the second one)
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [//Z65r4SvEjQ4JCgABAQgIDRksSLAIwAwKDgAIDAwACKAIICqAKggICAAKCAAKAAiICoiKCIAACAIAAAIAAAAA]

--- Static constraints / Mir
...)";
#endif

const char* const doc_subsets =
    R"(The following rules are selected from different subsets. You can click the 'Recognize' button to select the subsets they belong to.
The best way to get familiar with a subset is to generate randomized rules in it. As to the relation between the subsets, .............


--- None
This is typically what you will get in default density (without any constraints):
MAPYaxTu9YJm9UZsagD9KrzcclQXH5nLwLTGALPMYhZeR6QeYRX6y7WoAw4DDpCQnTjY7k71qW7iQtvjMBxLGNBBg

MAPBRAACkiAAUiATAhmCQkIEYMBgCBDCBFHUqQgJUQAAEAiAAmANgIkRAAISUA0ADgAAZAQAmAMFTCJgAAAYEYEgA

It's possible to get non-trivial rules that do not belong to any well-defined subsets. See "Rules in the wild" part for details.


--- Native symmetry
...............


Isotropic ('All'):
MAPgAAFBAAABAAAAVxABgEAAAAAAAAAAAASBAlAAAUEACIAFE4AAQEIABIKzIBQFoAABAUIAEEEADABUoAAGkgAAQ
MAPBAwgkEQAgsh8AQAAgQcAgEgAsgAAEgAAQQcggAMsBAJugQAAARUAgMiQAAKCXhABCREgBBVkgBATWgABWkgBBA

Notice that even isotropic rules can easily be completely chaotic:
MAPpYmhj8ES1QVpVzN8kX6+ZskXyU4XX1xeA2ZdWnr59y9pgzP2H35uPIRwF+8qaP36EXpPvzzrWnX66P6G6DOUkg



'-' ('|' is essentially the same as '-'):
MAPQCAkAAAAABCJAAgABFACAgCCoAABIQAAAAAGBAJABIFACEADAAIAAgAAlBVDABAAACEQCBQIQEhAAgEMJCAAEQ
MAPIKAkAIAAABAJAAgABFACAoCCoAABIQAAAAAGBAJABIFACEADAAIAAgAAlBVDABAAACEQCBQIQEhAAgEMJCAAEQ


'\' ('/' is essentially the same as '\'):
MAPEUwRQHEGwsIwQQlGQBgAFETB8AEAARBIEmABoA4aKABgjHPAk6gQ5DwAAcQAABKQoGAyZQsCgBggwBAIieYiCQ
MAPQAjggAIIgAAgAAAACQAAAAAAgAAggAAAKIQAAAAAAAAAAAIAAiEgAAgACIAICAEgACAAggAARAAKkAAEAABAAA


'| & -':
MAPIAJIChAAUMgAELFhAQCBgIAASWAABCgAAEZCAIIQhAAAAaOBAgApgAAQAAICIRAAAJAYhBwCAAAACWhIQQRIAA
MAP7AKSABAAABLAAwAAAQBgSKACABgQIEISEhCAIKkEAKCIAQAoEQAASIAAAAAAEgAAEuGAAAIEIKAASgAAWAEAAA


'\ & /':
MAPAhghuAgQgywsgCBAwliAgA0RiiAAAOCQgggwwAQABABBCEUgFAgeESlGQEIUAACBAEQCUKCIkWBkAMCRIAABRA
MAPAhEkiDkAgEICmmcMDTWcNgDDmAcEHBZKjUUNInwAZqgRQggGZxAAtAJqmisCKGlJQJIlOJh5EFlCyBoFohglFA

Both '| & -' and '\ & /' belong to the 'C2' subset (which is NOT true reversely - a rule that belongs to 'C2' may not belong to any of '|, -, \ or /').


'C2':
Notice how "sparse" this rule is:
MAP2AAAAQAAAQCAAAAQAAAAAAAAAAAAAAAAAAAAQAAAAACAAAAAAAACAAAAAAAAAAAAgAAAAAAACAAAAACAAACAAA
MAPAjaoGRYMbAUJCY78PLH9F3RZ5I6GChkQ1aZIFMMgJBp3BiKINUAIUWLoKJrChCqIYsgRKaAJj3pKQhA4KBiQyA


'C4':
'C4' is a strict subset of 'C2'. In 'C4', oftentimes (for example, by getting randomized rules) you will find interesting die-patterns:
MAPAkMwkQDI20gEBSC4F/gYtzNEmgAVCB0ookwgwMEGAA0FExCAo8gCgFw4ACAqEgALNCnhuUQcmQlgahCx2ACRHg
It's highly likely there are oscillators or spaceships close to it. (The following rule has huge shaceships.)
MAPAkMwkwDo20gEBSC4F/gItzNkmkA1iBkookwgwMEGgA0FExCAo8gigFw4AAAqEgALNCnhsUQcmQlgahCx2ACRHg

/////////////////////////////////////////////
(Outer-) Totalistic:
Aah! The Game-of-Life rule.
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA

--- The self-complementary rule
(...)
MAPgAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////g

Well, that's not many! I'd recommend trying all the rules in this set.
(...)

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

--- Atypical subsets

)";

const char* const doc_program_IO =
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

... more details about pasting...)";

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

const char* const doc_atypical =
    R"(Typically you will explore rules that belong to several of the "well-defined" subsets. These subsets, however, take up just a small part of all MAP rules. With the help of "lock" feature it's possible to generate very strange rules that do not belong to any well-defined subsets.

Here is the same example in ...
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ

If you try to capture the oscillator, you'll get:
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAEURVAEQRmYiqIlUARBGZiKoizESIqiKZzBHMRIiqIpnMEQ [/////////////////////P//////9+///u32/+/vu/L////+/v//3v///f3/7v/+//7/+v///uv8/t39//vneA]

Do the "Recognize" and you will find that all the groups are locked - The oscillator relies on ....
However, not all groups are fully locked, which means if you switch to a wider subsets...

The native C2 subset
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAFUVVAEQRmYiqIlUARBOZiKoizESIqyKZzBHMRIiqIpncEw
MAPEUQRVSLdM4gRRBFVIt0ziiK7IswiABFEIrsjzCIAFUxVAEQRmYiqIlUARBOZiKojzESIqiKZzAHMRIqqIpnMFQ
MAPEUQRVSLdM4gRRBFVIt0ziiK7IswiABFEIrsizCIAEUxVAEQRmYiqIlUARBGZiaojzESIqyKZzBHMRIiqIp3cFA
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEI7sizCIAEUVVAEQRmYiqIlUARBGZiaojzESIqyKZzAHORIiqIp3cFg
MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEIrsizCIAFURVAEQRmYiqIlUARBOZiaojzESIqiKZzBHMRIiqIp3MFQ

x = 7, y = 5
b3o3b$2b2o3b$4ob2o$b2o2b2o$2bo3bo!

C2 takes up a much larger part of the MAP rules. But that's still "only" 2^272 rules...

MAPEUQRVSLdM4gRRBFVIt0ziCK7IswiABFEI6srzCIAFUVVAEQQmIiqI1UARBOZmaojzEWIryKZzQXPRYioIp3EFQ [/////v///////v/+9vr79f///9v//b+6+P/4+fTHvCT/+//v/+//3v/2+PH/rqvK/63//v/99+v55vXe/nrzsQ]
MAPEUQRVCLdM4gRRBFVItkziiK7IswiABEAIKsoyCIAFBxVAEQAmIiqA1UIQBGZiOoCzEWIryKZxRHJRYCoIp3AEQ [/////v///////v/+9vr79f///9v//b+6+P/4+fTHvCT/+//v/+//3v/2+PH/rqvK/63//v/99+v55vXe/nrzsQ]
MAPEUQRVCLdM4gRRBFVItkziiK7IswiABEAIKsoyCIAFBxVAEQAmIiqA1UIQBGZiOoCzEWIryKZxRHJRYCoIp3AEQ [/////v/////////+/////////////f/6+v/4//7v/2T/+//v/////v/++vH/7rvO//3//v//9+v9/v3+/nr/uQ]
MAPEUQRVCLdM4gRRBFUItkziiK7IswiABEEIasuyCIAFAZVBEQAmIiqAlUIQBGZiKoizEWIryKZxQHJRIKpIhjAEQ [/////v/////////+/////////////f/6+v/4//7v/2T/+//v/////v/++vH/7rvO//3//v//9+v9/v3+/nr/uQ]
These things are insane, so let's stop here.)";

// TODO: the documents are currently unordered.
extern const char* const docs[][2]{{"About this program", doc_about},
                                   {"Subset, mask and rule operations", doc_workings},
                                   // {"Concepts", doc_concepts},
                                   // {"Workflow", doc_workflow},
                                   {"Rules in different subsets", doc_subsets},
                                   {"Lock and capture", doc_lock_and_capture},
                                   {"Rules in the wild", doc_atypical},
                                   {"Program I/O", doc_program_IO},
                                   {/* null terminator */}};
