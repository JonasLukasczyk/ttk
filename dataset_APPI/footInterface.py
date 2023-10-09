# state file generated using paraview version 5.11.1-1608-g13d096284f
import paraview
paraview.compatibility.major = 5
paraview.compatibility.minor = 11

#### import the simple module from the paraview
from paraview.simple import *
#### disable automatic camera reset on 'Show'
paraview.simple._DisableFirstRenderCameraReset()

# ----------------------------------------------------------------
# setup views used in the visualization
# ----------------------------------------------------------------

# Create a new 'Render View'
renderView1 = CreateView('RenderView')
renderView1.ViewSize = [1804, 1135]
renderView1.AxesGrid = 'Grid Axes 3D Actor'
renderView1.CenterOfRotation = [127.5, 127.5, 0.0]
renderView1.StereoType = 'Crystal Eyes'
renderView1.CameraPosition = [870.9452394365429, 842.5918619419407, 66.42829433518074]
renderView1.CameraFocalPoint = [123.25339230438367, 131.50261305240502, 5.388965602684944]
renderView1.CameraViewUp = [-0.6703438867935244, 0.7201946411753043, -0.17877011008813912]
renderView1.CameraFocalDisk = 1.0
renderView1.CameraParallelScale = 148.8740079440503
renderView1.LegendGrid = 'Legend Grid Actor'

SetActiveView(None)

# ----------------------------------------------------------------
# setup view layouts
# ----------------------------------------------------------------

# create new layout object 'Layout #1'
layout1 = CreateLayout(name='Layout #1')
layout1.AssignView(0, renderView1)
layout1.SetSize(1804, 1135)

# ----------------------------------------------------------------
# restore active view
SetActiveView(renderView1)
# ----------------------------------------------------------------

# ----------------------------------------------------------------
# setup the data processing pipelines
# ----------------------------------------------------------------

# create a new 'XML Unstructured Grid Reader'
footPPvtu = XMLUnstructuredGridReader(registrationName='footPP.vtu', FileName=['/home/jones/projects/ttk/dataset_APPI/footPP.vtu'])
footPPvtu.CellArrayStatus = ['PairIdentifier', 'PairType', 'Persistence']
footPPvtu.PointArrayStatus = ['ttkVertexScalarField', 'CriticalType', 'Coordinates', 'Scalars']
footPPvtu.TimeArray = 'None'

# create a new 'TTK BlockAggregator'
tTKBlockAggregator1 = TTKBlockAggregator(registrationName='TTKBlockAggregator1', Input=footPPvtu)

# create a new 'TTK PersistencePairInventory'
tTKPersistencePairInventory1 = TTKPersistencePairInventory(registrationName='TTKPersistencePairInventory1', Input=tTKBlockAggregator1)
tTKPersistencePairInventory1.Scalars = ['POINTS', 'None']
tTKPersistencePairInventory1.Scalars = ['POINTS', 'Scalars']
tTKPersistencePairInventory1.EntireScalarRange = 0
tTKPersistencePairInventory1.ScalarRange = [10.0, 250.0]
tTKPersistencePairInventory1.ScalarValues = 25
tTKPersistencePairInventory1.PersistenceThresholds = 50
tTKPersistencePairInventory1.PersistenceDelta = 5.0

# create a new 'TTK WebSocketIO'
tTKWebSocketIO1 = TTKWebSocketIO(registrationName='TTKWebSocketIO1', Input=tTKPersistencePairInventory1)
tTKWebSocketIO1.PortNumber = 9286

# create a new 'Merge Blocks'
mergeBlocks1 = MergeBlocks(registrationName='MergeBlocks1', Input=tTKWebSocketIO1)
mergeBlocks1.MergePoints = 0

# create a new 'XML Image Data Reader'
ctBonesvti = XMLImageDataReader(registrationName='ctBones.vti', FileName=['/home/jones/projects/ttk/dataset_APPI/ctBones.vti'])
ctBonesvti.PointArrayStatus = ['Scalars_']
ctBonesvti.TimeArray = 'None'

