#include <ttkWebSocketIO.h>

#include <vtkImageData.h>
#include <vtkDataObject.h> // For port info
#include <vtkObjectFactory.h> // for new macro

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkDoubleArray.h>

#include <vtkPointData.h>

#include <vtkPoints.h>
#include <vtkCellArray.h>

#include <ttkWebSocketIOUtils.cpp>

using namespace std;

vtkStandardNewMacro(ttkWebSocketIO);

ttkWebSocketIO::ttkWebSocketIO() :WebSocketIO() {
    this->lastInput = vtkSmartPointer<vtkUnstructuredGrid>::New();
    this->lastUGfromClient = vtkSmartPointer<vtkUnstructuredGrid>::New();

    this->lastImageInput = vtkSmartPointer<vtkImageData>::New();
    this->lastImageUGfromClient = vtkSmartPointer<vtkImageData>::New();

    this->SetNeedsUpdate(true);

    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(1);
}

ttkWebSocketIO::~ttkWebSocketIO() {
    this->printMsg("invoke ~ttkWebSocketIO!") ;
}

// see ttkAlgorithm::FillInputPortInformation for details about this method
int ttkWebSocketIO::FillInputPortInformation(int port, vtkInformation *info) {
    switch (port) {
        case 0:
            info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
            info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
            break;
        default:
            return 0;
    }
    return 1;
}

// see ttkAlgorithm::FillOutputPortInformation for details about this method
int ttkWebSocketIO::FillOutputPortInformation(int port, vtkInformation *info) {
    switch (port) {
        case 0:
            info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkUnstructuredGrid");
            break;
        default:
            return 0;
    }
    return 1;
}

int ttkWebSocketIO::RequestData(
        vtkInformation *request,
        vtkInformationVector **inputVector,
        vtkInformationVector *outputVector
) {
    auto imageInput = vtkImageData::GetData( inputVector[0] );
    auto input = vtkUnstructuredGrid::GetData( inputVector[0] );
    if(imageInput){
        this->structureType = 2 ;
    }

    this->printMsg("invoke RequestData! & Port: " + to_string(this->GetPortNumber()));
    this->SetNeedsUpdate(false);

    if (this->structureType == 1) {
        this->lastInput->ShallowCopy( input );
    } else if (this->structureType == 2) {
        this->lastImageInput->ShallowCopy( imageInput ) ;
    } else {

    }

    if (this->isListening() && this->getPortNumber() != this->PortNumber) {
        this->stopServer() ;
    }

    try {
        // port duplication or other kind of error, return error immediately
        if (!this->isListening()){
            this->startServer( this->PortNumber );
        } else {
            this->processClientRequest("on_update");
        }
    } catch (const std::exception& e) {
        this->printMsg("start Server error: " + string(e.what())) ;
        return 0 ;
    }

    // Get the output
    if (this->structureType == 1) {
        vtkUnstructuredGrid* output = vtkUnstructuredGrid::GetData(outputVector);
        output->ShallowCopy( this->lastUGfromClient );
    } else if (this->structureType == 2) {
        // structure type of output
    }
    return 1;
}

