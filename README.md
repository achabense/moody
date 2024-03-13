#### About this program
This program is designed to help explore subsets of 2-state CA rules in range-1 Moore neighborhood (also known as MAP rules).
(Link...)

With this program, it's easy to test the effect of a MAP rule, tell what subsets the rule belongs to, and generate rules in the specified subsets in non-trivial ways.
The supported subsets include self-complementary rules, isotropic/totalistic rules in Moore/hexagonal/Von-Neumann neighborhood, and so on. You can combine these subsets freely to explore rules in the intersections of these subsets.

#### What can be saved & how to save
The program uses normal MAP-strings (Link) to store rules. This means you can try these rules in other programs like Golly.
The program also has an extension called "lock" to help generate specific rules. A lock is encoded the same way as MAP rule and is always associated with a MAP rule.

For now the program cannot save the rule to the file directly. However, it's easy to copy the rule to the clipboard: right-click the rule-string and that's done.
The program is able to load all MAP rules from a file or from the clipboard. Rules are loaded on a per-line basis. There are no specific requirements for the file format.

#### Examples found by this program
(Rule with pics)

#### Getting started
As a starting point, it will help a lot if you are familiar with Convay's Game-of-Life rule.
See the "Documents" part in the program to get familiar with the concepts and operations.

#### How to build & Dependency
The program is written in C++20, and developed with the help of ImGui (Link) (which is carried by this project) and SDL2 (there is a zip in ...).
The project is currently organized as a VS project. ...