# create a new 'TTK ArrayEditor'
tTKArrayEditor1 = TTKArrayEditor(registrationName='TTKArrayEditor1', Target=mergeBlocks1,
    Source=None)
tTKArrayEditor1.DataString = """actual_scalar,1
actual_time,1
actual_threshold,1"""
tTKArrayEditor1.ReplaceExistingArrays = 0
tTKArrayEditor1.TargetArray = ['FIELD', 'PersistenceCurves']

# create a new 'TTK ArrayEditor'
tTKArrayEditor2 = TTKArrayEditor(registrationName='TTKArrayEditor2', Target=ctBonesvti,
    Source=tTKArrayEditor1)
tTKArrayEditor2.EditorMode = 'Add Arrays from Source'
tTKArrayEditor2.SourceFieldDataArrays = ['actual_scalar', 'actual_threshold', 'actual_time']
tTKArrayEditor2.TargetArray = ['FIELD', 'PersistenceCurves']

# create a new 'TTK TopologicalSimplificationByPersistence'
tTKTopologicalSimplificationByPersistence1 = TTKTopologicalSimplificationByPersistence(registrationName='TTKTopologicalSimplificationByPersistence1', Input=tTKArrayEditor2)
tTKTopologicalSimplificationByPersistence1.InputArray = ['POINTS', 'None']
tTKTopologicalSimplificationByPersistence1.InputArray = ['POINTS', 'Scalars_']
tTKTopologicalSimplificationByPersistence1.PairType = 'Maximum-Saddle'
tTKTopologicalSimplificationByPersistence1.PersistenceThreshold = '{actual_threshold}'
tTKTopologicalSimplificationByPersistence1.ThresholdIsAbsolute = 1

# create a new 'TTK LevelSets'
tTKLevelSets1 = TTKLevelSets(registrationName='TTKLevelSets1', Input=tTKTopologicalSimplificationByPersistence1)
tTKLevelSets1.InputArray = ['POINTS', 'None']
tTKLevelSets1.InputArray = ['POINTS', 'Scalars_']
tTKLevelSets1.LevelSetType = 'Superlevel Set'
tTKLevelSets1.Expression = '{actual_scalar}'

# create a new 'Connectivity'
connectivity1 = Connectivity(registrationName='Connectivity1', Input=tTKLevelSets1)

# create a new 'Calculator'
calculator1 = Calculator(registrationName='Calculator1', Input=connectivity1)
calculator1.ResultArrayName = 'RegionId'
calculator1.Function = 'RegionId - floor(RegionId/10)*10'
calculator1.ResultArrayType = 'Int'

# create a new 'Pass Arrays'
passArrays1 = PassArrays(registrationName='PassArrays1', Input=calculator1)
passArrays1.PointDataArrays = ['RegionId']
passArrays1.FieldDataArrays = ['actual_scalar', 'actual_threshold', 'actual_time']

# create a new 'TTK WebSocketIO'
tTKWebSocketIO2 = TTKWebSocketIO(registrationName='TTKWebSocketIO2', Input=passArrays1)
tTKWebSocketIO2.PortNumber = 9289

# ----------------------------------------------------------------
# setup the visualization in view 'renderView1'
# ----------------------------------------------------------------

# show data from ctBonesvti
ctBonesvtiDisplay = Show(ctBonesvti, renderView1, 'UniformGridRepresentation')

