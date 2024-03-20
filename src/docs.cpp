// TODO: simplest way to use this program... (randomize, and saving the rules...)

const char* const doc_about =
    R"(In these documents (as well as the ones opened via "Load file" or "Clipboard"), you can left-click the rules to see their effects and right-click the lines to copy the text (drag to select multiple lines). For more details see "Program I/O".

--- The sentiment
...
MAP+sQSUIzICkiQgAiAEKBAhrIGFgAUbAAA4AChgnAAAw6CAkAIgKCAlASgIACgIQBbqCqhEQAAkFQAARIDAQQRBA
I hope this is impressive enough. It was picked from many randomized rules many years ago ... (... background of this program)

MAP7KV6wLHQiAHIPICBCAhlIqKAhAuKAFBoYmCFEAACIUzbAIAsAsCBJoAANhiIBEBSUICEMQiQFgRBgAJKgAA4gA)";

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
The current rule is shown in the right plane. At the top ... (about "sequence"/undo/redo/lock/...)

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

// Must keep in sync with "load_rule.cpp".
struct docT {
    const char* title;
    const char* text;
};

// TODO: the documents are currently unordered.
extern const docT docs[]{{"0. About this program", doc_about},          //
                         {"1. Concepts", doc_concepts},                 //
                         {"2. Workflow", doc_workflow},                 //
                         {"3. Typical subsets", doc_subsets},           //
                         {"4. Lock and capture", doc_lock_and_capture}, //
                         {"5. Rules in the wild", doc_atypical},        //
                         {"6. Program I/O", doc_program_IO}};

extern const int doc_size = sizeof(docs) / sizeof(docT);