void ttkWebSocketIO::processClientRequest(std::string name, std::string payload){
    this->printMsg("processClientRequest' name: " + name) ;
    if(name.compare("on_open") == 0 || name.compare("on_requestData") == 0 || name.compare("on_update") == 0) {
        std::vector<std::map<string, string>> headers;
        std::vector<void *> sendingData;
        vtkSmartPointer<vtkDataSet> input ;

        // send structureType back to the Client
        signed int *tmp;
        tmp = new int[1];
        tmp[0] = this->structureType ;
        headers.push_back(combineObjectHeader("structureType", 1, 1, VTK_INT));
        sendingData.push_back(tmp);

        if (this->structureType == 1) {  // unStructuredGrid
            input = this->lastInput ;
            int nPoints = input->GetNumberOfPoints();
            auto *pointCoords = (float *) this->lastInput->GetPoints()->GetVoidPointer(0);
            headers.push_back(combineObjectHeader("pointCoords", nPoints, 3, VTK_FLOAT));
            sendingData.push_back(pointCoords);

            int nCells = this->lastInput->GetNumberOfCells();
            auto *connectivityList = (long long *) this->lastInput->GetCells()->GetPointer();
            size_t j = 0, topoIndex = 0;
            for (j = 0, topoIndex = 0; j < nCells; j++) {
                size_t nVertices = connectivityList[topoIndex];
                topoIndex += nVertices + 1;
            }
            headers.push_back(combineObjectHeader("connectivityList", topoIndex, 1, VTK_LONG));
            sendingData.push_back(connectivityList);
        }

        if (this->structureType == 2) { // imageData
            input = this->lastImageInput ;
            signed int *tmp;
            tmp = new int[6];
            this->lastImageInput->GetExtent(tmp);
            headers.push_back(combineObjectHeader("Extent", 6, 1, VTK_INT));
            sendingData.push_back(tmp);
        }

        vtkPointData *inputPD = input->GetPointData();
        int nPointDataArrays = inputPD->GetNumberOfArrays();
        for (int i = 0; i < nPointDataArrays; i++) {
            vtkAbstractArray *array = inputPD->GetAbstractArray(i);
            string string1 = string(array->GetName());
            switch (array->GetDataType()) {
                vtkTemplateMacro({
                        size_t n = array->GetNumberOfValues();
                        auto values = (VTK_TT *) array->GetVoidPointer(0);
                        headers.push_back(combineObjectHeader("PointData:" + string(array->GetName()),
                        array->GetNumberOfTuples(), array->GetNumberOfComponents(),
                        array->GetDataType()));
                        sendingData.push_back(values);});
                case VTK_STRING:
                    auto values = (string *) array->GetVoidPointer(0);
                    headers.push_back(
                            combineObjectHeader("PointData:" + string1, array->GetNumberOfTuples(), array->GetNumberOfComponents(), VTK_STRING));
                    sendingData.push_back(values);
            }
        }

        auto inputCD = (vtkFieldData *) input->GetCellData();
        int nCellDataArrays = inputCD->GetNumberOfArrays();
        for (int i = 0; i < nCellDataArrays; i++) {
            vtkAbstractArray *array = inputCD->GetAbstractArray(i);
            size_t t = array->GetDataType();
            switch (t) {
                vtkTemplateMacro({
                        size_t n = array->GetNumberOfValues();
                        auto values = (VTK_TT *) array->GetVoidPointer(0);
                        headers.push_back(combineObjectHeader("CellData:" + string(array->GetName()),
                        array->GetNumberOfTuples(), array->GetNumberOfComponents(),
                        array->GetDataType()));
                        sendingData.push_back(values);
                                 });
            }
        }

        vtkFieldData *inputFD = input->GetFieldData();  // FLAG, can not load FieldData from ParaView
        int nFieldDataArrays = inputFD->GetNumberOfArrays();
        for (int i = 0; i < nFieldDataArrays; i++) {
            vtkAbstractArray *array = inputFD->GetAbstractArray(i);
            string string1 = string(array->GetName());
            string1.erase(std::remove(string1.begin(), string1.end(), ':'), string1.end());
            switch (array->GetDataType()) {
                vtkTemplateMacro({
                        size_t n = array->GetNumberOfValues();
                        auto values = (VTK_TT *) array->GetVoidPointer(0);
                        headers.push_back(
                        combineObjectHeader("FieldData:" + string1, array->GetNumberOfTuples(), array->GetNumberOfComponents(), array->GetDataType()));
                        sendingData.push_back(values);
                                 });

                case VTK_STRING:
                    auto values = (string *) array->GetVoidPointer(0);
                    headers.push_back(
                            combineObjectHeader("FieldData:" + string1, array->GetNumberOfTuples(), array->GetNumberOfComponents(), VTK_STRING));
                    sendingData.push_back(values);
            }
        }

        this->setHeaders(headers) ;
        this->setObjectState(0) ;
        this->setSendingData(sendingData) ;
        this->sendObject() ;
    } else if (name.compare("updateUnstructuredGrid") == 0) {
        this->printMsg("payload for update structuredGrid:" + payload) ;
        this->CreateUnstructuredGrid( payload ) ;
    } else if (name.compare("updateImageData") == 0) {
        this->printMsg("payload for update ImageData:" + payload) ;
    } else {
        // other cases, none for now
    }
}

