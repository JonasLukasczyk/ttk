/// \ingroup base
/// \class ttk::RandomPointsGenerator
/// \author Emma Nilsson <emma.nilsson@liu.se>
/// \date 2021-08-31.
///
/// This module defines the %RandomPointsGenerator class that generates random
/// points in a given domain with scalar attributes.
///
/// \b Related \b publication: \n

#pragma once

// ttk common includes
#include <Debug.h>
#include <Triangulation.h>

namespace ttk {


  class RandomPointsGenerator : virtual public Debug {

  public:
    RandomPointsGenerator();

  }; // RandomPointsGenerator class

} // namespace ttk
