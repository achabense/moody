const char* const doc_about =
    R"(In these documents, you can left-click the rules (which will be highlighted if you hover on them) to see their effects, or try the buttons above to do quick navigation. Right-click to copy the text (on per-line basis; drag to select multiple lines).

--- The sentiment
...
MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
I hope this is impressive enough. It was picked from many randomized rules many years ago ... (... background of this program)

MAP7KV6wLHQiAHIPICBCAhlIqKAhAuKAFBoYmCFEAACIUzbAIAsAsCBJoAANhiIBEBSUICEMQiQFgRBgAJKgAA4gA)";

const char* const doc_concepts =
    R"(The program is based on several key concepts. They are subset, distance, mask and lock.

------
The "subset" in this program refers to a special type of subsets that can be defined as:
S = empty set or (R, P), where:
1. R is a MAP rule that is designated to belong to S.
2. P is a partition of all "cases".
3. A MAP rule belongs to S iff XOR-masked by R, the masked values are the same in every group in P.

A lot of categories of rules (e.g. isotropic rules / totalistic rules / self-complementary rules etc) can be composed this way. And most importantly, it can be proven that the intersection of such subsets are of the same structure (empty set or (R', P')), so in the program you are free to combine any of the supported subsets as long as the result is not empty.

It's also obvious that any rule in S can equally serve as the defining mask R.

------
If two rules belong to the same subset S = (R, P), then in every group of P their values are either all-same or all-different. As a result, it's natural to define the distance (in subset S) as the number of groups where the two rules have different values.

For example, the following rules have distance = 1 from the Game-of-Life rule in the isotropic subset.
MAPARYXbhZofOgWaH7oaIDogBZofuhogOiAaIDoAIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAAACAAIAAAAAAAA
MAPARYXfhZofugWan7oaIDogBZofuhogOiAaIDogIgAgAAWaH7oeIDogGiA6ICAAIAAaMDogIAAgACAAIAAAAAAAA
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6YSAAIQAaIDogIAAgACAAIQAAAAAAA
------
A lock is an array associated with a MAP rule, that marks some parts of the rule as contributor for something to happen.

(TODO more explanations...)

You can find a lot of interesting rules without "lock". However, some rules are unlikely discoverable by chance without this feature. Here is an example - an isotropic and self-complementary rule where gliders occur naturally:
MAPARYSZxZtPVoUYRG2cMoGoxdsEtJst5ppcpLka9c/q58GKgMUKdi2sWmmEsm0t8kXOp+s8ZJ3edelQ0mXGbeXfw [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]
MAPARYTZxZsPVoQYRH2UMoGoxdkEtIst5p7coLka9c/r78CCgMUKdi+sSGmEsu0t9kXOp+s9ZB3efelQ8mXGTeXfw)";

const char* const doc_intro =
    R"(Subset recognition and selection
(Example rule)
You may have noticed that in the left plane, some buttons are lighted up with bright-green rings. This means the rule belongs to the subsets represented by them. You may know that the Game-of-Life rule is a totalistic rule (which is also isotropic rule). ...
...
If you click the "Recognize" button, these subsets are selected automatically...
If you select no subsets, the default one is the universal set (which contains all MAP rules). There is no limit what you can do in the universal set. By selecting more subsets...

Current rule: The rule shown in the right plane. At the top ... (about "sequence"/undo/redo)

TODO: Operation precondition (contained/compatible etc)...

Rule-generation
The program has 3 generation modes that covers most needs. All the 3 modes will skip locked groups, and will do nothing if there are no free groups.

