
#include <boost/optional/optional.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace std ;

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

// =============================================== Sending Object utils =====================================
std::map<string, string> combineObjectHeader(const string &key, int nTuples, int nComponents, int dataType) {
    map<string, string> ans;

    ans.insert(std::make_pair("key", key));  // "pointCoords" or "FieldData:Result", split by ":"
    ans.insert(std::make_pair("nTuples", to_string(nTuples)));
    ans.insert(std::make_pair("nComponents", to_string(nComponents)));
    ans.insert(std::make_pair("dataType", to_string(dataType)));

    return ans;
}