#!/usr/bin/env bash
set -Eeuo pipefail

# Resolve the TTK checkout from this script, regardless of the working directory.
PACKAGING_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$PACKAGING_DIR/.." && pwd)"
ROOT="$REPO_ROOT"
WORK="${BUILD_ROOT:-$(cd -- "$REPO_ROOT/.." && pwd)/ttk-wheel-work}"
case "$WORK/" in "$REPO_ROOT/"*) echo "BUILD_ROOT must be outside the repository: $WORK" >&2; exit 1 ;; esac
JOBS="${BUILD_JOBS:-8}"
VTK_VERSION="${VTK_VERSION:-v9.7.1}"
BOOST_VERSION="${BOOST_VERSION:-1.86.0}"
BOOST_VERSION_U="${BOOST_VERSION//./_}"
VTK_SOURCE="$WORK/src/vtk"
VTK_BUILD="$WORK/build/vtk"
VTK_INSTALL="$WORK/install/vtk"
TTK_SOURCE="$WORK/src/ttk"
TTK_BUILD="$WORK/build/ttk"
TTK_INSTALL="$WORK/install/ttk"
BOOST_SOURCE="$WORK/src/boost"
BOOST_ROOT="$WORK/install/boost"
WHEEL_SOURCE="$WORK/wheel-src"
DIST="$WORK/dist"
REPAIRED="$WORK/repaired-dist"
VTK_CMAKE_DIR="$VTK_INSTALL/vtk-${VTK_VERSION#v}.data/headers/cmake"
export BOOST_ROOT
export PATH="/opt/python/cp312-cp312/bin:$PATH"

mkdir -p "$WORK/src" "$WORK/build" "$WORK/install" "$DIST" "$REPAIRED"
cd "$ROOT"

clone_once() {
  local url="$1" tag="$2" dest="$3"
  if [[ ! -d "$dest/.git" ]]; then
    [[ ! -e "$dest" ]] || { echo "Existing non-git directory: $dest" >&2; return 1; }
    git clone --branch "$tag" --depth 1 "$url" "$dest"
  fi
}

stage_vtk() {
  clone_once https://gitlab.kitware.com/vtk/vtk.git "$VTK_VERSION" "$VTK_SOURCE"
  if [[ ! -f "$VTK_SOURCE/.topology_namespace_patched" ]]; then
    python "$PACKAGING_DIR/patch_vtk.py" "$VTK_SOURCE"
    touch "$VTK_SOURCE/.topology_namespace_patched"
  fi
  cmake -S "$VTK_SOURCE" -B "$VTK_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$(command -v gcc)" \
    -DCMAKE_CXX_COMPILER="$(command -v g++)" \
    -DMPI_C_COMPILER="$(command -v mpicc)" \
    -DMPI_CXX_COMPILER="$(command -v mpicxx)" \
    -DVTK_USE_MPI=ON \
    -DVTK_GROUP_ENABLE_StandAlone=DEFAULT \
    -DVTK_GROUP_ENABLE_Rendering=DONT_WANT \
    -DVTK_GROUP_ENABLE_Views=DONT_WANT \
    -DVTK_GROUP_ENABLE_Web=DONT_WANT \
    -DVTK_MODULE_ENABLE_VTK_FiltersGeneral=YES \
    -DVTK_MODULE_ENABLE_VTK_FiltersGeometry=YES \
    -DVTK_MODULE_ENABLE_VTK_FiltersSources=YES \
    -DVTK_MODULE_ENABLE_VTK_IOXML=YES \
    -DVTK_MODULE_ENABLE_VTK_FiltersParallelDIY2=YES \
    -DVTK_MODULE_ENABLE_VTK_ParallelMPI=YES \
    -DVTK_WRAP_PYTHON=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTING=OFF \
    -DVTK_WHEEL_BUILD=ON \
    -DVTK_INSTALL_SDK=ON \
    -DCMAKE_INSTALL_PREFIX="$VTK_INSTALL"
  cmake --build "$VTK_BUILD" --parallel "$JOBS"
  cmake --install "$VTK_BUILD"
  local private_file="$VTK_CMAKE_DIR/topology_toolkit._vtkmodules-vtk-python-module-properties.cmake"
  local compat_file="$VTK_CMAKE_DIR/vtkmodules-vtk-python-module-properties.cmake"
  test -f "$private_file"
  if [[ ! -e "$compat_file" ]]; then
    ln -s "$(basename "$private_file")" "$compat_file"
  fi
}

