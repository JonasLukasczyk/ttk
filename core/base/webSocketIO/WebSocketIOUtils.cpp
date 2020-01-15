
// https://vtk.org/doc/nightly/html/vtkType_8h_source.html
// ============================================= Constants ==========================================================
const int DUPLICATE = 1 ;
const int BIG_ENDIANNESS = 2 ;
const int LITTLE_ENDIANNESS = 3 ;
const int OBJECT_WILL_SENDING = 5 ;
const int OBJECT_ACK_OBJECT = 6 ;
const int OBJECT_WILL_FINISH = 7 ;
const int OBJECT_ACK_FINISH = 8 ;

const int DATA_FLOAT_ARRAY = 10 ;
const int DATA_LONG_ARRAY = 8 ;
const int DATA_UNSIGNED_ARRAY = 3 ;
const int DATA_INT_ARRAY = 6 ;
const int DATA_DOUBLE_ARRAY = 11 ;
const int DATA_STRING_ARRAY = 13 ;

// ============================================ Utils ==============================================================

// borrow from https://stackoverflow.com/questions/4239993/determining-endianness-at-compile-time/4240029
bool isLittleEndian()
{
    short int number = 0x1;
    char *numPtr = (char*)&number;
    return (numPtr[0] == 1);
}