#pragma once

namespace Rhiza
{

// A physical key, identified by position rather than what it types - the
// same key is Key::W on QWERTY and AZERTY. Right for gameplay bindings,
// wrong for "press X to continue" UI, which Rhiza doesn't need yet.
//
// Extending the list is one line here plus one in Window.cpp's table.
enum class Key
{
    Unknown = 0,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    Up, Down, Left, Right,

    Space, Return, Escape, Tab, Backspace,

    LeftShift, RightShift,
    LeftCtrl, RightCtrl,
    LeftAlt, RightAlt,

    // Sizes Input's storage, so it can't go stale.
    Count
};

}  // namespace Rhiza
