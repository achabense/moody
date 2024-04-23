### Starry
This program is designed for exploring 2-state CA rules in the range-1 Moore neighborhood (aka "MAP rules").

#### Features
- Rules are saved in the MAP-string format, so you are able to test them in other programs like `Golly`. Also, you can save the pattern as RLE-string, which is also .......
- Subset-based rule-analysis and generation. The supported subsets include self-complementary rules, isotropic/totalistic rules in Moore/hexagonal/Von-Neumann neighborhood, and so on. The subsets ......
- Rule-analysis: the program is able to telling whether a MAP-rule is isotropic/self-complementary etc.
- Rule-generation/edition: the program is able to generate (theoretically all possible) rules in any supported subsets.
- Preview mode, "slice" (making it easier to "know" which direction to modify for more interesting rules...)
![TODO]() ~ add some screenshots...
- Reading files, with preview window turned on ...
- Lock & capture ...
...

#### Getting started
See the "Documents" part in the program to get familiar with the concepts and operations. I'd recommend firstly checking the "Rules in different subsets" section to get some idea about what can be found with this program.
The operations are either accessible via buttons, or recorded somewhere in the tooltips. Most importantly, here is how to save the rules/patterns:
- Saving rules: right-click the MAP-string for the current rule to save to the clipboard.
- Saving patterns: right-click to select area and press 'C' to save to the clipboard (as RLE-string).
- Undoing editions: ...

#### Dependencies & Building
- The project uses CMake for building. The program is written in C++20 using [ImGui](https://github.com/ocornut/imgui) and [SDL2](https://github.com/libsdl-org/SDL) libraries. (ImGui is copied in this project; SDL2 will be fetched automatically by CMake.) In my experience, the project can be opened and built directly in Visual Studio (CMake tools required).
- As to OS dependency: unknown, but hopefully the program does not rely on OS-specific features. That said, the project/program is guaranteed to work on Windows.

#### Recent plans
- Recheck the rule lists collected so far (those in `rules/...`)... (There are going to be selected lists of rules in the formal releases. ...)
- Finish documentation for the rule & capture feature (which has been completed but it's program-specific so it's harder to explain...)
- Add support for unbounded space.
- Add support for batch-preview mode for 'Randomize' etc...
- Refine and stabilize space operations (keyboard controls etc). (Some controls may be different in the formal release...)
- Better visual/layout.
- (Maybe) Add a convenient way to save rules directly into files.
