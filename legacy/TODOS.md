**support building with cmake**

Common todos
Unify naming conventions. (e.g. always use Abc for enums)
More Documentations. More meaningful names.
Add more tooltips.
Keyboard control.
Refine logs
Improve layout

The program shall fulfill as much as features mentioned in:
https://conwaylife.com/wiki/Static_symmetry
https://conwaylife.com/wiki/Neighbourhood
https://conwaylife.com/wiki/Black/white_reversal
https://conwaylife.com/wiki/Self-complementary
https://golly.sourceforge.io/Help/Algorithms/QuickLife.html

How to support general utf-8 glyphs?
    Need to check:
    https://github.com/ocornut/imgui/blob/master/docs/FONTS.md
~ The problem is that is will necessitate .ttf tiles...

How to build? (Especially, the project should be accessible in other systems...)
What is the typical way to include imgui source files in the project?
~ Learn CMake...

What license should this project use?

Whether to support boundless space? It cannot be too useful without copy/paste...
~ Not planned for the first release.

Whether to support customized partition? It can get out of control too easily...
~ Not planned for the first release.

Consider using imgui's "docking" branch?
~ Unfortunately docking features doesn't work with SDL library.

Relax dependency to C++17?
~ No.

Is it really a good idea to use bool type?
~ Yes for now.

Are there UBs in the program?
~ ~> How to use UBsan?

// TODO: recheck c-style casts...
// TODO: recheck captures in the form of [&]...
// TODO: tell apart precondition and impl assertion...