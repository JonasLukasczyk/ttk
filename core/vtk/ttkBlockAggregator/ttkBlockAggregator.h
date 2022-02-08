/// \ingroup vtk
/// \class ttkBlockAggregator
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.11.2020
///
/// \brief TTK VTK-filter that appends every input vtkDataObject as a block to
/// an output vtkMultiBlockDataSet.
///
/// This filter appends every input vtkDataObject as a block to an output
/// vtkMultiBlockDataSet.
///
/// \param Input vtkDataObject that will be added as a block.
/// \param Output vtkMultiBlockDataSet containing all added blocks.
/// \sa ttkAlgorithm

#pragma once

// VTK Module
#include <ttkBlockAggregatorModule.h>

// VTK Includes
#include <ttkAlgorithm.h>
#include <vtkSmartPointer.h>

class vtkMultiBlockDataSet;

class TTKBLOCKAGGREGATOR_EXPORT ttkBlockAggregator : public ttkAlgorithm {
private:
  bool EnableStreaming{true};
  // bool MergeStream{false};
  bool MergeInputs{false};
  vtkSmartPointer<vtkMultiBlockDataSet> OutputMultiBlockDataSet;

public:
  vtkSetMacro(EnableStreaming, bool);
  vtkGetMacro(EnableStreaming, bool);
  // vtkSetMacro(MergeStream, bool);
  // vtkGetMacro(MergeStream, bool);
  vtkSetMacro(MergeInputs, bool);
  vtkGetMacro(MergeInputs, bool);

  static ttkBlockAggregator *New();
  vtkTypeMacro(ttkBlockAggregator, ttkAlgorithm);

  int Reset();

protected:
  ttkBlockAggregator();
  ~ttkBlockAggregator() override;

  int FillInputPortInformation(int port, vtkInformation *info) override;
  int FillOutputPortInformation(int port, vtkInformation *info) override;
  int RequestData(vtkInformation *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
  int AggregateBlock(vtkDataObject *dataObject);
};