int ttkWebSocketIO::CreateUnstructuredGrid( std::string json ) {
    this->printMsg("invoke CreateUnstructuredGrid, receive JSON: " + json) ;

    if ( json.empty() ) {
        this->printMsg("lastClientInput is empty") ;
        return 1 ;
    }

    vtkUnstructuredGrid* ug = this->lastUGfromClient;

    // parse lastClientInput into json
    std::stringstream ss ;
    boost::property_tree::ptree pt ;
    ss << json ;
    try {
        boost::property_tree::read_json(ss, pt);
    } catch (const std::exception& e) {
        this->printMsg("input is not json: " + json) ;
        return 0 ;
    }

    // if json has pointCoords -> update point coords
    if (hasChild(pt, "pointCoords")) {
        size_t pointSize = pt.get_child("pointCoords").size() / 3;
        if (pointSize > 0) {
            vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
            points->SetNumberOfPoints(pointSize);
            auto *pointCoordinates = (float *) points->GetVoidPointer(0);
            vector<double> jsArray(pointSize * 3);
            jsonEntryToVector<double>(pt, "pointCoords", jsArray.data());

            for (std::vector<double>::size_type i = 0; i != pointSize * 3; i++) {
                pointCoordinates[i] = jsArray[i];
            }
            ug->SetPoints(points);
        }

        // if json has point data -> then add each array as doubleArray
        if ( hasChild(pt, "PointData") ) {
            auto pd = ug->GetPointData();
            for (auto& item : pt.get_child("PointData")) {
                vtkSmartPointer<vtkDoubleArray> array_s = vtkSmartPointer<vtkDoubleArray>::New();
                vtkSmartPointer<vtkDataObject> data = vtkSmartPointer<vtkDataObject>::New();
                array_s->SetName(item.first.c_str()) ;

                vector<double> jsArray(pt.get_child("PointData." + item.first ).size());
                jsonEntryToVector<double>(pt, "PointData." + item.first, jsArray.data());

                for (std::vector<double>::size_type i = 0; i != jsArray.size(); i++) {
                    array_s->InsertValue(i, jsArray[i]);
                }
                pd->AddArray(array_s) ;
            }
        }
    }

    // if json has connectivityList -> update connectivityList; if not create vertex cell for each point
    if (hasChild(pt, "connectivityList")) {
        vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();

        size_t cellSize = pt.get_child("connectivityList").size();
        int nCells = 0 ;
        vector<long> jsArray(cellSize);
        jsonEntryToVector<long>(pt, "connectivityList", jsArray.data());

        for (std::vector<long>::size_type i = 0; i != jsArray.size(); ) {
            nCells += 1 ;
            i += (jsArray[i] + 1) ;
        }

        vtkIdType *connectivityList = cells->WritePointer(nCells, cellSize);
        for (size_t i = 0; i < cellSize; i++) {
            connectivityList[i] = jsArray[i];
        }
        int cellTypes[nCells] ;
        for (size_t i = 0, topoIndex = 0; i < nCells; i++) {
            cellTypes[i] = vtkCellsTypeHash[jsArray[topoIndex]];
            topoIndex += jsArray[topoIndex] + 1;
        }
        ug->SetCells(cellTypes, cells);

        // if json has cell data -> then add each array as doubleArray
        if ( hasChild(pt, "CellData") ) {
            auto cd = (vtkFieldData*) ug->GetCellData();
            for (auto& item : pt.get_child("CellData")) {
                vtkSmartPointer<vtkDoubleArray> array_s = vtkSmartPointer<vtkDoubleArray>::New();
                vtkSmartPointer<vtkDataObject> data = vtkSmartPointer<vtkDataObject>::New();
                array_s->SetName(item.first.c_str()) ;

                vector<double> jsArray(pt.get_child("CellData." + item.first ).size());
                jsonEntryToVector<double>(pt, "CellData." + item.first, jsArray.data());

                for (std::vector<double>::size_type i = 0; i != jsArray.size(); i++) {
                    array_s->InsertValue(i, jsArray[i]);
                }
                cd->AddArray(array_s) ;
            }
        }
    }

    // if json has field data -> then add each array as doubleArray
    if ( hasChild(pt, "FieldData") ) {
        auto fd = ug->GetFieldData();
        for (auto& item : pt.get_child("FieldData")) {
            vtkSmartPointer<vtkDoubleArray> array_s = vtkSmartPointer<vtkDoubleArray>::New();
            vtkSmartPointer<vtkDataObject> data = vtkSmartPointer<vtkDataObject>::New();
            array_s->SetName(item.first.c_str()) ;

            vector<double> jsArray(pt.get_child("FieldData." + item.first ).size());
            jsonEntryToVector<double>(pt, "FieldData." + item.first, jsArray.data());

            for (std::vector<double>::size_type i = 0; i != jsArray.size(); i++) {
                array_s->InsertValue(i, jsArray[i]);
            }
            fd->AddArray(array_s) ;
        }
    }
    this->SetNeedsUpdate(true);

    return 1;
}
