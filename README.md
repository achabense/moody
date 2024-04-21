#### About this program
This program is designed to help explore 2-state CA rules in range-1 Moore neighborhood (aka "MAP rules").
(Link...)

With this program, it's easy to test the effect of a MAP rule, know what subsets it belongs to, and explore rules in the specified subsets systematically.
The supported subsets include self-complementary rules, isotropic/totalistic rules in Moore/hexagonal/Von-Neumann neighborhood, and so on. You can combine these subsets freely to explore rules in the intersections of these subsets.

#### What can be saved & how to save
The program uses normal MAP-strings (Link) to store rules. This means you can try these rules in other programs like Golly.
The program also has an extension called "lock" to help generate specific rules. A lock is encoded the same way as MAP rule and is always associated with a MAP rule.
The format is defined as: `MAP-string + ' ' + '[' + base64-string-of-the-lock + ']'`.

For now the program cannot save the rule to the file directly. However, it's easy to copy the rule to the clipboard: right-click the rule-string and that's done.
The program is able to load all MAP rules from a file or from the clipboard. Rules are loaded on a per-line basis. There are no specific requirements for the file format.

If you find some interesting patterns you can also select and save as RLE string.

#### Getting started
As a starting point, it will help a lot if you are familiar with Convay's Game-of-Life rule.
See the "Documents" part in the program to get familiar with the concepts and operations.

#### Dependencies & Building
The project uses CMake for building. The program is written in C++20 using [ImGui](https://github.com/ocornut/imgui) and [SDL2](https://github.com/libsdl-org/SDL) libraries. (ImGui is copied in this project; SDL2 will be fetched automatically by CMake.) In my experience, the project can be opened and built directly in Visual Studio (CMake tools required).