# trace defaults for the display properties.
ctBonesvtiDisplay.Representation = 'Outline'
ctBonesvtiDisplay.ColorArrayName = ['POINTS', '']
ctBonesvtiDisplay.SelectTCoordArray = 'None'
ctBonesvtiDisplay.SelectNormalArray = 'None'
ctBonesvtiDisplay.SelectTangentArray = 'None'
ctBonesvtiDisplay.OSPRayScaleArray = 'Scalars_'
ctBonesvtiDisplay.OSPRayScaleFunction = 'Piecewise Function'
ctBonesvtiDisplay.Assembly = ''
ctBonesvtiDisplay.SelectOrientationVectors = 'None'
ctBonesvtiDisplay.ScaleFactor = 25.5
ctBonesvtiDisplay.SelectScaleArray = 'Scalars_'
ctBonesvtiDisplay.GlyphType = 'Arrow'
ctBonesvtiDisplay.GlyphTableIndexArray = 'Scalars_'
ctBonesvtiDisplay.GaussianRadius = 1.2750000000000001
ctBonesvtiDisplay.SetScaleArray = ['POINTS', 'Scalars_']
ctBonesvtiDisplay.ScaleTransferFunction = 'Piecewise Function'
ctBonesvtiDisplay.OpacityArray = ['POINTS', 'Scalars_']
ctBonesvtiDisplay.OpacityTransferFunction = 'Piecewise Function'
ctBonesvtiDisplay.DataAxesGrid = 'Grid Axes Representation'
ctBonesvtiDisplay.PolarAxes = 'Polar Axes Representation'
ctBonesvtiDisplay.ScalarOpacityUnitDistance = 1.7320508075688774
ctBonesvtiDisplay.OpacityArrayName = ['POINTS', 'Scalars_']
ctBonesvtiDisplay.ColorArray2Name = ['POINTS', 'Scalars_']
ctBonesvtiDisplay.IsosurfaceValues = [127.5]
ctBonesvtiDisplay.SliceFunction = 'Plane'
ctBonesvtiDisplay.Slice = 127
ctBonesvtiDisplay.SelectInputVectors = ['POINTS', '']
ctBonesvtiDisplay.WriteLog = ''

# init the 'Piecewise Function' selected for 'ScaleTransferFunction'
ctBonesvtiDisplay.ScaleTransferFunction.Points = [0.0, 0.0, 0.5, 0.0, 255.0, 1.0, 0.5, 0.0]

# init the 'Piecewise Function' selected for 'OpacityTransferFunction'
ctBonesvtiDisplay.OpacityTransferFunction.Points = [0.0, 0.0, 0.5, 0.0, 255.0, 1.0, 0.5, 0.0]

# init the 'Polar Axes Representation' selected for 'PolarAxes'
ctBonesvtiDisplay.PolarAxes.EnableOverallColor = 0
ctBonesvtiDisplay.PolarAxes.ArcTickMatchesRadialAxes = 0

# init the 'Plane' selected for 'SliceFunction'
ctBonesvtiDisplay.SliceFunction.Origin = [127.5, 127.5, 127.5]

# show data from passArrays1
passArrays1Display = Show(passArrays1, renderView1, 'UnstructuredGridRepresentation')

# get 2D transfer function for 'RegionId'
regionIdTF2D = GetTransferFunction2D('RegionId')

# get color transfer function/color map for 'RegionId'
regionIdLUT = GetColorTransferFunction('RegionId')
regionIdLUT.TransferFunction2D = regionIdTF2D
regionIdLUT.RGBPoints = [0.0, 0.231373, 0.298039, 0.752941, 602.0, 0.865003, 0.865003, 0.865003, 1204.0, 0.705882, 0.0156863, 0.14902]
regionIdLUT.ScalarRangeInitialized = 1.0

# get opacity transfer function/opacity map for 'RegionId'
regionIdPWF = GetOpacityTransferFunction('RegionId')
regionIdPWF.Points = [0.0, 0.0, 0.5, 0.0, 1204.0, 1.0, 0.5, 0.0]
regionIdPWF.ScalarRangeInitialized = 1

