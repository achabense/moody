### Moody
<p><sub>
(Renamed from 'Astral' in v0.9.5; the current name is still not satisfactory to me; a final name will be decided before v1.0)
</sub></p>

https://github.com/achabense/moody

This program helps to explore a set of cellular automaton rules known as "[MAP rules](https://golly.sourceforge.io/Help/Algorithms/QuickLife.html#map)". In short, MAP rules are 2-state rules in the range-1 Moore neighborhood. They are a strict superset of outer-totalistic (aka life-like) rules, the most famous one being [Conway's Game of Life](https://conwaylife.com/wiki/Conway%27s_Game_of_Life). Below are some discoveries made by this program.

<img width="200" alt="Example" src="https://github.com/user-attachments/assets/bc5024d7-e27e-4a66-9c31-3dd3551dc693">
<img width="200" alt="Example" src="https://github.com/user-attachments/assets/3b0099f6-f634-4e47-be9d-b61c3d5f4d7b">
<img width="200" alt="Example" src="https://github.com/user-attachments/assets/2b7afd32-7b92-43cb-8f98-ae523d3221b6">

#### Features
The program supports a series of subsets. You can easily identify which subsets a rule belongs to, and explore rules in the subsets or their intersections (for example, to explore self-complementary isotropic rules). You can get random rules in the `Random` window, or modify existing ones - with the help of preview mode (`Preview`), it's easy to discover interesting rules nearby to the given rules. If the set is reasonably small, you can also test all rules in the set with the `Traverse` window.

<img width="300" alt="image" src="https://github.com/user-attachments/assets/cf4afd1a-7817-41e0-bcc7-9100fcbc4362">
<img width="300" alt="image" src="https://github.com/user-attachments/assets/d1d01887-2f46-425b-ba0c-fb9b8f9f1ce5">

~~Also, the program is able to extract value constraints from patterns, and generate rules under the constraints (for example, rules allowing for gliders).~~ (v0.9.7: temporarily removed, as the old implementation was terrible; will re-support in the future.)

The program saves rules and patterns as common MAP-strings and RLE-strings, which can further be tested by other programs like Golly.

There are a lot of rules collected during the development of this program, available [here](https://github.com/achabense/moody/tree/main/rules). You may download and have a look at some of them using the `Files` or `Clipboard` window.

<img width="300" alt="image" src="https://github.com/user-attachments/assets/ac885633-6ee1-480c-bd79-f47f23c8af62">

#### Getting started
The binary built for Windows 10 is available at the [latest-release](https://github.com/achabense/moody/releases/latest) page. (If you are using a different system, you may try building the project yourself; see the "building" section below.)

It's recommended to place the program in a separate folder, as it will create an "imgui.ini" file in the same directory, and for example, you can download those rule lists to a subdirectory, then it will be easy to find them in the `Files` window.

To get familiar with the program, in the program: press `H` to enter help mode; check the tooltips; check the `Documents` for the concepts and workflow. Below are some basic operations for saving/loading rules and patterns (listed for convenience; all of them are recorded within the program):
- Saving rules: right-click the MAP-string to save to the clipboard. (Also, you can right-click a preview window to copy the previewed rule.)
- Loading rules: open the `Clipboard` window and click `Read clipboard` to load rules from the clipboard. The equivalent shortcut is `W`.
- Saving patterns: select area in the space window with right mouse button, and press `C` to save the entire area, or `I` to identify and save the pattern (limited to oscillators and spaceships).
- Pasting patterns: hover over the space window and press `V` to paste.

#### Building
The project is written in C++20 using the [ImGui](https://github.com/ocornut/imgui) and [SDL2](https://github.com/libsdl-org/SDL) libraries, and uses CMake for building. ImGui is included in this project; SDL2 will be fetched automatically by CMake.

The project is developed on Windows 10. In my experience, it should be easy to open and build the project with Visual Studio (CMake tools required). There is no explicit dependency on OS-specific features, so the project may also work on some other systems.