stage_boost() {
  if [[ -f "$BOOST_ROOT/.topology_boost_installed" ]]; then
    echo "Boost installation exists; skipping. Remove $BOOST_ROOT to rebuild."
    return
  fi
  if [[ ! -f "$BOOST_SOURCE/bootstrap.sh" ]]; then
    mkdir -p "$BOOST_SOURCE"
    curl -fL "https://archives.boost.io/release/$BOOST_VERSION/source/boost_${BOOST_VERSION_U}.tar.bz2" \
      -o "$WORK/src/boost.tar.bz2"
    tar -xjf "$WORK/src/boost.tar.bz2" -C "$BOOST_SOURCE" --strip-components=1
  fi
  (cd "$BOOST_SOURCE" && ./bootstrap.sh --prefix="$BOOST_ROOT" && \
    ./b2 -j"$JOBS" variant=release link=shared threading=multi --without-python install)
  touch "$BOOST_ROOT/.topology_boost_installed"
}

stage_ttk() {
  test -d "$VTK_CMAKE_DIR" || { echo 'Build VTK first' >&2; return 1; }
  # Copy the CURRENT working tree, including uncommitted edits, to avoid
  # modifying tracked source files when patch_ttk.py runs.
  test -f "$REPO_ROOT/CMakeLists.txt" || { echo "Not a TTK checkout: $REPO_ROOT" >&2; return 1; }
  rm -rf "$TTK_SOURCE"
  REPO_ROOT="$REPO_ROOT" TTK_SOURCE="$TTK_SOURCE" python - <<'PY_COPY'
import os
import shutil
from pathlib import Path
src = Path(os.environ['REPO_ROOT'])
dst = Path(os.environ['TTK_SOURCE'])
shutil.copytree(src, dst, ignore=shutil.ignore_patterns(
    '.git', '.venv', '__pycache__', '.pytest_cache', 'CMakeFiles',
    'build', 'work', '*.pyc'))
print('Copied current TTK working tree:', src, '->', dst)
PY_COPY
  python "$PACKAGING_DIR/patch_ttk.py" "$TTK_SOURCE"
  local module cmake_exclusions=()
  local exclusions_file="${TTK_DISABLED_MODULES_FILE:-$PACKAGING_DIR/ttk-disabled-modules.txt}"
  [[ -f "$exclusions_file" ]] || {
    echo "Missing TTK module exclusions: $exclusions_file" >&2
    echo "Copy the original ttk-disabled-modules.txt into packaging/" >&2
    return 1
  }
  while IFS= read -r module || [[ -n "$module" ]]; do
    case "$module" in ''|\#*) continue ;; esac
    cmake_exclusions+=("-DVTK_MODULE_ENABLE_${module}=DONT_WANT")
  done < "$exclusions_file"
  cmake -S "$TTK_SOURCE" -B "$TTK_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$VTK_INSTALL;$BOOST_ROOT" \
    -DCMAKE_INSTALL_PREFIX="$TTK_INSTALL" \
    -DCMAKE_C_COMPILER="$(command -v gcc)" \
    -DCMAKE_CXX_COMPILER="$(command -v g++)" \
    -DMPI_C_COMPILER="$(command -v mpicc)" \
    -DMPI_CXX_COMPILER="$(command -v mpicxx)" \
    -DTTK_BUILD_VTK_WRAPPERS=ON \
    -DTTK_BUILD_PARAVIEW_PLUGINS=OFF \
    -DTTK_BUILD_STANDALONE_APPS=OFF \
    -DTTK_WHITELIST_MODE=OFF \
    -DTTK_ENABLE_MPI=ON \
    -DTTK_ENABLE_OPENMP=ON \
    -DTTK_ENABLE_CPU_OPTIMIZATION=OFF \
    -DTTK_PYTHON_MODULE_DIR=. \
    -DVTK_DIR="$VTK_CMAKE_DIR" \
    -DBUILD_SHARED_LIBS=ON \
    "${cmake_exclusions[@]}"
  cmake --build "$TTK_BUILD" --parallel "$JOBS"
  cmake --install "$TTK_BUILD"
}