# trace defaults for the display properties.
passArrays1Display.Representation = 'Surface'
passArrays1Display.ColorArrayName = ['POINTS', 'RegionId']
passArrays1Display.LookupTable = regionIdLUT
passArrays1Display.SelectTCoordArray = 'None'
passArrays1Display.SelectNormalArray = 'None'
passArrays1Display.SelectTangentArray = 'None'
passArrays1Display.OSPRayScaleArray = 'RegionId'
passArrays1Display.OSPRayScaleFunction = 'Piecewise Function'
passArrays1Display.Assembly = ''
passArrays1Display.SelectOrientationVectors = 'None'
passArrays1Display.ScaleFactor = 25.395
passArrays1Display.SelectScaleArray = 'RegionId'
passArrays1Display.GlyphType = 'Arrow'
passArrays1Display.GlyphTableIndexArray = 'RegionId'
passArrays1Display.GaussianRadius = 1.26975
passArrays1Display.SetScaleArray = ['POINTS', 'RegionId']
passArrays1Display.ScaleTransferFunction = 'Piecewise Function'
passArrays1Display.OpacityArray = ['POINTS', 'RegionId']
passArrays1Display.OpacityTransferFunction = 'Piecewise Function'
passArrays1Display.DataAxesGrid = 'Grid Axes Representation'
passArrays1Display.PolarAxes = 'Polar Axes Representation'
passArrays1Display.ScalarOpacityFunction = regionIdPWF
passArrays1Display.ScalarOpacityUnitDistance = 4.038149777403242
passArrays1Display.OpacityArrayName = ['POINTS', 'RegionId']
passArrays1Display.SelectInputVectors = ['POINTS', '']
passArrays1Display.WriteLog = ''

# init the 'Piecewise Function' selected for 'ScaleTransferFunction'
passArrays1Display.ScaleTransferFunction.Points = [0.0, 0.0, 0.5, 0.0, 9.0, 1.0, 0.5, 0.0]

# init the 'Piecewise Function' selected for 'OpacityTransferFunction'
passArrays1Display.OpacityTransferFunction.Points = [0.0, 0.0, 0.5, 0.0, 9.0, 1.0, 0.5, 0.0]

# init the 'Polar Axes Representation' selected for 'PolarAxes'
passArrays1Display.PolarAxes.EnableOverallColor = 0
passArrays1Display.PolarAxes.ArcTickMatchesRadialAxes = 0

# show data from tTKWebSocketIO2
tTKWebSocketIO2Display = Show(tTKWebSocketIO2, renderView1, 'GeometryRepresentation')

# trace defaults for the display properties.
tTKWebSocketIO2Display.Representation = 'Surface'
tTKWebSocketIO2Display.ColorArrayName = ['POINTS', '']
tTKWebSocketIO2Display.SelectTCoordArray = 'None'
tTKWebSocketIO2Display.SelectNormalArray = 'None'
tTKWebSocketIO2Display.SelectTangentArray = 'None'
tTKWebSocketIO2Display.OSPRayScaleFunction = 'Piecewise Function'
tTKWebSocketIO2Display.Assembly = 'Hierarchy'
tTKWebSocketIO2Display.SelectOrientationVectors = 'None'
tTKWebSocketIO2Display.ScaleFactor = -2.0000000000000002e+298
tTKWebSocketIO2Display.SelectScaleArray = 'None'
tTKWebSocketIO2Display.GlyphType = 'Arrow'
tTKWebSocketIO2Display.GlyphTableIndexArray = 'None'
tTKWebSocketIO2Display.GaussianRadius = -1e+297
tTKWebSocketIO2Display.SetScaleArray = ['POINTS', '']
tTKWebSocketIO2Display.ScaleTransferFunction = 'Piecewise Function'
tTKWebSocketIO2Display.OpacityArray = ['POINTS', '']
tTKWebSocketIO2Display.OpacityTransferFunction = 'Piecewise Function'
tTKWebSocketIO2Display.DataAxesGrid = 'Grid Axes Representation'
tTKWebSocketIO2Display.PolarAxes = 'Polar Axes Representation'
tTKWebSocketIO2Display.SelectInputVectors = ['POINTS', '']
tTKWebSocketIO2Display.WriteLog = ''

