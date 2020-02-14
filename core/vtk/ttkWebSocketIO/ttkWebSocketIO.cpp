#include <ttkWebSocketIO.h>

#include <vtkImageData.h>
#include <vtkDataObject.h> // For port info
#include <vtkObjectFactory.h> // for new macro

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkDoubleArray.h>

#include <vtkPointData.h>
#include <vtkCellData.h>

#include <vtkPoints.h>
#include <vtkCellArray.h>

#include <boost/optional/optional.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

// TODO
using namespace std;

vtkStandardNewMacro(ttkWebSocketIO);

// ======================================== Constants ==================================================
const int vtkCellsTypeHash[20] = {
        VTK_EMPTY_CELL ,
        VTK_VERTEX,
        VTK_LINE,
        VTK_TRIANGLE,
        VTK_TETRA,
        VTK_CONVEX_POINT_SET, // 5
        VTK_CONVEX_POINT_SET, // 6
        VTK_CONVEX_POINT_SET, // 7
        VTK_VOXEL,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET,
        VTK_CONVEX_POINT_SET
};

// ===============================================  JSON utils =======================================
template <typename T> void jsonEntryToVector(
        const boost::property_tree::ptree& pt,
        const boost::property_tree::ptree::key_type& key,
        T* result
) {
    size_t i=0;
    for (auto& item : pt.get_child(key))
        result[i++] = item.second.get_value<T>();
}

// check if a path exists in property_tree or a key exists in json object
bool hasChild(const boost::property_tree::ptree& pt,
              const boost::property_tree::ptree::key_type& key) {
    boost::property_tree::ptree::const_assoc_iterator it = pt.find(key);
    if( it == pt.not_found() )
    {
        return false ;
    }
    return true ;
}

ttkWebSocketIO::ttkWebSocketIO() {
    this->lastOutput = vtkSmartPointer<vtkUnstructuredGrid>::New();

    this->SetNeedsUpdate(true);

    this->SetNumberOfInputPorts(1);
    this->SetNumberOfOutputPorts(1);
}

ttkWebSocketIO::~ttkWebSocketIO() {
    this->printMsg("invoke ~ttkWebSocketIO!") ;
}

int ttkWebSocketIO::FillInputPortInformation(int port, vtkInformation *info) {
    switch (port) {
        case 0:
            info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
            break;
        default:
            return 0;
    }
    return 1;
}

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
    this->printMsg("invoke RequestData! & Port: " + to_string(this->GetPortNumber()));
    this->SetNeedsUpdate(false);

    auto input = vtkDataSet::GetData( inputVector[0] );
    this->lastInput = vtkSmartPointer<vtkDataSet>::Take( input->NewInstance() );
    this->lastInput->ShallowCopy( input );

    if (this->isListening() && this->getPortNumber() != this->PortNumber) {
        this->stopServer() ;
    }

    try {
        // port duplication or other kind of error, return error immediately
        if (!this->isListening()){
            this->startServer( this->PortNumber );
        } else {
            if ( this->lastReqUpdate ) {
                this->processClientRequest("raw","requestData");
            }
        }
    } catch (const std::exception& e) {
        this->printMsg("start Server error: " + string(e.what())) ;
        return 0 ;
    }

    // Get the output
    auto output = vtkUnstructuredGrid::GetData( outputVector );
    output->ShallowCopy( this->lastOutput );
    this->lastReqUpdate = true ;

    return 1;
}

int addFieldDataArraysToHeader(vtkFieldData* fd, std::string typeName, std::vector<std::map<string, string>>& headers, std::vector<void *>& sendingData){
    int nFieldDataArrays = fd->GetNumberOfArrays();

    for (int i = 0; i < nFieldDataArrays; i++) {
        vtkAbstractArray *array = fd->GetAbstractArray(i);
        std::string string1 = string(array->GetName());
        string1.erase(std::remove(string1.begin(), string1.end(), ':'), string1.end());
        switch (array->GetDataType()) {
            vtkTemplateMacro({
                    size_t n = array->GetNumberOfValues();
                    auto values = (VTK_TT *) array->GetVoidPointer(0);
                    headers.push_back(
                    ttkWebSocketIO::combine_object_header_object(typeName + ":" + string1, array->GetNumberOfTuples(), array->GetNumberOfComponents(), array->GetDataType()));
                    sendingData.push_back(values);
                             });

            case VTK_STRING:
                auto values = (string *) array->GetVoidPointer(0);
                headers.push_back(
                        ttkWebSocketIO::combine_object_header_object(typeName + ":" + string1, array->GetNumberOfTuples(), array->GetNumberOfComponents(), VTK_STRING));
                sendingData.push_back(values);
        }
    }

    return 1;
}

/**
 * @param name:
 *       reserved options:
 *         1. on_open, callback from base library with parameter "on_open"
 *         2. raw, send back a raw payload from the browser
 *
 *       custom options (user-defined):
 *         ...
 * @param payload
 *
 */
