import tempfile
from pathlib import Path
from urllib.request import urlretrieve

import vtk
import topologytoolkit as ttk


URL = (
    "https://github.com/topology-tool-kit/ttk-data/"
    "raw/refs/heads/dev/dragon.vtu"
)


def main():
    # -------------------------------------------------------------------------
    # Download dragon
    # -------------------------------------------------------------------------

    with tempfile.TemporaryDirectory() as tmpdir:
        filename = Path(tmpdir) / "dragon.vtu"

        print(f"Downloading {URL}")
        urlretrieve(URL, filename)

        # ---------------------------------------------------------------------
        # Read VTU
        # ---------------------------------------------------------------------

        reader = vtk.vtkXMLUnstructuredGridReader()
        reader.SetFileName(str(filename))
        reader.Update()

        dragon = reader.GetOutput()

        print("VTK:", vtk.vtkVersion.GetVTKVersion())
        print("Dragon points:", dragon.GetNumberOfPoints())
        print("Dragon cells:", dragon.GetNumberOfCells())

        bounds = dragon.GetBounds()
        xmin, xmax, ymin, ymax, zmin, zmax = bounds

        print("Bounds:", bounds)

        # ---------------------------------------------------------------------
        # Elevation along Y axis
        # ---------------------------------------------------------------------

        elevation = vtk.vtkElevationFilter()
        elevation.SetInputConnection(reader.GetOutputPort())

        elevation.SetLowPoint(0.0, ymin, 0.0)
        elevation.SetHighPoint(0.0, ymax, 0.0)

        # Make the scalar values correspond to the actual Y-coordinate range.
        elevation.SetScalarRange(ymin, ymax)

        elevation.Update()

        elevated_dragon = elevation.GetOutput()

        height_array = elevated_dragon.GetPointData().GetArray("Elevation")

        if height_array is None:
            raise RuntimeError(
                "vtkElevationFilter did not produce an 'Elevation' array"
            )

        height_range = height_array.GetRange()

        print("Elevation range:", height_range)

        # ---------------------------------------------------------------------
        # Compute critical points
        # ---------------------------------------------------------------------

        critical_points = ttk.ttkScalarFieldCriticalPoints()
        critical_points.SetInputConnection(elevation.GetOutputPort())

        critical_points.SetInputArrayToProcess(
            0,
            0,
            0,
            vtk.vtkDataObject.FIELD_ASSOCIATION_POINTS,
            "Elevation",
        )

        critical_points.Update()

        critical_output = critical_points.GetOutput()

        print(
            "Critical points:",
            critical_output.GetNumberOfPoints(),
        )

        print("Critical-point arrays:")
        critical_pd = critical_output.GetPointData()

        for i in range(critical_pd.GetNumberOfArrays()):
            print(" ", critical_pd.GetArrayName(i))

        critical_type = critical_pd.GetArray("CriticalType")

        if critical_type is None:
            raise RuntimeError(
                "ttkScalarFieldCriticalPoints did not produce "
                "'CriticalType'"
            )

        # ---------------------------------------------------------------------
        # Convert critical points to icospheres
        # ---------------------------------------------------------------------

        spheres = ttk.ttkIcospheresFromPoints()
        spheres.SetInputConnection(critical_points.GetOutputPort())

        diagonal = (
            (xmax - xmin) ** 2
            + (ymax - ymin) ** 2
            + (zmax - zmin) ** 2
        ) ** 0.5

        spheres.SetRadius(0.015 * diagonal)
        spheres.Update()

        sphere_output = spheres.GetOutput()

        print(
            "Sphere output points:",
            sphere_output.GetNumberOfPoints(),
        )
        print(
            "Sphere output cells:",
            sphere_output.GetNumberOfCells(),
        )

        print("Sphere arrays:")
        sphere_pd = sphere_output.GetPointData()

        for i in range(sphere_pd.GetNumberOfArrays()):
            print(" ", sphere_pd.GetArrayName(i))

        # ---------------------------------------------------------------------
        # Extract dragon surface
        # ---------------------------------------------------------------------

        surface = vtk.vtkDataSetSurfaceFilter()
        surface.SetInputConnection(elevation.GetOutputPort())

        # ---------------------------------------------------------------------
        # Dragon height color map
        # ---------------------------------------------------------------------

        height_lut = vtk.vtkLookupTable()
        height_lut.SetNumberOfTableValues(256)
        height_lut.SetRange(height_range)

        # Blue -> red height map
        height_lut.SetHueRange(0.667, 0.0)
        height_lut.Build()

        # ---------------------------------------------------------------------
        # Dragon actor
        # ---------------------------------------------------------------------

        dragon_mapper = vtk.vtkPolyDataMapper()
        dragon_mapper.SetInputConnection(surface.GetOutputPort())
        dragon_mapper.SetScalarModeToUsePointFieldData()
        dragon_mapper.SelectColorArray("Elevation")
        dragon_mapper.SetScalarRange(height_range)
        dragon_mapper.SetLookupTable(height_lut)
        dragon_mapper.ScalarVisibilityOn()

        dragon_actor = vtk.vtkActor()
        dragon_actor.SetMapper(dragon_mapper)

        dragon_actor.GetProperty().SetInterpolationToPhong()
        dragon_actor.GetProperty().SetAmbient(0.15)
        dragon_actor.GetProperty().SetDiffuse(0.8)
        dragon_actor.GetProperty().SetSpecular(0.25)
        dragon_actor.GetProperty().SetSpecularPower(20.0)

        # ---------------------------------------------------------------------
        # Critical point type color map
        #
        # TTK CriticalType:
        #   0 = minimum
        #   1 = 1-saddle
        #   2 = 2-saddle
        #   3 = maximum
        # ---------------------------------------------------------------------

        critical_lut = vtk.vtkLookupTable()
        critical_lut.SetNumberOfTableValues(4)
        critical_lut.SetRange(0, 3)

        # Minimum: blue
        critical_lut.SetTableValue(
            0,
            0.10,
            0.30,
            1.00,
            1.0,
        )

        # 1-saddle: cyan
        critical_lut.SetTableValue(
            1,
            0.20,
            0.85,
            1.00,
            1.0,
        )

        # 2-saddle: yellow/orange
        critical_lut.SetTableValue(
            2,
            1.00,
            0.75,
            0.10,
            1.0,
        )

        # Maximum: red
        critical_lut.SetTableValue(
            3,
            1.00,
            0.15,
            0.10,
            1.0,
        )

        critical_lut.SetAnnotation(0, "Minimum")
        critical_lut.SetAnnotation(1, "1-Saddle")
        critical_lut.SetAnnotation(2, "2-Saddle")
        critical_lut.SetAnnotation(3, "Maximum")

        critical_lut.Build()

        # ---------------------------------------------------------------------
        # Critical point sphere actor
        # ---------------------------------------------------------------------

        sphere_mapper = vtk.vtkPolyDataMapper()
        sphere_mapper.SetInputConnection(spheres.GetOutputPort())

        sphere_mapper.SetScalarModeToUsePointFieldData()
        sphere_mapper.SelectColorArray("CriticalType")
        sphere_mapper.SetScalarRange(0, 3)
        sphere_mapper.SetLookupTable(critical_lut)
        sphere_mapper.ScalarVisibilityOn()

        sphere_actor = vtk.vtkActor()
        sphere_actor.SetMapper(sphere_mapper)

        sphere_actor.GetProperty().SetInterpolationToPhong()
        sphere_actor.GetProperty().SetAmbient(0.25)
        sphere_actor.GetProperty().SetDiffuse(0.8)
        sphere_actor.GetProperty().SetSpecular(0.5)
        sphere_actor.GetProperty().SetSpecularPower(30.0)

        # ---------------------------------------------------------------------
        # Height scalar bar
        # ---------------------------------------------------------------------

        height_bar = vtk.vtkScalarBarActor()
        height_bar.SetLookupTable(height_lut)
        height_bar.SetTitle("Height")
        height_bar.SetNumberOfLabels(5)

        height_bar.SetPosition(0.03, 0.1)
        height_bar.SetWidth(0.1)
        height_bar.SetHeight(0.4)

        # ---------------------------------------------------------------------
        # Critical type scalar bar
        # ---------------------------------------------------------------------

        critical_bar = vtk.vtkScalarBarActor()
        critical_bar.SetLookupTable(critical_lut)
        critical_bar.SetTitle("Critical Type")

        critical_bar.SetPosition(0.82, 0.1)
        critical_bar.SetWidth(0.15)
        critical_bar.SetHeight(0.35)

        # ---------------------------------------------------------------------
        # Renderer
        # ---------------------------------------------------------------------

        renderer = vtk.vtkRenderer()

        renderer.AddActor(dragon_actor)
        renderer.AddActor(sphere_actor)

        renderer.AddActor2D(height_bar)
        renderer.AddActor2D(critical_bar)

        renderer.SetBackground(0.08, 0.08, 0.10)

        renderer.ResetCamera()

        # ---------------------------------------------------------------------
        # Window
        # ---------------------------------------------------------------------

        window = vtk.vtkRenderWindow()
        window.AddRenderer(renderer)
        window.SetSize(1200, 900)
        window.SetWindowName(
            "TTK Dragon: Height Critical Points"
        )

        # ---------------------------------------------------------------------
        # Mouse interaction
        # ---------------------------------------------------------------------

        interactor = vtk.vtkRenderWindowInteractor()
        interactor.SetRenderWindow(window)

        style = vtk.vtkInteractorStyleTrackballCamera()
        interactor.SetInteractorStyle(style)

        # ---------------------------------------------------------------------
        # Render
        # ---------------------------------------------------------------------

        window.Render()

        interactor.Initialize()
        interactor.Start()


if __name__ == "__main__":
    main()