setup_library_path() {
  local paths
  paths="$(find "$VTK_INSTALL" "$TTK_INSTALL" "$BOOST_ROOT" /opt/mpi /opt/openmpi \
    -type f -name '*.so*' -printf '%h\n' 2>/dev/null | sort -u | paste -sd: - || true)"
  export LD_LIBRARY_PATH="${paths}${paths:+:}${LD_LIBRARY_PATH:-}"
}

stage_package() {
  local pkg="$WHEEL_SOURCE/topology_toolkit" mods="$WHEEL_SOURCE/topology_toolkit/_vtkmodules"
  test -d "$VTK_INSTALL/topology_toolkit/_vtkmodules"
  test -d "$TTK_INSTALL/topology_toolkit/_vtkmodules"
  rm -rf "$WHEEL_SOURCE"
  mkdir -p "$mods"
  cp -a "$VTK_INSTALL/topology_toolkit/_vtkmodules/." "$mods/"
  cp -a "$TTK_INSTALL/topology_toolkit/_vtkmodules/." "$mods/"
  cp "$PACKAGING_DIR/package_init.py" "$pkg/__init__.py"
  local directory
  for directory in util numpy_interface; do
    if [[ -d "$VTK_INSTALL/vtkmodules/$directory" ]]; then
      cp -a "$VTK_INSTALL/vtkmodules/$directory" "$mods/"
    fi
  done
  VTK_INIT="$VTK_INSTALL/vtkmodules/__init__.py" DEST_INIT="$mods/__init__.py" python - <<'PY'
import os
from pathlib import Path
source = Path(os.environ['VTK_INIT'])
destination = Path(os.environ['DEST_INIT'])
text = source.read_text()
for old, new in (
    ('"vtkmodules.vtk', '"topology_toolkit._vtkmodules.vtk'),
    ('"vtkmodules.numpy_interface', '"topology_toolkit._vtkmodules.numpy_interface'),
    ('"vtkmodules.util', '"topology_toolkit._vtkmodules.util'),
):
    text = text.replace(old, new)
assert 'def on_vtk_module_init(' in text
assert 'def on_vtk_module_init_completed(' in text
destination.write_text(text)
print('Installed generated VTK initializer:', destination)
PY
  cp "$PACKAGING_DIR/pyproject.toml" "$WHEEL_SOURCE/"
  local name
  for name in vtkCommonCore vtkFiltersCore vtkParallelMPI ttkIcosphere; do
    compgen -G "$mods/$name*.so" >/dev/null || { echo "Missing native extension: $name" >&2; return 1; }
  done
  setup_library_path
  (cd "$WHEEL_SOURCE" && PYTHONPATH="$WHEEL_SOURCE" python -X faulthandler -c \
    "import topology_toolkit; print('Assembled package import OK')")
  (cd "$WHEEL_SOURCE" && python -m build --wheel --no-isolation --outdir "$DIST")
}

stage_repair() {
  setup_library_path
  local wheels=("$DIST"/*.whl)
  [[ -f "${wheels[0]}" ]] || { echo "No wheel in $DIST" >&2; return 1; }
  auditwheel repair --plat manylinux_2_28_x86_64 --wheel-dir "$REPAIRED" "${wheels[@]}"
  auditwheel show "$REPAIRED"/*.whl
}

usage() {
  echo "Usage: $0 [all|vtk|boost|ttk|package|repair]" >&2
  echo "Env: BUILD_ROOT=<outside-repo-dir> BUILD_JOBS=8 TTK_DISABLED_MODULES_FILE=<path>" >&2
}

case "${1:-all}" in
  all) stage_vtk; stage_boost; stage_ttk; stage_package; stage_repair ;;
  vtk) stage_vtk ;;
  boost) stage_boost ;;
  ttk) stage_ttk ;;
  package) stage_package ;;
  repair) stage_repair ;;
  *) usage; exit 2 ;;
esac
