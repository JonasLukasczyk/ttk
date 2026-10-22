#!/usr/bin/env python3
"""Functional smoke test for an installed topology_toolkit wheel."""

import topology_toolkit
from topology_toolkit._vtkmodules import ttkIcosphere, vtkFiltersCore


def main():
    sphere = ttkIcosphere.ttkIcosphere()
    sphere.SetNumberOfSubdivisions(3)
    sphere.Update()
    mesh = sphere.GetOutput()
    assert mesh.GetNumberOfPoints() == 642, mesh.GetNumberOfPoints()
    assert mesh.GetNumberOfCells() == 1280, mesh.GetNumberOfCells()

    bounds = mesh.GetBounds()
    elevation = vtkFiltersCore.vtkElevationFilter()
    elevation.SetInputConnection(sphere.GetOutputPort())
    elevation.SetLowPoint(0.0, 0.0, bounds[4])
    elevation.SetHighPoint(0.0, 0.0, bounds[5])
    elevation.Update()
    scalars = elevation.GetOutput().GetPointData().GetScalars()
    assert scalars is not None, 'Missing elevation scalars'
    lo, hi = scalars.GetRange()
    assert abs(lo) < 1e-6 and abs(hi - 1.0) < 1e-6, (lo, hi)

    contour = vtkFiltersCore.vtkContourFilter()
    contour.SetInputConnection(elevation.GetOutputPort())
    contour.SetValue(0, 0.5)
    contour.Update()
    result = contour.GetOutput()
    assert result.GetNumberOfPoints() == 48, result.GetNumberOfPoints()
    assert result.GetNumberOfCells() == 48, result.GetNumberOfCells()
    print('PASS:', topology_toolkit.__file__)
    print('Icosphere: 642 points, 1280 cells; elevation: [0, 1]; contour: 48 points, 48 cells')


if __name__ == '__main__':
    main()
