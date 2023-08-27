#include <Debug.h>

#include <H5Cpp.h>

namespace ttk {
  class PyCinemaWriter : virtual public ttk::Debug {
    public:
      PyCinemaWriter(const std::string& path);
      ~PyCinemaWriter();

      void addH5DataSet(H5::H5Location& location, const std::string& name, const hsize_t dim[3], const float* data, const int compression);

      H5::H5File root;
      H5::Group meta;
      H5::Group channels;
  };
}
