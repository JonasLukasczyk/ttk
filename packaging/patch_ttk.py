
#!/usr/bin/env python3
"""
Patch TTK 1.4.0 for a private VTK/TTK Python wheel.

Usage:
    python /opt/patch_ttk.py /opt/src/ttk

Changes:
  1. Redirect TTK Python wrapping into topology_toolkit._vtkmodules.
  2. Add include paths for shared persistence-diagram utility headers.
  3. Restore the original module dependency declarations for:
       - ttkBottleneckDistance
       - ttkTrackingFromPersistenceDiagrams

The script is idempotent.
"""

from pathlib import Path
import sys


PYTHON_PACKAGE = "topology_toolkit._vtkmodules"
ORIGINAL_PYTHON_PACKAGE = "topologytoolkit"


def read_required(path: Path) -> str:
    if not path.is_file():
        raise FileNotFoundError(
            f"Required TTK source file does not exist: {path}"
        )

    return path.read_text(encoding="utf-8")


def write_if_changed(
    path: Path,
    original: str,
    updated: str,
) -> None:
    if updated == original:
        print(f"Already patched: {path}")
        return

    path.write_text(updated, encoding="utf-8")
    print(f"Patched: {path}")


def patch_python_package(ttk_root: Path) -> None:
    """
    Redirect TTK's Python wrapping into the private VTK namespace.

    The original TTK package name is topologytoolkit.
    """

    path = ttk_root / "core/vtk/CMakeLists.txt"
    original = read_required(path)

    if ORIGINAL_PYTHON_PACKAGE in original:
        updated = original.replace(
            ORIGINAL_PYTHON_PACKAGE,
            PYTHON_PACKAGE,
        )
    elif PYTHON_PACKAGE in original:
        updated = original
    else:
        raise RuntimeError(
            "Neither the original nor patched Python package name "
            f"was found in {path}"
        )

    write_if_changed(path, original, updated)


def patch_shared_utility_includes(ttk_root: Path) -> None:
    """
    Make utility headers from other TTK VTK modules discoverable.

    ttkBottleneckDistanceUtils.h includes:
        ttkPersistenceDiagramUtils.h

    ttkTrackingFromPersistenceDiagrams.cpp includes:
        ttkBottleneckDistanceUtils.h

    TTK 1.4.0's custom dependency checker can reject cross-module
    PRIVATE_DEPENDS declarations before the corresponding targets
    have been created. Use include directories instead.
    """

    path = ttk_root / "core/vtk/CMakeLists.txt"
    original = read_required(path)

    marker = "# BEGIN topology_toolkit shared utility includes"

    if marker in original:
        print(f"Already patched: {path} (utility includes)")
        return

    snippet = """\
# BEGIN topology_toolkit shared utility includes
# TTK 1.4.0: utility headers shared between VTK wrapper modules.
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/ttkPersistenceDiagram"
  "${CMAKE_CURRENT_SOURCE_DIR}/ttkBottleneckDistance"
)
# END topology_toolkit shared utility includes

"""

    updated = snippet + original

    write_if_changed(path, original, updated)


def restore_module_dependencies(
    ttk_root: Path,
    module_name: str,
) -> None:
    """
    Restore the original TTK 1.4.0 vtk.module declaration.

    This removes PRIVATE_DEPENDS entries introduced by our previous
    patch, which were rejected by TTK's custom dependency checker.
    """

    path = ttk_root / f"core/vtk/{module_name}/vtk.module"
    original = read_required(path)

    expected_original = (
        "NAME\n"
        f" {module_name}\n"
        "DEPENDS\n"
        " ttkAlgorithm\n"
    )

    # Both affected vtk.module files originally contain exactly
    # this declaration, as verified from the source tree.
    if original.strip() == expected_original.strip():
        print(f"Already original: {path}")
        return

    lines = original.splitlines()

    if not any(
        line.strip() == module_name
        for line in lines
    ):
        raise RuntimeError(
            f"Unexpected module declaration in {path}"
        )

    # Only restore the two known modules. Fail rather than silently
    # discard other unexpected source changes.
    allowed_lines = {
        "NAME",
        module_name,
        "DEPENDS",
        "ttkAlgorithm",
        "PRIVATE_DEPENDS",
        "ttkPersistenceDiagram",
        "ttkBottleneckDistance",
    }

    unexpected = [
        line.strip()
        for line in lines
        if line.strip() and line.strip() not in allowed_lines
    ]

    if unexpected:
        raise RuntimeError(
            f"Unexpected content in {path}: {unexpected}"
        )

    write_if_changed(
        path,
        original,
        expected_original,
    )


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(
            "Usage: patch_ttk.py /path/to/ttk"
        )

    ttk_root = Path(sys.argv[1]).resolve()

    if not ttk_root.is_dir():
        raise NotADirectoryError(
            f"TTK source directory does not exist: {ttk_root}"
        )

    print(f"TTK source: {ttk_root}")

    # Restore the original module declarations first.
    restore_module_dependencies(
        ttk_root,
        "ttkBottleneckDistance",
    )

    restore_module_dependencies(
        ttk_root,
        "ttkTrackingFromPersistenceDiagrams",
    )

    # Apply the private Python namespace.
    patch_python_package(ttk_root)

    # Make shared utility headers available to the compiler.
    patch_shared_utility_includes(ttk_root)

    print("TTK patches applied successfully.")


if __name__ == "__main__":
    main()
