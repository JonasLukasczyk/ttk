#include <PyCinemaWriter.h>

ttk::PyCinemaWriter::PyCinemaWriter(const std::string& path){
  this->root = H5::H5File( path.data(), H5F_ACC_TRUNC );
  this->meta = this->root.createGroup("meta");
  this->channels = this->root.createGroup("channels");
}
ttk::PyCinemaWriter::~PyCinemaWriter(){
  this->root.close();
}

void ttk::PyCinemaWriter::addH5DataSet(H5::H5Location& location, const std::string& name, const hsize_t dim[3], const float* data, const int compression){
  hsize_t cdim[3]{
    32<dim[0] ? 32 : dim[0],
    32<dim[1] ? 32 : dim[1],
    dim[2]
  };
  hsize_t DIM = dim[1]==1 && dim[2]==1
    ? 1
    : dim[2]==1
      ? 2
      : 3;

  H5::DSetCreatPropList ds_creatplist;  // create dataset creation prop list
  ds_creatplist.setChunk( DIM, cdim );  // then modify it for compression
  ds_creatplist.setDeflate( compression );

  H5::DataSet dataset = location.createDataSet(
    name,
    H5::FloatType(H5::PredType::NATIVE_FLOAT),
    H5::DataSpace(DIM,dim),
    ds_creatplist
  );
  dataset.write( data, H5::PredType::NATIVE_FLOAT );
}
