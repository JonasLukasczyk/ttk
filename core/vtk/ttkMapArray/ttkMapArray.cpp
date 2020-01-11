#include <ttkMapArray.h>

#include <vtkDataSet.h>
#include <vtkAbstractArray.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkFieldData.h>
#include <vtkSmartPointer.h>
#include <vtkFieldData.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkDelimitedTextReader.h>

#include <vtkFloatArray.h>

#include <unordered_map>

vtkStandardNewMacro(ttkMapArray);

ttkMapArray::ttkMapArray() {
  this->setDebugMsgPrefix("MapArray");

  this->SetNumberOfInputPorts(2);
  this->SetNumberOfOutputPorts(1);
}

ttkMapArray::~ttkMapArray() {
}

int ttkMapArray::FillInputPortInformation(int port, vtkInformation *info) {
  if(port == 0 || port == 1)
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  else
    return 0;

  return 1;
}

int ttkMapArray::FillOutputPortInformation(int port, vtkInformation *info) {
  if(port == 0)
    info->Set(ttkAlgorithm::SAME_DATA_TYPE_AS_INPUT_PORT(), 1);
  else
    return 0;

  return 1;
}

#include <tuple>
// function has to live in the std namespace
// so that it is picked up by argument-dependent name lookup (ADL).
namespace std{
    namespace
    {

        // Code from boost
        // Reciprocal of the golden ratio helps spread entropy
        //     and handles duplicates.
        // See Mike Seymour in magic-numbers-in-boosthash-combine:
        //     https://stackoverflow.com/questions/4948780

        template <class T>
        inline void hash_combine(std::size_t& seed, T const& v)
        {
            seed ^= hash<T>()(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        }

        // Recursive template code derived from Matthieu M.
        template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
        struct HashValueImpl
        {
          static void apply(size_t& seed, Tuple const& tuple)
          {
            HashValueImpl<Tuple, Index-1>::apply(seed, tuple);
            hash_combine(seed, get<Index>(tuple));
          }
        };

        template <class Tuple>
        struct HashValueImpl<Tuple,0>
        {
          static void apply(size_t& seed, Tuple const& tuple)
          {
            hash_combine(seed, get<0>(tuple));
          }
        };
    }

