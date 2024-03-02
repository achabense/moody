#### About this program
The program provides many tools to explore subsets of 2-state anisotropic rules in range-1 Moore neighborhood (also known as MAP rules).
(Link...)

In this program, it's easy to:
- Test what subsets a MAP rule belongs to.
- Generate rules in the specified subsets. (Explain...)
- Look through all the rules in a subset.
- Look through all the rules that are ...
- The above things, with the extra constraints that some ...
- Look through all MAP-encoded rules in a file.
- ... and so on.

The supported subsets are many, including:
- Totalistic rules (with or without the center cell)
- Isotropic non-totalistic rules.
- Self-complementary rules.
- Rules in hexagonal neighborhood.
- ... (Full list, with link)

And you can combine these subsets freely to explore intersections of these subsets.

#### What can be saved & how to save
- The program uses normal MAP-strings (TODO...) to store rules. This means you can try these rules in other programs like Golly.
- It's easy to copy the rule to the clipboard: right-click the text and it's there.
- For now the program cannot ..., which means you'd have to use this program with a text-editor.
- The program is able to load all MAP rules from a file (on a per-line basis) (Explain...) There are no specific requirements for the file format...

#### Concepts
(masks, locks etc...)

#### Typical use cases
(Should be supported interactively by the program)
(Should begin with the gol rule)
(As to capture, the glider-only rule is the best beginning example)
(Should document how to capture, especially bounding-box is not large enough)

#### Dependency
* ImGui (which is carried by this project) (Link)
* SDL2 (Link)
* C++20
* VS project (will consider cmake build in the future)
* ...