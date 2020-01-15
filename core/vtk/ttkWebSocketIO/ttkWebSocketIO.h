/// \ingroup vtk
/// \class ttkWebSocketIO
/// \author Jonas Lukasczyk <jl@jluk.der>
/// \date 01.09.2019.
///
/// TODO

#pragma once

// VTK Module
#include <ttkWebSocketIOModule.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkImageData.h>

// VTK Includes
#include <vtkInformation.h>
#include <ttkAlgorithm.h>

// TTK Base Includes
#include <WebSocketIO.h>

class TTKWEBSOCKETIO_EXPORT ttkWebSocketIO
    : public ttkAlgorithm
    , public ttk::WebSocketIO
{
    public:
        static ttkWebSocketIO *New();
        vtkTypeMacro(ttkWebSocketIO, ttkAlgorithm);

        vtkSetMacro(PortNumber, int);
        vtkGetMacro(PortNumber, int);

        vtkSetMacro(NeedsUpdate, bool);
        vtkGetMacro(NeedsUpdate, bool);

        void processClientRequest(std::string, std::string payload="") override;

    protected:
        ttkWebSocketIO();
        ~ttkWebSocketIO();

        int FillInputPortInformation(int port, vtkInformation* info) override;
        int FillOutputPortInformation(int port, vtkInformation* info) override;

        int RequestData(
            vtkInformation* request,
            vtkInformationVector** inputVector,
            vtkInformationVector* outputVector
        ) override;

        int CreateUnstructuredGrid( std::string json );

    private:
        int PortNumber;
        int structureType = 1 ; // 1: vtkUnstructuredGrid, 2: vtkImageData
        vtkSmartPointer<vtkUnstructuredGrid> lastInput;
        vtkSmartPointer<vtkUnstructuredGrid> lastUGfromClient;

        vtkSmartPointer<vtkImageData> lastImageInput;
        vtkSmartPointer<vtkImageData> lastImageUGfromClient;

        bool NeedsUpdate;

};