---- Integral mode (<00.. [dec/inc] 11..>)
Treat the free groups as a sequence of 0/1 relative to the mask, and apply integral increment/decrement. With this mode you can iterate through all rules in a subset. It does not matter which mask is used (as long as it's valid) in this case.

Example:
MAPAAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////w (whether to add {} lock to ensure the effect?)
This rule belongs to both self-complementary rule and inner-totalistic rule. (whether to explain here?) If you do "Recognize", you wil find both "Zero" and "Identity" mask do not work (... native mask). It's a fairly small subset - only 5 groups exist, which means there are only 2^5 = 32 rules that belongs to the subset.
You can:
1. Select "Native" mask (or custom mask, if the current rule is the above one)
2. "<00.." to set the masked sequence to "0...", which means the result is the same as the masking rule.
3. "inc" until the sequence reached to the end (111...).

---- Permutative mode (<1.0. [prev/next] 0.1.>)
Treat the free groups as a sequence of 0/1 relative to the mask, and reshuffle (?permute) them without changing the distance to the mask. With this mode you can iterate through all rules that have the same distance to the masking rule.

This is especially useful when combining with custom mask. You can check all rules that have distance = 1 to an existing rule.
0. (Correct subset)
1. "Take current" to set the current rule to custom mask. The distance is now 0 (the rule to itself).
2. "inc" to increase the current rule by "1". The distance is now 1.
3. "next"... until the "1" reach to the end. (... About key-binding)

---- Randomization.
Get an arbitrary rule that belongs to the selected subsets, and whose distance to the masking rule (including those that are locked) is exactly / around "C" as set in the program.
(& Zero/Identity mask) ... Again, the masking rule can be the Custom rule ... which means if you have interesting rules you can do randomization with a small distance...

For example, suppose the "Zero" mask belongs to the subset (which is true in most cases), then randomize with dist = ... (effect)

(~ bind to undo/redo)


In a small subset, ...
In a large subset...
Starting from one of Zero or Identity, (or Native if necessary) mask, get randomized rules until you find interesting ones.
Then you can try to find nearby rules with permutative mode/ randomization.

Random-access edition
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

Here is a very important tip. Sometimes you may find rules like this:
(from rul33.txt; better example...)
MAPA0wFEBBTV2EGZnFywDNkKEYRDigbKHiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TPYilLBNImEJIhUoA
However (... -> dist=1)
MAPA0wFEBFTV2EGdnFywDNkKEYRDigbKHiJ6DIklgrKDhYnSAit2JIckGwBtsuJBFMGAPAc5TvYilLBtImEJIhUoA

There is no need to stick to the original mask. You can switch to a new mask whenever you find a "better" rule.
...
MAPA00FARBSVXAPZnFywDNmKEYRDigbKHqJ6GIkbgrKDhZnSAi93pIekGxBtsuJBFMGAPAc5THYSNLJNImEpAjUIA
x = 6, y = 4
4bob$6b$4obo$5bo!

--- The "lock" and capture
The following "glider"-ish rule is apparently not found by chance. As you'd expect, there are ways to "reach" to rules like this in this program...
MAPARYSZhYAPEgQYBCgAAAAgABAEsAIAIgAQIDgAIAAgAACAAIAKACggACAAICAAIAACICogIAAAACAAAAAAAAAAA

Below is the Game-of-life rule, with some additional information about glider.
(... As to the first one vs the second one)
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [/OI4QIQCgACAwBAAAAAQAIAAEMCLAAAAgIAAAAAAAACKAAAAgAKAgAAAAAAAAKAAgACAgAAAAACAAAAAAAAAAA]
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA [//Z65r4SvEjQ4JCgABAQgIDRksSLAIwAwKDgAIDAwACKAIICqAKggICAAKCAAKAAiICoiKCIAACAIAAAIAAAAA]

--- Static constraints
...)";

const char* const doc_subsets =
    R"(The following rules are examples from certain subsets. They may or may not be representative of the subsets they belong to. You can click the "Recognize" button to select the subsets they belong to.
The best way to get familiar with a subset is to generate randomized rules in it. ... (relation between the subsets)

--- None
MAPYaxTu9YJm9UZsagD9KrzcclQXH5nLwLTGALPMYhZeR6QeYRX6y7WoAw4DDpCQnTjY7k71qW7iQtvjMBxLGNBBg
This is typically what you will get in default density (without any constraints).

MAPBRAACkiAAUiATAhmCQkIEYMBgCBDCBFHUqQgJUQAAEAiAAmANgIkRAAISUA0ADgAAZAQAmAMFTCJgAAAYEYEgA

It's possible to get non-trivial rules that do not belong to any well-defined subsets. See "Rules in the wild" part for details.

--- Native symmetry

Isotropic:
MAPEkAFDAgARAAAgXxAAgGAAAAASAAAAAASgAlAAAUEACIAEG6AgQEIAAACzIAQFoAAgAUIAEEEADABUoAAGkgAAQ

\:
MAPEUQRQDEGwsYQQAlGgBgAFAjB8AEAARBIEmARoA4aKABABHPAkqgQ5BwAAcQAABIQoGAwZQsCgBgggBAIgeYiCQ


| & -:
MAPIAJIChAAUMgAELFhAQCBgIAASWAABCgAAEZCAIIQhAAAAaOBAgApgAAQAAICIRAAAJAYhBwCAAAACWhIQQRIAA

\ & /:
MAPAhghuAgQgyQsgCBAwliAAA0RiCAAAOCQgggQwAQABABBCEUgFAgeESlGQEIUAACBAEQCUKCIkWBkAMCRIAABRA

Notice that both (| & -) and (\ & /) belong to the C2 subset (which is NOT true reversely - a rule that belongs to C2 may not belong to any of |, -, \ or /)

C2: (Temp from rul32.txt)
MAP2AAAAQAAAQCAAAAQAAAAAAAAAAAAAAAAAAAAQAAAAACAAAAAAAACAAAAAAAAAAAAgAAAAAAACAAAAACAAACAAA
Notice how "sparse" this rule is...

C4:
C4 is a strict subset of C2 (so every C4 rule will belong to C2). In C4...
MAPAkMwkQDI20gEBSC4AtgYpzNEmgABABwookggwMACAA0FEhCAo8gCgEwAACAoAAALMADhuUAYmQFgYBCRiACRHg
It's highly likely that there are "more interesting" rules close to it. This is a very important technique as described in TODO...
MAPAkMwkQDI20gEBSS4AtAYpzNEmgABABwookggwMACAA0FEhSAo8gCgEwASCAoAAALIADhuUAYmQFgYBCRiACRHg

(Outer-) Totalistic:
MAPARYXfhZofugWaH7oaIDogBZofuhogOiAaIDogIAAgAAWaH7oaIDogGiA6ICAAIAAaIDogIAAgACAAIAAAAAAAA
Aah! The Game-of-Life rule.

--- The self-complementary rule
(...)
MAPgAAAAQABARcAAQEXARcXfwABARcBFxd/ARcXfxd/f/8AAQEXARcXfwEXF38Xf3//ARcXfxd/f/8Xf3//f////g

Well, that's not many! I'd recommend trying all the rules in this set.
(...)

--- Native symmetry, but in Von-Neumann neighborhood

Isotropic:
MAPzAAzzAAzzMzMADPMADPMzAAz/wAz/wAzADP/ADP/ADPMADPMADPMzMwAM8wAM8zMADP/ADP/ADMAM/8AM/8AMw

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

// Must keep in sync with "load_rule.cpp".
struct docT {
    const char* title;
    const char* text;
};

extern const docT docs[]{{"0. About this program", doc_about}, //
                         {"1. Concepts", doc_concepts},        //
                         {"2. Rule operations", doc_intro},    //
                         {"3. Typical subsets", doc_subsets},  //
                         {"4. Rules in the wild", doc_atypical}};

extern const int doc_size = sizeof(docs) / sizeof(docT);