    template <typename ... TT>
    struct hash<std::tuple<TT...>>
    {
        size_t
        operator()(std::tuple<TT...> const& tt) const
        {
            size_t seed = 0;
            HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
            return seed;
        }

    };
}

template <class valueType, class keyType0, class keyType1>
int buildMap(
    std::unordered_map<std::tuple<int,int>, valueType>& map,
    keyType0* key0ArrayData,
    keyType1* key1ArrayData,
    vtkAbstractArray* valueArray
){
    auto valueArrayData = (valueType*) valueArray->GetVoidPointer(0);

    for(size_t i=0, n=valueArray->GetNumberOfTuples(); i<n; i++){
        map.insert({
            {(int)key0ArrayData[i],(int)key1ArrayData[i]},
            valueArrayData[i]
        });
    }

    return 1;
}

template <class valueType, class lookup1Type>
int processMap(
    std::unordered_map<std::tuple<int,int>, valueType>& map,
    int lookup0Const,
    lookup1Type* lookup1ArrayData,
    vtkAbstractArray* mappedArray
){
    auto mappedArrayData = (valueType*) mappedArray->GetVoidPointer(0);
    for(size_t i=0, n=mappedArray->GetNumberOfTuples(); i<n; i++){
        auto key = std::make_tuple(lookup0Const,(int)lookup1ArrayData[i]);
        auto it = map.find(key);
        if(it == map.end())
            mappedArrayData[i] = -1;
        else
            mappedArrayData[i] = it->second;
    }

    return 1;
}

template <class valueType>
int dispatch(
    vtkAbstractArray* valueArray,

    vtkAbstractArray* key0Array,
    vtkAbstractArray* key1Array,

    std::vector<vtkAbstractArray*>& lookup0Arrays,
    std::vector<vtkAbstractArray*>& lookup1Arrays,

    vtkDataObject* output,
    vtkDataObject* source
){
    auto outputAsMB = vtkMultiBlockDataSet::SafeDownCast( output );
    auto sourceAsDS = vtkDataSet::SafeDownCast( source );

    typedef std::unordered_map<std::tuple<int,int>,valueType> mapType;
    mapType map;

    switch( vtkTemplate2PackMacro(
        key0Array->GetDataType(),
        key1Array->GetDataType()
    ) ){
        vtkTemplate2Macro(
            buildMap(
                map,
                (VTK_T1*) key0Array->GetVoidPointer(0),
                (VTK_T2*) key1Array->GetVoidPointer(0),
                valueArray
            );
        );
    }

    for(size_t b=0, n=outputAsMB->GetNumberOfBlocks(); b<n; b++){
        auto blockAsDS = vtkDataSet::SafeDownCast( outputAsMB->GetBlock(b) );
        auto mappedArray = vtkSmartPointer<vtkAbstractArray>::Take( valueArray->NewInstance() );
        mappedArray->SetName( valueArray->GetName() );
        mappedArray->SetNumberOfComponents( 1 );
        mappedArray->SetNumberOfTuples( blockAsDS->GetNumberOfPoints() );

        switch( lookup1Arrays[b]->GetDataType() ){
            vtkTemplateMacro(
                processMap(
                    map,
                    b,
                    (VTK_TT*) lookup1Arrays[b]->GetVoidPointer(0),
                    mappedArray
                )
            );
        }

        blockAsDS->GetPointData()->AddArray( mappedArray );
    }

    return 1;
};

// =============================================================================
// RequestData
// =============================================================================
int ttkMapArray::RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector
){
    ttk::Timer t;

    this->printMsg(ttk::debug::Separator::L1);

    // -------------------------------------------------------------------------
    // Get Input and Output
    // -------------------------------------------------------------------------
    auto source = vtkDataObject::GetData(inputVector[0]);
    auto target = vtkDataObject::GetData(inputVector[1]);

    auto output = vtkDataObject::GetData(outputVector);
    output->ShallowCopy( target );

    size_t nKeys = Key2Mode>0 ? 3 : Key1Mode>0 ? 2 : Key0Mode>0 ? 1 : 0;

    auto valueArray = this->GetInputArrayToProcess(0, inputVector);
    auto key0Array = this->GetInputArrayToProcess(1, inputVector);
    auto key1Array = this->GetInputArrayToProcess(2, inputVector);

    // auto lookup0Arrays
    std::vector<vtkAbstractArray*> lookup0Arrays;
    std::vector<vtkAbstractArray*> lookup1Arrays;
    auto targetAsMB = vtkMultiBlockDataSet::SafeDownCast( target );
    for(size_t b=0, n=targetAsMB->GetNumberOfBlocks(); b<n; b++){
        auto blockAsDS = (vtkDataSet*) targetAsMB->GetBlock(b);
        lookup0Arrays.push_back( this->GetInputArrayToProcess(3, blockAsDS) );
        lookup1Arrays.push_back( this->GetInputArrayToProcess(4, blockAsDS) );
    }

    switch(valueArray->GetDataType()){
        vtkTemplateMacro(
            dispatch<VTK_TT>(
                valueArray,
                key0Array,
                key1Array,

                lookup0Arrays,
                lookup1Arrays,

                output,
                source
            )
        );
    }

    // if(nKeys==1){
    //     // std::unordered_map<std::tuple<int>,>
    // } else {
    //     this->printErr("Key number currently not supported");
    // }

    // // Check Parameter Consistency
    // {
    //     // Keys
    //     {
    //         bool error = false;
    //         if(this->Key0Mode==KeyMode::Field && this->Key0Array.first!=this->ValueArray.first) error = true;
    //         if(this->Key1Mode==KeyMode::Field && this->Key1Array.first!=this->ValueArray.first) error = true;
    //         if(this->Key2Mode==KeyMode::Field && this->Key2Array.first!=this->ValueArray.first) error = true;
    //         if(error){
    //             this->printErr("If field arrays are used as keys then they must be of the same attribute type as the value array.");
    //             return 0;
    //         }
    //     }

    //     // Lookup
    //     {
    //         bool error = false;
    //         if(this->Lookup0Mode==KeyMode::Field && this->Lookup0Array.first!=this->TargetDomain) error = true;
    //         if(this->Lookup1Mode==KeyMode::Field && this->Lookup1Array.first!=this->TargetDomain) error = true;
    //         if(this->Lookup2Mode==KeyMode::Field && this->Lookup2Array.first!=this->TargetDomain) error = true;
    //         if(error){
    //             this->printErr("If field arrays are used for lookup then they must be of the same attribute type as the target domain.");
    //             return 0;
    //         }
    //     }
    // }

    // vtkFieldData* sourceData[3] = {
    //     source->GetPointData(),
    //     source->GetCellData(),
    //     source->GetFieldData()
    // };

    // if(this->Implementation==0){
        // use unordered_map
        // switch(this->Key0Mode)

        // if(this->Key0Mode==KeyMode::Disabled){

        // }

            // auto valueArray = sourceData[ this->ValueArray.first ]->GetAbstractArray( this->ValueArray.second.data() );
            // if(!valueArray){
            //     dMsg(cout, "[ttkMapArray] ERROR: Source does not have value array '"+this->ValueArray.second+"'.\n", fatalMsg);
            //     return 0;
            // }

            // switch( valueArray->GetDataType() ){
            //     ttkTemplateMacro({
            //         std::unordered_map<SimplexId TTK_COMMA VTK_TT> map;
            //         auto valueArrayData = (VTK_TT*) valueArray->GetVoidPointer(0);
            //         for(SimplexId i=0, j=valueArray->GetNumberOfTuples(); i<j; i++)
            //             map[i] = valueArrayData[i];

            //         auto outputArray = vtkSmartPointer<vtkAbstractArray>::Take( valueArray->NewInstance() );
            //         outputArray->SetName( valueArray->GetName() );

            //         cout<<"done"<<endl;
            //         cout<<"perform lookup"<<endl;

            //         performLookup(this, output, map, outputArray, (VTK_TT) this->DefaultValue);

            //     });
            // }


    // } else {
    //     // use array

    // }


    // vtkFieldData* sourceData[3] = {
    //     source->GetPointData(),
    //     source->GetCellData(),
    //     source->GetFieldData()
    // };

    // int sourceFieldType = this->GetSourceFieldType();
    // string codomainFieldName = sourceFieldType==0
    //     ? this->GetCodomainFieldName0().data()
    //     : sourceFieldType==1
    //         ? this->GetCodomainFieldName1().data()
    //         : this->GetCodomainFieldName2().data();

    // auto sourceCodomainField = sourceData[ sourceFieldType ]->GetAbstractArray( codomainFieldName.data() );
    // if(!sourceCodomainField){
    //     dMsg(cout, "[ttkMapArray] ERROR: Source does not have field '"+ codomainFieldName +"'.\n", fatalMsg);
    //     return 0;
    // }

    // // -------------------------------------------------------------------------
    // // Get Domain and Lookup Field Names
    // // -------------------------------------------------------------------------
    // vector<string> domainFieldNames;
    // vector<string> lookupFieldNames;

    // auto split = [](const string& s, vector<string>& result){
    //     auto trim = [](string& s, const std::string& delimiters = " \f\n\r\t\v"){
    //         s.erase( s.find_last_not_of(delimiters) + 1 );
    //         s.erase( 0, s.find_first_not_of(delimiters) );
    //         return s;
    //     };

    //     stringstream ss( s );
    //     string element;
    //     while( getline(ss, element, ',') )
    //         result.push_back( trim(element) );
    // };

    // split(this->GetDomainFieldNames(), domainFieldNames);
    // split(this->GetLookupFieldNames(), lookupFieldNames);

    // if(lookupFieldNames.size() != domainFieldNames.size()){
    //     dMsg(cout, "[ttkMapArray] ERROR: Number of domain and lookup fields are not the same.\n", fatalMsg);
    //     return 0;
    // }

    // // -------------------------------------------------------------------------
    // // Generate map and lookup values
    // // -------------------------------------------------------------------------
    // bool useLookupTable = true;

    // string fieldTypes[3] = {"point data", "cell data", "field data"};
    // switch( sourceCodomainField->GetDataType() ){
    //     ttkTemplateMacro({

    //         unordered_map<string TTK_COMMA VTK_TT> map;
    //         vector<VTK_TT> lookupTable;
    //         VTK_TT defaultValue = (VTK_TT) this->GetDefaultValue();
    //         int offset = 0;

    //         // -----------------------------------------------------------------
    //         // Generate LookupTable
    //         // -----------------------------------------------------------------
    //         if(useLookupTable && domainFieldNames.size()==1){
    //             Timer t;
    //             dMsg(cout, "[ttkMapArray] Generating table  ... ", timeMsg);

    //             auto nCodomainTuples = sourceCodomainField->GetNumberOfTuples();

    //             const string& domainFieldName = domainFieldNames[0];
    //             auto domainField = vtkFloatArray::SafeDownCast(
    //                 sourceData[ sourceFieldType ]->GetAbstractArray( domainFieldName.data() )
    //             );
    //             if(!domainField){
    //                 dMsg(cout, "failed\n[ttkMapArray] ERROR: Source does not have " + fieldTypes[ sourceFieldType ] + " '"+ domainFieldName +"'.\n", fatalMsg);
    //                 return 0;
    //             }
    //             if(domainField->GetNumberOfTuples()!=nCodomainTuples){
    //                 dMsg(cout, "failed\n[ttkMapArray] ERROR: Codomain does not have the same number of tuples than " + fieldTypes[ sourceFieldType ] + " '"+ domainFieldName +"'.\n", fatalMsg);
    //                 return 0;
    //             }
    //             if(domainField->GetNumberOfComponents()!=1){
    //                 dMsg(cout, "failed\n[ttkMapArray] ERROR: Domain " + fieldTypes[ sourceFieldType ] + " array '"+ domainFieldName +"' must have exactly one component.\n", fatalMsg);
    //                 return 0;
    //             }

    //             lookupTable.clear();

    //             if(domainField->GetNumberOfTuples()>0){
    //                 auto domainFieldData = (float*) domainField->GetVoidPointer(0);

    //                 int min = 9999999;
    //                 int max = -9999999;
    //                 for(size_t i=0, j=domainField->GetNumberOfTuples(); i<j; i++){
    //                     const auto& v = domainFieldData[i];
    //                     if(min>v) min=v;
    //                     if(max<v) max=v;
    //                 }

    //                 offset = -min;
    //                 max += offset;

    //                 cout<<min<<" "<<max<<endl;

    //                 lookupTable.resize( max+1, defaultValue );

    //                 auto sourceCodomainData = (VTK_TT*) sourceCodomainField->GetVoidPointer(0);
    //                 for(size_t i=0, j=domainField->GetNumberOfTuples(); i<j; i++){
    //                     lookupTable[ domainFieldData[i]+offset ] = sourceCodomainData[i];
    //                 }
    //             }

    //             dMsg(cout, "done ("+to_string(t.getElapsedTime())+" s).\n", timeMsg);
    //         }

    //         // -----------------------------------------------------------------
    //         // Generate Map
    //         // -----------------------------------------------------------------
    //         if(!useLookupTable && domainFieldNames.size()>0){
    //             Timer t;
    //             dMsg(cout, "[ttkMapArray] Generating map    ... ", timeMsg);

    //             auto nCodomainTuples = sourceCodomainField->GetNumberOfTuples();

    //             vector<vtkAbstractArray*> domainFields;
    //             for(auto domainFieldName: domainFieldNames){
    //                 auto domainField = sourceData[ sourceFieldType ]->GetAbstractArray( domainFieldName.data() );
    //                 if(!domainField){
    //                     dMsg(cout, "failed\n[ttkMapArray] ERROR: Source does not have " + fieldTypes[ sourceFieldType ] + " '"+ domainFieldName +"'.\n", fatalMsg);
    //                     return 0;
    //                 }
    //                 if(domainField->GetNumberOfTuples()!=nCodomainTuples){
    //                     dMsg(cout, "failed\n[ttkMapArray] ERROR: Codomain does not have the same number of tuples than " + fieldTypes[ sourceFieldType ] + " '"+ domainFieldName +"'.\n", fatalMsg);
    //                     return 0;
    //                 }
    //                 domainFields.push_back( domainField );
    //             }

    //             auto sourceCodomainData = (VTK_TT*) sourceCodomainField->GetVoidPointer(0);
    //             for(vtkIdType i=0; i<nCodomainTuples; i++){
    //                 string key = "";
    //                 for(auto& field: domainFields)
    //                     key += field->GetVariantValue(i).ToString() + "_";

    //                 map[key] = sourceCodomainData[i];
    //             }

    //             dMsg(cout, "done ("+to_string(t.getElapsedTime())+" s).\n", timeMsg);
    //         }
    //         // else {
    //         //     dMsg(cout, "[ttkMapArray] WARNING: No domain fields specified -> Mapping to first value of codomain.\n", timeMsg);
    //         // }

    //         // -----------------------------------------------------------------
    //         // Lookup
    //         // -----------------------------------------------------------------
    //         {
    //             Timer t;
    //             dMsg(cout, "[ttkMapArray] Looking up values ... ", timeMsg);

    //             int status = 0;
    //             if(useLookupTable){
    //                 status = processObjectLT<VTK_TT>(
    //                     output,
    //                     sourceCodomainField,
    //                     lookupTable,
    //                     lookupFieldNames[0],
    //                     this->GetTargetFieldType(),
    //                     offset,
    //                     defaultValue
    //                 );
    //             } else {
    //                 status = processObject<VTK_TT>(
    //                     output,
    //                     sourceCodomainField,
    //                     map,
    //                     lookupFieldNames,
    //                     this->GetTargetFieldType(),
    //                     -1,
    //                     defaultValue
    //                 );
    //             }

    //             if(status==0){
    //                 dMsg(cout, "failed\n[ttkMapArray] ERROR: Target is not a 'vtkDataSet' nor a 'vtkMultiBlockDataSet' that contains 'vtkDataSets'.\n", fatalMsg);
    //                 return 0;
    //             } else if(status==2){
    //                 dMsg(cout, "failed\n[ttkMapArray] ERROR: Target does not have one of the lookup fields.\n", fatalMsg);
    //                 return 0;
    //             } else if(status==3){
    //                 dMsg(cout, "failed\n[ttkMapArray] ERROR: Lookup fields do not have the same dimension as target field type.\n", fatalMsg);
    //                 return 0;
    //             }

    //             dMsg(cout, "done ("+to_string(t.getElapsedTime())+" s).\n", timeMsg);
    //         }
    //     });
    // }

    // Output Performance
    // {
    //     stringstream msg;
    //     msg << "[ttkMapArray] --------------------------------------------------------------"<<endl;
    //     msg << "[ttkMapArray] time: " << t.getElapsedTime() << " s." << endl;
    //     dMsg(cout, msg.str(), timeMsg);
    // }

    return 1;
}