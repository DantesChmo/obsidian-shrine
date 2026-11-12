"""Names of the engine keys (EKeys) a binding in Game/input.yaml may use.

The engine resolves the names at runtime and ignores the ones it does not know, so this list is what turns a
typo into an error with a line number. Analog single axes (MouseX, trigger axes) are left out on purpose: the
game's actions are either buttons or whole sticks.
"""

AXES_2D = ("Mouse2D", "Gamepad_Left2D", "Gamepad_Right2D")

_LETTERS = tuple("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
_DIGITS = ("Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine")

BUTTONS = _LETTERS + _DIGITS + tuple("NumPad" + digit for digit in _DIGITS) + tuple("F{}".format(n) for n in range(1, 13)) + (
    "LeftMouseButton", "RightMouseButton", "MiddleMouseButton", "ThumbMouseButton", "ThumbMouseButton2",
    "MouseScrollUp", "MouseScrollDown",
    "SpaceBar", "Enter", "Tab", "Escape", "BackSpace", "CapsLock",
    "LeftShift", "RightShift", "LeftControl", "RightControl", "LeftAlt", "RightAlt",
    "Left", "Right", "Up", "Down", "Insert", "Delete", "Home", "End", "PageUp", "PageDown",
    "Multiply", "Add", "Subtract", "Decimal", "Divide",
    "Semicolon", "Equals", "Comma", "Hyphen", "Period", "Slash", "Tilde", "LeftBracket", "Backslash", "RightBracket",
    "Apostrophe",
    "Gamepad_FaceButton_Bottom", "Gamepad_FaceButton_Right", "Gamepad_FaceButton_Left", "Gamepad_FaceButton_Top",
    "Gamepad_LeftShoulder", "Gamepad_RightShoulder", "Gamepad_LeftTrigger", "Gamepad_RightTrigger",
    "Gamepad_LeftThumbstick", "Gamepad_RightThumbstick", "Gamepad_Special_Left", "Gamepad_Special_Right",
    "Gamepad_DPad_Up", "Gamepad_DPad_Down", "Gamepad_DPad_Left", "Gamepad_DPad_Right",
)

ALL = AXES_2D + BUTTONS