# init the 'Polar Axes Representation' selected for 'PolarAxes'
tTKWebSocketIO2Display.PolarAxes.EnableOverallColor = 0
tTKWebSocketIO2Display.PolarAxes.ArcTickMatchesRadialAxes = 0

# show data from tTKArrayEditor1
tTKArrayEditor1Display = Show(tTKArrayEditor1, renderView1, 'UnstructuredGridRepresentation')

# trace defaults for the display properties.
tTKArrayEditor1Display.Representation = 'Surface'
tTKArrayEditor1Display.ColorArrayName = ['POINTS', '']
tTKArrayEditor1Display.SelectTCoordArray = 'None'
tTKArrayEditor1Display.SelectNormalArray = 'None'
tTKArrayEditor1Display.SelectTangentArray = 'None'
tTKArrayEditor1Display.OSPRayScaleFunction = 'Piecewise Function'
tTKArrayEditor1Display.Assembly = ''
tTKArrayEditor1Display.SelectOrientationVectors = 'None'
tTKArrayEditor1Display.ScaleFactor = -0.2
tTKArrayEditor1Display.SelectScaleArray = 'None'
tTKArrayEditor1Display.GlyphType = 'Arrow'
tTKArrayEditor1Display.GlyphTableIndexArray = 'None'
tTKArrayEditor1Display.GaussianRadius = -0.01
tTKArrayEditor1Display.SetScaleArray = ['POINTS', '']
tTKArrayEditor1Display.ScaleTransferFunction = 'Piecewise Function'
tTKArrayEditor1Display.OpacityArray = ['POINTS', '']
tTKArrayEditor1Display.OpacityTransferFunction = 'Piecewise Function'
tTKArrayEditor1Display.DataAxesGrid = 'Grid Axes Representation'
tTKArrayEditor1Display.PolarAxes = 'Polar Axes Representation'
tTKArrayEditor1Display.OpacityArrayName = ['FIELD', 'PPI']
tTKArrayEditor1Display.SelectInputVectors = ['POINTS', '']
tTKArrayEditor1Display.WriteLog = ''

# init the 'Polar Axes Representation' selected for 'PolarAxes'
tTKArrayEditor1Display.PolarAxes.EnableOverallColor = 0
tTKArrayEditor1Display.PolarAxes.ArcTickMatchesRadialAxes = 0

# show data from tTKArrayEditor2
tTKArrayEditor2Display = Show(tTKArrayEditor2, renderView1, 'UniformGridRepresentation')

# trace defaults for the display properties.
tTKArrayEditor2Display.Representation = 'Outline'
tTKArrayEditor2Display.ColorArrayName = ['POINTS', '']
tTKArrayEditor2Display.SelectTCoordArray = 'None'
tTKArrayEditor2Display.SelectNormalArray = 'None'
tTKArrayEditor2Display.SelectTangentArray = 'None'
tTKArrayEditor2Display.OSPRayScaleArray = 'Scalars_'
tTKArrayEditor2Display.OSPRayScaleFunction = 'Piecewise Function'
tTKArrayEditor2Display.Assembly = ''
tTKArrayEditor2Display.SelectOrientationVectors = 'None'
tTKArrayEditor2Display.ScaleFactor = 25.5
tTKArrayEditor2Display.SelectScaleArray = 'Scalars_'
tTKArrayEditor2Display.GlyphType = 'Arrow'
tTKArrayEditor2Display.GlyphTableIndexArray = 'Scalars_'
tTKArrayEditor2Display.GaussianRadius = 1.2750000000000001
tTKArrayEditor2Display.SetScaleArray = ['POINTS', 'Scalars_']
tTKArrayEditor2Display.ScaleTransferFunction = 'Piecewise Function'
tTKArrayEditor2Display.OpacityArray = ['POINTS', 'Scalars_']
tTKArrayEditor2Display.OpacityTransferFunction = 'Piecewise Function'
tTKArrayEditor2Display.DataAxesGrid = 'Grid Axes Representation'
tTKArrayEditor2Display.PolarAxes = 'Polar Axes Representation'
tTKArrayEditor2Display.ScalarOpacityUnitDistance = 1.7320508075688774
tTKArrayEditor2Display.OpacityArrayName = ['POINTS', 'Scalars_']
tTKArrayEditor2Display.ColorArray2Name = ['POINTS', 'Scalars_']
tTKArrayEditor2Display.IsosurfaceValues = [127.5]
tTKArrayEditor2Display.SliceFunction = 'Plane'
tTKArrayEditor2Display.Slice = 127
tTKArrayEditor2Display.SelectInputVectors = ['POINTS', '']
tTKArrayEditor2Display.WriteLog = ''

