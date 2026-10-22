
from pathlib import Path
import re
import sys

vtk = Path(sys.argv[1])
package = "topology_toolkit._vtkmodules"


def replace_required(filename, old, new):
    path = vtk / filename
    content = path.read_text()

    if old not in content:
        raise RuntimeError(
            f"Expected text not found in {filename}: {old!r}"
        )

    path.write_text(content.replace(old, new))
    print(f"Patched {filename}")


def replace_idempotent(filename, old, new):
    path = vtk / filename
    content = path.read_text()

    if old in content:
        path.write_text(content.replace(old, new))
        print(f"Patched {filename}")
    elif new in content:
        print(f"Already patched {filename}")
    else:
        raise RuntimeError(
            f"Neither expected original nor patched text "
            f"found in {filename}"
        )


# ============================================================
# Main VTK wrapping configuration
# ============================================================

replace_idempotent(
    "CMakeLists.txt",
    'PYTHON_PACKAGE  "vtkmodules"',
    f'PYTHON_PACKAGE  "{package}"',
)


# ============================================================
# Rewrite Python aggregation imports and generated paths
# ============================================================

path = vtk / "Wrapping/Python/CMakeLists.txt"
content = path.read_text()

# Replace literal Python package references in this CMake file.
# This is deliberately limited to the wrapping configuration.
content = re.sub(
    r"(?<![\w.])vtkmodules\.",
    f"{package}.",
    content,
)

path.write_text(content)

print("Patched Wrapping/Python/CMakeLists.txt")


# ============================================================
# Fix native VTK Python module initialization
# ============================================================

# vtkPythonUtil::AddModule() imports the Python package to
# retrieve the on_vtk_module_init initialization hook.
#
# The original VTK source imports "vtkmodules", but our
# extensions live in "topology_toolkit._vtkmodules".
#
# Without this change, PyImport_ImportModule("vtkmodules")
# can return NULL. The subsequent PyObject_GetAttrString()
# call then segfaults.

replace_idempotent(
    "Wrapping/PythonCore/vtkPythonUtil.cxx",
    'PyImport_ImportModule("vtkmodules")',
    f'PyImport_ImportModule("{package}")',
)


# ============================================================
# Report remaining hardcoded references
# ============================================================

print("\nRemaining wrapping references to inspect:")

for path in (vtk / "Wrapping/Python").rglob("*"):
    if not path.is_file():
        continue
    if path.suffix not in {".py", ".in", ".cmake", ".txt"}:
        continue

    try:
        content = path.read_text()
    except UnicodeError:
        continue

    if "vtkmodules" in content:
        print(path.relative_to(vtk))

print("\nInitial patch completed.")
