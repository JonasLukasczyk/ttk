/// \ingroup vtk
/// \class ttkTrackingFromLabels
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.09.2018
///
/// \brief TTK VTK-filter that computes the overlap between labeled vtkPointSets.
///
/// VTK wrapping code for the @TrackingFromLabels package.
///
/// This filter identifies and tracks labled vtkPointSets across time (and optionally levels) based on spatial overlap, where two points overlap iff their corresponding coordinates are equal. This filter can be executed iteratively and can generate nested tracking graphs.
///
/// This filter can be used as any other VTK filter (for instance, by using the
/// sequence of calls SetInputData(), Update(), GetOutput()).
///
/// See the related ParaView example state files for usage examples within a
/// VTK pipeline.
///
/// \b Related \b publication: \n
/// 'Nested Tracking Graphs'.
/// Jonas Lukasczyk, Gunther Weber, Ross Maciejewski, Christoph Garth, and Heike Leitte.
/// Computer Graphics Forum (Special Issue, Proceedings Eurographics / IEEE Symposium on Visualization). Vol. 36. No. 3. 2017.
///
/// \param Input A \b vtkMultiBlockDataSet that holds the labeled \b vtkPointSets and has one of the following forms:\n{t_0,...,t_n} or {l_0:{t_0,...,t_n}, ... , l_m:{t_0,...,t_n}} where \b t_i is the \b vtkPointSet of timestep \b i, and \b l_j is a \b vtkMultiBlockDataSet that holds all timesteps of level \b j.
/// \param Output A \b vtkUnstructuredGrid that represents the (nested) tracking graph embedded in the spatial domain.
///
/// sa ttk::TrackingFromLabels

#pragma once

// VTK Module
#include <ttkTrackingFromLabelsModule.h>

// VTK Includes
#include <ttkAlgorithm.h>

// TTK Base Includes
#include <TrackingFromLabels.h>

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkMultiBlockDataSet.h>

class TTKTRACKINGFROMLABELS_EXPORT ttkTrackingFromLabels
  : public ttkAlgorithm
  , public ttk::TrackingFromLabels
{
    public:
        static ttkTrackingFromLabels* New();
        vtkTypeMacro(ttkTrackingFromLabels, ttkAlgorithm);

    protected:

        ttkTrackingFromLabels();
        ~ttkTrackingFromLabels();

        int reset();

        int packInputData(vtkDataObject* inputDataObject, vtkMultiBlockDataSet* packedData) const;
        int packStreamedData(vtkMultiBlockDataSet* streamedData, vtkMultiBlockDataSet* packedData) const;
        int checkData(vtkMultiBlockDataSet* data);

        int storeStreamedData(vtkMultiBlockDataSet* data);
        int computeNodes(vtkMultiBlockDataSet* data);
        int computeTrackingGraphs(vtkMultiBlockDataSet* data);
        int computeNestingTrees(vtkMultiBlockDataSet* data);
        int computeBranches();

        int meshNestedTrackingGraph(vtkDataObject* trackingGraph);

        int FillInputPortInformation(int port, vtkInformation *info) override;
        int FillOutputPortInformation(int port, vtkInformation *info) override;
        int RequestData(vtkInformation *request,
          vtkInformationVector **inputVector,
          vtkInformationVector *outputVector) override;

    private:

        int LabelDataType;
        std::string LabelArrayName;

        vtkSmartPointer<vtkMultiBlockDataSet> previousIterationData;

        // Containers for nodes and edges
        std::vector<std::vector<std::vector<
            ttk::TrackingFromLabels::Node
        >>> levelTimeNodesMap; // N
        std::vector<std::vector<std::vector<
            ttk::TrackingFromLabels::Edge
        >>> levelTimeEdgesTMap; // E_T
        std::vector<std::vector<std::vector<
            ttk::TrackingFromLabels::Edge
        >>> timeLevelEdgesNMap; // E_N
};