# init the 'Piecewise Function' selected for 'ScaleTransferFunction'
tTKArrayEditor2Display.ScaleTransferFunction.Points = [0.0, 0.0, 0.5, 0.0, 255.0, 1.0, 0.5, 0.0]

# init the 'Piecewise Function' selected for 'OpacityTransferFunction'
tTKArrayEditor2Display.OpacityTransferFunction.Points = [0.0, 0.0, 0.5, 0.0, 255.0, 1.0, 0.5, 0.0]

# init the 'Polar Axes Representation' selected for 'PolarAxes'
tTKArrayEditor2Display.PolarAxes.EnableOverallColor = 0
tTKArrayEditor2Display.PolarAxes.ArcTickMatchesRadialAxes = 0

# init the 'Plane' selected for 'SliceFunction'
tTKArrayEditor2Display.SliceFunction.Origin = [127.5, 127.5, 127.5]

# setup the color legend parameters for each legend in this view

# get color legend/bar for regionIdLUT in view renderView1
regionIdLUTColorBar = GetScalarBar(regionIdLUT, renderView1)
regionIdLUTColorBar.WindowLocation = 'Upper Right Corner'
regionIdLUTColorBar.Title = 'RegionId'
regionIdLUTColorBar.ComponentTitle = ''

# set color bar visibility
regionIdLUTColorBar.Visibility = 1

# show color legend
passArrays1Display.SetScalarBarVisibility(renderView1, True)

# ----------------------------------------------------------------
# setup color maps and opacity maps used in the visualization
# note: the Get..() functions create a new object, if needed
# ----------------------------------------------------------------

# ----------------------------------------------------------------
# setup animation scene, tracks and keyframes
# note: the Get..() functions create a new object, if needed
# ----------------------------------------------------------------

# get the time-keeper
timeKeeper1 = GetTimeKeeper()

# initialize the timekeeper

# get time animation track
timeAnimationCue1 = GetTimeTrack()

# initialize the animation track

# get animation scene
animationScene1 = GetAnimationScene()

# initialize the animation scene
animationScene1.ViewModules = renderView1
animationScene1.Cues = timeAnimationCue1
animationScene1.AnimationTime = 0.0

# initialize the animation scene

# ----------------------------------------------------------------
# restore active source
SetActiveSource(tTKLevelSets1)
# ----------------------------------------------------------------


##--------------------------------------------
## You may need to add some code at the end of this python script depending on your usage, eg:
#
## Render all views to see them appears
# RenderAllViews()
#
## Interact with the view, usefull when running from pvpython
# Interact()
#
## Save a screenshot of the active view
# SaveScreenshot("path/to/screenshot.png")
#
## Save a screenshot of a layout (multiple splitted view)
# SaveScreenshot("path/to/screenshot.png", GetLayout())
#
## Save all "Extractors" from the pipeline browser
# SaveExtracts()
#
## Save a animation of the current active view
# SaveAnimation()
#
## Please refer to the documentation of paraview.simple
## https://kitware.github.io/paraview-docs/latest/python/paraview.simple.html
##--------------------------------------------
