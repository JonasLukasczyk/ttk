/// \author Your Name Here <Your Email Address Here>.
/// \date The Date Here.
///
/// \brief dummy program example.

// include the local headers
#include <vtkSmartPointer.h>
#include <ttkArrayEditor.h>
#include <ttkWebSocketIO.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLImageDataReader.h>

using namespace std;
using namespace ttk;

void startServer() {
    // get the test data
    auto unStructuredGrid = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
    unStructuredGrid->SetFileName("/home/local/ASUAD/wshen24/Documents/ttk-data-wk/road_data.vtu");

    // send the two data to e2rr module
    auto webSocket = vtkSmartPointer<ttkWebSocketIO>::New();
    webSocket->SetInputConnection(0, unStructuredGrid->GetOutputPort(0));

    webSocket->SetPortNumber(8779);
    webSocket->Update();

    // in the ParaView, the main thread will always be there
    size_t i = 0 ;
    while (i <= 100000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        i += 1 ;
    }
}

void startServerX() {
    // get the test data
    auto unImageData = vtkSmartPointer<vtkXMLImageDataReader>::New();
    unImageData->SetFileName("/home/local/ASUAD/wshen24/Documents/ttk-data-wk/FingersPPI.vti");

    // send the two data to e2rr module
    auto webSocket = vtkSmartPointer<ttkWebSocketIO>::New();
    webSocket->SetInputConnection(0, unImageData->GetOutputPort(0));

    webSocket->SetPortNumber(9285);
    webSocket->Update();

    // in the ParaView, the main thread will always be there
    size_t i = 0 ;
    while (i <= 100000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        i += 1 ;
    }
}

void startServerImage() {
    // get the test data
    auto unImageData = vtkSmartPointer<vtkXMLImageDataReader>::New();
    unImageData->SetFileName("/home/local/ASUAD/wshen24/Documents/ttk-data-wk/histogram.vti");

    // send the two data to e2rr module
    auto webSocket = vtkSmartPointer<ttkWebSocketIO>::New();
    webSocket->SetInputConnection(0, unImageData->GetOutputPort(0));

    webSocket->SetPortNumber(8778);
    webSocket->Update();

    // in the ParaView, the main thread will always be there
    size_t i = 0 ;
    while (i <= 100000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        i += 1 ;
    }
}

int main(int argc, char **argv) {

    ttk::globalDebugLevel_ = 3;

    // test on multiple ports
    std::thread t1(startServerImage), t2(startServer), t3(startServerX);
    t1.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
    t2.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
    t3.join() ;

    return 1;
}
