"""ZiiNAN: Existing unsupported mode must exit 4 even when Python catches its RuntimeError."""
import app
import builtins

with builtins.old_open("renderer-startup.log", "r") as selection:
    if "Renderer: Diligent D3D11" not in selection.read():
        raise RuntimeError("Wrong backend for failure test; never create a Legacy fullscreen window")

try:
    app.Create("M12 expected startup rejection", 640, 480, 1)
except RuntimeError as error:
    if "Diligent D3D11 requires windowed mode" not in str(error):
        raise
    with builtins.old_open("expected-startup-failure.log", "w") as log:
        log.write("Caught original renderer startup error; expect process ExitCode=4, no fallback.\n")
else:
    raise RuntimeError("Expected Diligent window-mode rejection")