int ttkWebSocketIO::processClientRequest(std::string name, std::string payload){
    this->printMsg("The name in processClientRequest is : " + name) ;
    if ( name == "raw" ) {
        if ( payload.rfind("updateUnstructuredGrid:", 0) == 0 ) {
            this->CreateUnstructuredGrid(  payload.substr(23) ) ;
            this->lastReqUpdate = false ;
            this->printMsg("payload in processClientRequest is: " + payload) ;
            return 1;
        } else if ( payload.rfind("updateImageData:", 0) == 0 ) {
            this->printMsg("payload for update ImageData:" + payload.substr(16) ) ;
            this->lastReqUpdate = false ;
            return 1;
        }
    }

    int structureType = this->lastInput->IsA("vtkImageData") ? 2 : this->lastInput->IsA("vtkUnstructuredGrid") ? 1 : 0;
    if(structureType<1){
        this->printErr("Unsupported Data Object Type.");
        return 0;
    }

    if(name.compare("on_open") == 0 || (name == "raw" && payload == "requestData")) {
        std::vector<std::map<string, string>> headers;
        std::vector<void *> sendingData;

        // send structureType back to the Client
        signed int *tmp;
        tmp = new int[1];
        tmp[0] = structureType;
        headers.push_back(ttkWebSocketIO::combine_object_header_object("VtkDataObjectType", 1, 1, VTK_INT));
        sendingData.push_back(tmp);

        if (structureType == 1) {  // unStructuredGrid
            auto* lastInputAsUG = vtkUnstructuredGrid::SafeDownCast( this->lastInput );

            int nPoints = lastInputAsUG->GetNumberOfPoints();
            auto *pointCoords = (float *) lastInputAsUG->GetPoints()->GetVoidPointer(0);
            headers.push_back(ttkWebSocketIO::combine_object_header_object("PointCoords", nPoints, 3, VTK_FLOAT));
            sendingData.push_back(pointCoords);

            int nCells = lastInputAsUG->GetNumberOfCells();
            auto *connectivityList = (long long *) lastInputAsUG->GetCells()->GetPointer();
            size_t j = 0, topoIndex = 0;
            for (j = 0, topoIndex = 0; j < nCells; j++) {
                size_t nVertices = connectivityList[topoIndex];
                topoIndex += nVertices + 1;
            }
            headers.push_back(ttkWebSocketIO::combine_object_header_object("ConnectivityList", topoIndex, 1, VTK_LONG));
            sendingData.push_back(connectivityList);
        }

        if (structureType == 2) { // imageData
            signed int *tmp;
            tmp = new int[6];
            auto* lastInputAsID = vtkImageData::SafeDownCast( this->lastInput );
            lastInputAsID->GetExtent(tmp);
            headers.push_back(ttkWebSocketIO::combine_object_header_object("Extent", 6, 1, VTK_INT));
            sendingData.push_back(tmp);
        }

        addFieldDataArraysToHeader( this->lastInput->GetPointData(), "PointData", headers, sendingData );
        addFieldDataArraysToHeader( this->lastInput->GetCellData(), "CellData", headers, sendingData );
        addFieldDataArraysToHeader( this->lastInput->GetFieldData(), "FieldData", headers, sendingData );

        this->setHeaders(headers);
        this->setObjectState(0);
        this->setSendingData(sendingData);
        this->sendObject();
    }

    return 1;
}

int ttkWebSocketIO::CreateUnstructuredGrid( std::string json ) {
    this->printMsg("invoke CreateUnstructuredGrid, receive JSON: " + json, ttk::debug::Priority::VERBOSE);

    if ( json.empty() ) {
        this->printMsg("lastClientInput is empty") ;
        return 1 ;
    }

    this->lastOutput = vtkSmartPointer<vtkUnstructuredGrid>::New();

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
    if (hasChild(pt, "PointCoords")) {
        size_t pointSize = pt.get_child("PointCoords").size() / 3;
        if (pointSize > 0) {
            vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
            points->SetNumberOfPoints(pointSize);
            auto *pointCoordinates = (float *) points->GetVoidPointer(0);
            vector<double> jsArray(pointSize * 3);
            jsonEntryToVector<double>(pt, "PointCoords", jsArray.data());

            for (std::vector<double>::size_type i = 0; i != pointSize * 3; i++) {
                pointCoordinates[i] = jsArray[i];
            }
            this->lastOutput->SetPoints(points);
        }
    }

    // if json has connectivityList -> update connectivityList; if not create vertex cell for each point
    if (hasChild(pt, "ConnectivityList")) {
        vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();

        size_t cellSize = pt.get_child("ConnectivityList").size();
        int nCells = 0 ;
        vector<long> jsArray(cellSize);
        jsonEntryToVector<long>(pt, "ConnectivityList", jsArray.data());

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
        this->lastOutput->SetCells(cellTypes, cells);
    }

    // if json has point data -> then add each array as doubleArray
    if ( hasChild(pt, "PointData") ) {
        auto pd = this->lastOutput->GetPointData();
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

    // if json has cell data -> then add each array as doubleArray
    if ( hasChild(pt, "CellData") ) {
        auto cd = this->lastOutput->GetCellData();
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

    // if json has field data -> then add each array as doubleArray
    if ( hasChild(pt, "FieldData") ) {
        auto fd = this->lastOutput->GetFieldData();
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
    this->Modified() ;
    this->SetNeedsUpdate(true);

    return 1;
}
