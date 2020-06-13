/// \ingroup base
/// \class ttk::MorseSmaleComplex3D::
/// \author Guillaume Favelier <guillaume.favelier@lip6.fr>
/// \author Julien Tierny <julien.tierny@lip6.fr>
/// \date February 2017.
///
/// \brief TTK %morseSmaleComplex3D processing package.
///
/// %MorseSmaleComplex3D is a TTK processing package that takes a scalar field
/// on the input and produces a scalar field on the output.
///
/// \sa ttk::Triangulation
/// \sa ttkMorseSmaleComplex3D.cpp %for a usage example.

#ifndef _MORSESMALECOMPLEX3D_H
#define _MORSESMALECOMPLEX3D_H

// base code includes
#include <AbstractMorseSmaleComplex.h>

namespace ttk {

  /**
   * Class specialized in building the Morse-Smale complex
   * of 3D triangulation.
   */
  class MorseSmaleComplex3D : public AbstractMorseSmaleComplex {

  public:
    MorseSmaleComplex3D();
    ~MorseSmaleComplex3D();

    /**
     * Main function for computing the whole Morse-Smale complex.
     */
    template <typename dataType, typename idtype>
    int execute();

    /**
     * Compute the (saddle1, saddle2) pairs not detected by the
     * contour tree.
     */
    template <typename dataType, typename idType>
    int computePersistencePairs(
      std::vector<std::tuple<SimplexId, SimplexId, dataType>>
        &pl_saddleSaddlePairs);

    template <typename dataType>
    int setAugmentedCriticalPoints(const std::vector<dcg::Cell> &criticalPoints,
                                   SimplexId *ascendingManifold,
                                   SimplexId *descendingManifold) const;

    /**
     * Compute the descending 1-separatrices by reading into the discrete
     * gradient.
     */
    int getAscendingSeparatrices1(
      const std::vector<dcg::Cell> &criticalPoints,
      std::vector<Separatrix> &separatrices,
      std::vector<std::vector<dcg::Cell>> &separatricesGeometry) const;

    /**
     * Compute the saddle-connectors by reading into the discrete
     * gradient.
     */
    int getSaddleConnectors(
      const std::vector<dcg::Cell> &criticalPoints,
      std::vector<Separatrix> &separatrices,
      std::vector<std::vector<dcg::Cell>> &separatricesGeometry) const;

    /**
     * Compute the 2-separatrices by reading into the discrete
     * gradient from the maxima.
     */
    int getDescendingSeparatrices2(
      const std::vector<dcg::Cell> &criticalPoints,
      std::vector<Separatrix> &separatrices,
      std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
      std::vector<std::set<SimplexId>> &separatricesSaddles) const;

    /**
     * Compute the geometrical embedding of the descending
     * 2-separatrices. This function needs the following
     * internal pointers to be set:
     * outputSeparatrices2_numberOfPoints_
     * outputSeparatrices2_points_
     * outputSeparatrices2_numberOfCells_
     * outputSeparatrices2_cells_
     * inputScalarField_
     */
    template <typename dataType>
    int setDescendingSeparatrices2(
      const std::vector<Separatrix> &separatrices,
      const std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
      const std::vector<std::set<SimplexId>> &separatricesSaddles) const;
#ifdef TTK_ENABLE_OPENMP
    template <typename dataType>
    int omp_setDescendingSeparatrices2(
      const std::vector<Separatrix> &separatrices,
      const std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
      const std::vector<std::set<SimplexId>> &separatricesSaddles) const;
#endif

    int getDualPolygon(const SimplexId edgeId,
                       std::vector<SimplexId> &polygon) const;

    int sortDualPolygonVertices(std::vector<SimplexId> &polygon) const;

    /**
     * Compute the 2-separatrices by reading into the discrete
     * gradient from the minima.
     */
    int getAscendingSeparatrices2(
      const std::vector<dcg::Cell> &criticalPoints,
      std::vector<Separatrix> &separatrices,
      std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
      std::vector<std::set<SimplexId>> &separatricesSaddles) const;

    /**
     * Compute the geometrical embedding of the ascending
     * 2-separatrices. This function needs the following
     * internal pointers to be set:
     * outputSeparatrices2_numberOfPoints_
     * outputSeparatrices2_points_
     * outputSeparatrices2_numberOfCells_
     * outputSeparatrices2_cells_
     * inputScalarField_
     */
    template <typename dataType>
    int setAscendingSeparatrices2(
      const std::vector<Separatrix> &separatrices,
      const std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
      const std::vector<std::set<SimplexId>> &separatricesSaddles) const;

#ifdef TTK_ENABLE_OPENMP
    template <typename dataType>
    int omp_setAscendingSeparatrices2(
      const std::vector<Separatrix> &separatrices,
      const std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
      const std::vector<std::set<SimplexId>> &separatricesSaddles) const;
#endif

    /**
     * @brief Flatten the vectors of vectors into their first component
     */
    void flattenSeparatricesVectors(
      std::vector<std::vector<ttk::Separatrix>> &separatrices,
      std::vector<std::vector<std::vector<ttk::dcg::Cell>>>
        &separatricesGeometry) const;
  };
} // namespace ttk

#ifdef TTK_ENABLE_OPENMP
template <typename dataType>
int ttk::MorseSmaleComplex3D::omp_setAscendingSeparatrices2(
  const std::vector<Separatrix> &separatrices,
  const std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
  const std::vector<std::set<SimplexId>> &separatricesSaddles) const {
#ifndef TTK_ENABLE_KAMIKAZE
  if(!outputSeparatrices2_numberOfPoints_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to "
                 "numberOfPoints is null."
              << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_points_) {
    std::cerr
      << "[MorseSmaleComplex3D] 2-separatrices pointer to points is null."
      << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_numberOfCells_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to "
                 "numberOfCells is null."
              << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_cells_) {
    std::cerr
      << "[MorseSmaleComplex3D] 2-separatrices pointer to cells is null."
      << std::endl;
    return -1;
  }
  if(!inputScalarField_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to the input "
                 "scalar field is null."
              << std::endl;
    return -1;
  }
#endif

  const dataType *const scalars
    = static_cast<const dataType *>(inputScalarField_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionMaxima
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionMaxima_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionMinima
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionMinima_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionDiffs
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionDiffs_);

  SimplexId pointId = (*outputSeparatrices2_numberOfPoints_);
  SimplexId separatrixId = 0;
  if(outputSeparatrices2_cells_separatrixIds_
     and outputSeparatrices2_cells_separatrixIds_->size()) {
    separatrixId
      = *std::max_element(outputSeparatrices2_cells_separatrixIds_->begin(),
                          outputSeparatrices2_cells_separatrixIds_->end())
        + 1;
  }

  std::vector<SimplexId> separatrixIds(threadNumber_, 0);
  std::vector<SimplexId> numberOfPoints(threadNumber_, 0);
  std::vector<std::vector<float>> separatrices2_points(threadNumber_);
  std::vector<SimplexId> numberOfCells(threadNumber_, 0);
  std::vector<std::vector<SimplexId>> separatrices2_cells(threadNumber_);
  std::vector<std::vector<SimplexId>> separatrices2_cells_sourceIds(
    threadNumber_);
  std::vector<std::vector<SimplexId>> separatrices2_cells_separatrixIds(
    threadNumber_);
  std::vector<std::vector<char>> separatrices2_cells_separatrixTypes(
    threadNumber_);
  std::vector<std::vector<dataType>>
    separatrices2_cells_separatrixFunctionMaxima(threadNumber_);
  std::vector<std::vector<dataType>>
    separatrices2_cells_separatrixFunctionMinima(threadNumber_);
  std::vector<std::vector<dataType>>
    separatrices2_cells_separatrixFunctionDiffs(threadNumber_);
  std::vector<std::vector<char>> separatrices2_cells_isOnBoundary(
    threadNumber_);

  const SimplexId numberOfSeparatrices = separatrices.size();
#pragma omp parallel for num_threads(threadNumber_)
  for(SimplexId i = 0; i < numberOfSeparatrices; ++i) {
    const ThreadId threadId = omp_get_thread_num();
    const Separatrix &separatrix = separatrices[i];
    if(!separatrix.isValid_)
      continue;
    if(!separatrix.geometry_.size())
      continue;

    const dcg::Cell &saddle = separatrix.source_;
    const char separatrixType = 1;
    const SimplexId saddleId = saddle.id_;

    const dataType separatrixFunctionMinimum
      = discreteGradient_.scalarMin<dataType>(saddle, scalars);
    dataType separatrixFunctionMaximum{};

    // get separatrix infos
    char isOnBoundary{};
    bool isFirst = true;
    for(const SimplexId saddle2Id : separatricesSaddles[i]) {
      if(inputTriangulation_->isTriangleOnBoundary(saddle2Id))
        ++isOnBoundary;

      if(isFirst) {
        separatrixFunctionMaximum = discreteGradient_.scalarMax<dataType>(
          dcg::Cell(2, saddle2Id), scalars);
        isFirst = false;
      } else {
        separatrixFunctionMaximum = std::max(
          separatrixFunctionMaximum, discreteGradient_.scalarMax<dataType>(
                                       dcg::Cell(2, saddle2Id), scalars));
      }
    }

    const dataType separatrixFunctionDiff
      = separatrixFunctionMaximum - separatrixFunctionMinimum;

    isFirst = true;
    for(const SimplexId geometryId : separatrix.geometry_) {
      for(const dcg::Cell &edge : separatricesGeometry[geometryId]) {
        const SimplexId edgeId = edge.id_;

        // Transform to dual : edge -> polygon
        std::vector<SimplexId> polygon;
        getDualPolygon(edgeId, polygon);

        const SimplexId vertexNumber = polygon.size();
        if(vertexNumber > 2) {
          sortDualPolygonVertices(polygon);

          // add the polygon
          separatrices2_cells[threadId].push_back(vertexNumber);
          for(SimplexId k = 0; k < vertexNumber; ++k) {
            const SimplexId tetraId = polygon[k];
            float point[3];
            inputTriangulation_->getTetraIncenter(tetraId, point);

            separatrices2_points[threadId].push_back(point[0]);
            separatrices2_points[threadId].push_back(point[1]);
            separatrices2_points[threadId].push_back(point[2]);
            separatrices2_cells[threadId].push_back(numberOfPoints[threadId]);
            ++numberOfPoints[threadId];
          }

          if(outputSeparatrices2_cells_sourceIds_)
            separatrices2_cells_sourceIds[threadId].push_back(saddleId);
          if(outputSeparatrices2_cells_separatrixIds_)
            separatrices2_cells_separatrixIds[threadId].push_back(
              separatrixIds[threadId]);
          if(outputSeparatrices2_cells_separatrixTypes_)
            separatrices2_cells_separatrixTypes[threadId].push_back(
              separatrixType);
          if(outputSeparatrices2_cells_separatrixFunctionMaxima)
            separatrices2_cells_separatrixFunctionMaxima[threadId].push_back(
              separatrixFunctionMaximum);
          if(outputSeparatrices2_cells_separatrixFunctionMinima)
            separatrices2_cells_separatrixFunctionMinima[threadId].push_back(
              separatrixFunctionMinimum);
          if(outputSeparatrices2_cells_separatrixFunctionDiffs)
            separatrices2_cells_separatrixFunctionDiffs[threadId].push_back(
              separatrixFunctionDiff);
          if(outputSeparatrices2_cells_isOnBoundary_)
            separatrices2_cells_isOnBoundary[threadId].push_back(isOnBoundary);
          ++numberOfCells[threadId];

          isFirst = false;
        }
      }
    }

    if(!isFirst)
      ++separatrixIds[threadId];
  }

  const SimplexId oldPointSize = outputSeparatrices2_points_->size();
  const SimplexId oldCellSize = outputSeparatrices2_cells_->size();
  const SimplexId oldFieldSize = outputSeparatrices2_cells_sourceIds_->size();
  SimplexId totalNumberOfPoints = 0;
  SimplexId totalNumberOfCells = 0;
  {
    SimplexId npoints = 0;
    SimplexId ncells = 0;
    SimplexId nnpoints = pointId;
    SimplexId nncells = 0;
    SimplexId tmp_separatrixId = separatrixId;
    std::vector<SimplexId> offsetPoints(threadNumber_, 0);
    std::vector<SimplexId> offsetCells(threadNumber_, 0);
    std::vector<SimplexId> offsetNPoints(threadNumber_, 0);
    std::vector<SimplexId> offsetNCells(threadNumber_, 0);
    std::vector<SimplexId> offsetSeparatrixIds(threadNumber_, 0);

    for(ThreadId i = 0; i < threadNumber_; ++i) {
      offsetPoints[i] = npoints;
      offsetCells[i] = ncells;
      offsetNPoints[i] = nnpoints;
      offsetNCells[i] = nncells;
      offsetSeparatrixIds[i] = tmp_separatrixId;

      npoints += separatrices2_points[i].size();
      ncells += separatrices2_cells[i].size();
      nnpoints += numberOfPoints[i];
      nncells += numberOfCells[i];
      tmp_separatrixId += separatrixIds[i];

      totalNumberOfPoints += numberOfPoints[i];
      totalNumberOfCells += numberOfCells[i];
    }

    outputSeparatrices2_points_->resize(oldPointSize + npoints);
    outputSeparatrices2_cells_->resize(oldCellSize + ncells);
    if(outputSeparatrices2_cells_sourceIds_)
      outputSeparatrices2_cells_sourceIds_->resize(oldFieldSize
                                                   + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixIds_)
      outputSeparatrices2_cells_separatrixIds_->resize(oldFieldSize
                                                       + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixTypes_)
      outputSeparatrices2_cells_separatrixTypes_->resize(oldFieldSize
                                                         + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixFunctionMaxima)
      outputSeparatrices2_cells_separatrixFunctionMaxima->resize(
        oldFieldSize + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixFunctionMinima)
      outputSeparatrices2_cells_separatrixFunctionMinima->resize(
        oldFieldSize + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixFunctionDiffs)
      outputSeparatrices2_cells_separatrixFunctionDiffs->resize(
        oldFieldSize + totalNumberOfCells);
    if(outputSeparatrices2_cells_isOnBoundary_)
      outputSeparatrices2_cells_isOnBoundary_->resize(oldFieldSize
                                                      + totalNumberOfCells);
#pragma omp parallel for num_threads(threadNumber_)
    for(ThreadId i = 0; i < threadNumber_; ++i) {
      // reduce: points
      const SimplexId tmp_npoints = separatrices2_points[i].size();
      const SimplexId tmp_offsetPoints = offsetPoints[i];
      for(SimplexId j = 0; j < tmp_npoints; ++j)
        (*outputSeparatrices2_points_)[oldPointSize + tmp_offsetPoints + j]
          = separatrices2_points[i][j];

      // reduce: cells
      const SimplexId tmp_ncells = separatrices2_cells[i].size();
      const SimplexId tmp_offsetCells = offsetCells[i];
      for(SimplexId j = 0; j < tmp_ncells;) {
        const SimplexId cellSize = separatrices2_cells[i][j];
        (*outputSeparatrices2_cells_)[oldCellSize + tmp_offsetCells + j]
          = cellSize;
        for(SimplexId k = 0; k < cellSize; ++k) {
          const SimplexId tmp_pointId = separatrices2_cells[i][j + k + 1];
          (*outputSeparatrices2_cells_)[oldCellSize + tmp_offsetCells + j + k
                                        + 1]
            = offsetNPoints[i] + tmp_pointId;
        }
        j += (cellSize + 1);
      }

      // reduce: fields
      for(SimplexId j = 0; j < numberOfCells[i]; ++j) {
        if(outputSeparatrices2_cells_sourceIds_)
          (*outputSeparatrices2_cells_sourceIds_)[oldFieldSize + offsetNCells[i]
                                                  + j]
            = separatrices2_cells_sourceIds[i][j];
        if(outputSeparatrices2_cells_separatrixIds_)
          (*outputSeparatrices2_cells_separatrixIds_)[oldFieldSize
                                                      + offsetNCells[i] + j]
            = offsetSeparatrixIds[i] + separatrices2_cells_separatrixIds[i][j];
        if(outputSeparatrices2_cells_separatrixTypes_)
          (*outputSeparatrices2_cells_separatrixTypes_)[oldFieldSize
                                                        + offsetNCells[i] + j]
            = separatrices2_cells_separatrixTypes[i][j];
        if(outputSeparatrices2_cells_separatrixFunctionMaxima)
          (*outputSeparatrices2_cells_separatrixFunctionMaxima)
            [oldFieldSize + offsetNCells[i] + j]
            = separatrices2_cells_separatrixFunctionMaxima[i][j];
        if(outputSeparatrices2_cells_separatrixFunctionMinima)
          (*outputSeparatrices2_cells_separatrixFunctionMinima)
            [oldFieldSize + offsetNCells[i] + j]
            = separatrices2_cells_separatrixFunctionMinima[i][j];
        if(outputSeparatrices2_cells_separatrixFunctionDiffs)
          (*outputSeparatrices2_cells_separatrixFunctionDiffs)[oldFieldSize
                                                               + offsetNCells[i]
                                                               + j]
            = separatrices2_cells_separatrixFunctionDiffs[i][j];
        if(outputSeparatrices2_cells_isOnBoundary_)
          (*outputSeparatrices2_cells_isOnBoundary_)[oldFieldSize
                                                     + offsetNCells[i] + j]
            = separatrices2_cells_isOnBoundary[i][j];
      }
    }
  }

  (*outputSeparatrices2_numberOfPoints_) += totalNumberOfPoints;
  (*outputSeparatrices2_numberOfCells_) += totalNumberOfCells;

  return 0;
}
#endif

template <typename dataType>
int ttk::MorseSmaleComplex3D::setAscendingSeparatrices2(
  const std::vector<Separatrix> &separatrices,
  const std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
  const std::vector<std::set<SimplexId>> &separatricesSaddles) const {
#ifndef TTK_ENABLE_KAMIKAZE
  if(!outputSeparatrices2_numberOfPoints_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to "
                 "numberOfPoints is null."
              << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_points_) {
    std::cerr
      << "[MorseSmaleComplex3D] 2-separatrices pointer to points is null."
      << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_numberOfCells_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to "
                 "numberOfCells is null."
              << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_cells_) {
    std::cerr
      << "[MorseSmaleComplex3D] 2-separatrices pointer to cells is null."
      << std::endl;
    return -1;
  }
  if(!inputScalarField_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to the input "
                 "scalar field is null."
              << std::endl;
    return -1;
  }
#endif

  const dataType *const scalars
    = static_cast<const dataType *>(inputScalarField_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionMaxima
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionMaxima_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionMinima
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionMinima_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionDiffs
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionDiffs_);

  SimplexId pointId = (*outputSeparatrices2_numberOfPoints_);
  SimplexId cellId = (*outputSeparatrices2_numberOfCells_);
  SimplexId separatrixId = 0;
  if(outputSeparatrices2_cells_separatrixIds_
     and outputSeparatrices2_cells_separatrixIds_->size()) {
    separatrixId
      = *std::max_element(outputSeparatrices2_cells_separatrixIds_->begin(),
                          outputSeparatrices2_cells_separatrixIds_->end())
        + 1;
  }

  const SimplexId numberOfCells = inputTriangulation_->getNumberOfCells();
  std::vector<SimplexId> isVisited(numberOfCells, -1);

  const SimplexId numberOfSeparatrices = separatrices.size();
  for(SimplexId i = 0; i < numberOfSeparatrices; ++i) {
    const Separatrix &separatrix = separatrices[i];
    if(!separatrix.isValid_)
      continue;
    if(!separatrix.geometry_.size())
      continue;

    const dcg::Cell &saddle = separatrix.source_;
    const char separatrixType = 1;
    const SimplexId saddleId = saddle.id_;

    const dataType separatrixFunctionMinimum
      = discreteGradient_.scalarMin<dataType>(saddle, scalars);
    dataType separatrixFunctionMaximum{};

    // get separatrix infos
    char isOnBoundary{};
    bool isFirst = true;
    for(const SimplexId saddle2Id : separatricesSaddles[i]) {
      if(inputTriangulation_->isTriangleOnBoundary(saddle2Id))
        ++isOnBoundary;

      if(isFirst) {
        separatrixFunctionMaximum = discreteGradient_.scalarMax<dataType>(
          dcg::Cell(2, saddle2Id), scalars);
        isFirst = false;
      } else {
        separatrixFunctionMaximum = std::max(
          separatrixFunctionMaximum, discreteGradient_.scalarMax<dataType>(
                                       dcg::Cell(2, saddle2Id), scalars));
      }
    }

    const dataType separatrixFunctionDiff
      = separatrixFunctionMaximum - separatrixFunctionMinimum;

    isFirst = true;
    for(const SimplexId geometryId : separatrix.geometry_) {
      for(const dcg::Cell &edge : separatricesGeometry[geometryId]) {
        const SimplexId edgeId = edge.id_;

        // Transform to dual : edge -> polygon
        std::vector<SimplexId> polygon;
        getDualPolygon(edgeId, polygon);

        const SimplexId vertexNumber = polygon.size();
        if(vertexNumber > 2) {
          sortDualPolygonVertices(polygon);

          // add the polygon
          outputSeparatrices2_cells_->push_back(vertexNumber);

          float point[3];
          for(SimplexId j = 0; j < vertexNumber; ++j) {
            const SimplexId tetraId = polygon[j];
            inputTriangulation_->getTetraIncenter(tetraId, point);

            if(isVisited[tetraId] == -1) {
              outputSeparatrices2_points_->push_back(point[0]);
              outputSeparatrices2_points_->push_back(point[1]);
              outputSeparatrices2_points_->push_back(point[2]);

              outputSeparatrices2_cells_->push_back(pointId);

              isVisited[tetraId] = pointId;
              ++pointId;
            } else
              outputSeparatrices2_cells_->push_back(isVisited[tetraId]);
          }

          if(outputSeparatrices2_cells_sourceIds_)
            outputSeparatrices2_cells_sourceIds_->push_back(saddleId);
          if(outputSeparatrices2_cells_separatrixIds_)
            outputSeparatrices2_cells_separatrixIds_->push_back(separatrixId);
          if(outputSeparatrices2_cells_separatrixTypes_)
            outputSeparatrices2_cells_separatrixTypes_->push_back(
              separatrixType);
          if(outputSeparatrices2_cells_separatrixFunctionMaxima)
            outputSeparatrices2_cells_separatrixFunctionMaxima->push_back(
              separatrixFunctionMaximum);
          if(outputSeparatrices2_cells_separatrixFunctionMinima)
            outputSeparatrices2_cells_separatrixFunctionMinima->push_back(
              separatrixFunctionMinimum);
          if(outputSeparatrices2_cells_separatrixFunctionDiffs)
            outputSeparatrices2_cells_separatrixFunctionDiffs->push_back(
              separatrixFunctionDiff);
          if(outputSeparatrices2_cells_isOnBoundary_)
            outputSeparatrices2_cells_isOnBoundary_->push_back(isOnBoundary);

          ++cellId;
          isFirst = false;
        }
      }
    }

    if(!isFirst)
      ++separatrixId;
  }

  (*outputSeparatrices2_numberOfPoints_) = pointId;
  (*outputSeparatrices2_numberOfCells_) = cellId;

  return 0;
}

#ifdef TTK_ENABLE_OPENMP
template <typename dataType>
int ttk::MorseSmaleComplex3D::omp_setDescendingSeparatrices2(
  const std::vector<Separatrix> &separatrices,
  const std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
  const std::vector<std::set<SimplexId>> &separatricesSaddles) const {
#ifndef TTK_ENABLE_KAMIKAZE
  if(!outputSeparatrices2_numberOfPoints_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to "
                 "numberOfPoints is null."
              << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_points_) {
    std::cerr
      << "[MorseSmaleComplex3D] 2-separatrices pointer to points is null."
      << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_numberOfCells_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to "
                 "numberOfCells is null."
              << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_cells_) {
    std::cerr
      << "[MorseSmaleComplex3D] 2-separatrices pointer to cells is null."
      << std::endl;
    return -1;
  }
  if(!inputScalarField_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to the input "
                 "scalar field is null."
              << std::endl;
    return -1;
  }
#endif

  const dataType *const scalars
    = static_cast<const dataType *>(inputScalarField_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionMaxima
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionMaxima_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionMinima
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionMinima_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionDiffs
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionDiffs_);

  SimplexId pointId = (*outputSeparatrices2_numberOfPoints_);
  SimplexId separatrixId = 0;
  if(outputSeparatrices2_cells_separatrixIds_
     and outputSeparatrices2_cells_separatrixIds_->size()) {
    separatrixId
      = *std::max_element(outputSeparatrices2_cells_separatrixIds_->begin(),
                          outputSeparatrices2_cells_separatrixIds_->end())
        + 1;
  }

  std::vector<SimplexId> separatrixIds(threadNumber_, 0);
  std::vector<SimplexId> numberOfPoints(threadNumber_, 0);
  std::vector<std::vector<float>> separatrices2_points(threadNumber_);
  std::vector<SimplexId> numberOfCells(threadNumber_, 0);
  std::vector<std::vector<SimplexId>> separatrices2_cells(threadNumber_);
  std::vector<std::vector<SimplexId>> separatrices2_cells_sourceIds(
    threadNumber_);
  std::vector<std::vector<SimplexId>> separatrices2_cells_separatrixIds(
    threadNumber_);
  std::vector<std::vector<char>> separatrices2_cells_separatrixTypes(
    threadNumber_);
  std::vector<std::vector<dataType>>
    separatrices2_cells_separatrixFunctionMaxima(threadNumber_);
  std::vector<std::vector<dataType>>
    separatrices2_cells_separatrixFunctionMinima(threadNumber_);
  std::vector<std::vector<dataType>>
    separatrices2_cells_separatrixFunctionDiffs(threadNumber_);
  std::vector<std::vector<char>> separatrices2_cells_isOnBoundary(
    threadNumber_);

  const SimplexId numberOfSeparatrices = separatrices.size();
#pragma omp parallel for num_threads(threadNumber_)
  for(SimplexId i = 0; i < numberOfSeparatrices; ++i) {
    const ThreadId threadId = omp_get_thread_num();
    const Separatrix &separatrix = separatrices[i];
    if(!separatrix.isValid_)
      continue;
    if(!separatrix.geometry_.size())
      continue;

    const dcg::Cell &saddle = separatrix.source_;
    const char separatrixType = 2;
    const SimplexId saddleId = saddle.id_;

    const dataType separatrixFunctionMaximum
      = discreteGradient_.scalarMax<dataType>(saddle, scalars);
    dataType separatrixFunctionMinimum{};

    // get separatrix infos
    char isOnBoundary{};
    bool isFirst = true;
    for(const SimplexId saddle1Id : separatricesSaddles[i]) {
      if(inputTriangulation_->isEdgeOnBoundary(saddle1Id))
        ++isOnBoundary;

      if(isFirst) {
        separatrixFunctionMinimum = discreteGradient_.scalarMin<dataType>(
          dcg::Cell(1, saddle1Id), scalars);
        isFirst = false;
      } else {
        separatrixFunctionMinimum = std::min(
          separatrixFunctionMinimum, discreteGradient_.scalarMin<dataType>(
                                       dcg::Cell(1, saddle1Id), scalars));
      }
    }

    const dataType separatrixFunctionDiff
      = separatrixFunctionMaximum - separatrixFunctionMinimum;

    isFirst = true;
    for(const SimplexId geometryId : separatrix.geometry_) {
      for(const dcg::Cell &cell : separatricesGeometry[geometryId]) {
        const SimplexId triangleId = cell.id_;

        separatrices2_cells[threadId].push_back(3);
        for(int k = 0; k < 3; ++k) {
          SimplexId vertexId;
          inputTriangulation_->getTriangleVertex(triangleId, k, vertexId);
          float point[3];
          inputTriangulation_->getVertexPoint(
            vertexId, point[0], point[1], point[2]);

          separatrices2_points[threadId].push_back(point[0]);
          separatrices2_points[threadId].push_back(point[1]);
          separatrices2_points[threadId].push_back(point[2]);
          separatrices2_cells[threadId].push_back(numberOfPoints[threadId]);
          ++numberOfPoints[threadId];
        }

        if(outputSeparatrices2_cells_sourceIds_)
          separatrices2_cells_sourceIds[threadId].push_back(saddleId);
        if(outputSeparatrices2_cells_separatrixIds_)
          separatrices2_cells_separatrixIds[threadId].push_back(
            separatrixIds[threadId]);
        if(outputSeparatrices2_cells_separatrixTypes_)
          separatrices2_cells_separatrixTypes[threadId].push_back(
            separatrixType);
        if(outputSeparatrices2_cells_separatrixFunctionMaxima)
          separatrices2_cells_separatrixFunctionMaxima[threadId].push_back(
            separatrixFunctionMaximum);
        if(outputSeparatrices2_cells_separatrixFunctionMinima)
          separatrices2_cells_separatrixFunctionMinima[threadId].push_back(
            separatrixFunctionMinimum);
        if(outputSeparatrices2_cells_separatrixFunctionDiffs)
          separatrices2_cells_separatrixFunctionDiffs[threadId].push_back(
            separatrixFunctionDiff);
        if(outputSeparatrices2_cells_isOnBoundary_)
          separatrices2_cells_isOnBoundary[threadId].push_back(isOnBoundary);
        ++numberOfCells[threadId];

        isFirst = false;
      }
    }

    if(!isFirst)
      ++separatrixIds[threadId];
  }

  const SimplexId oldPointSize = outputSeparatrices2_points_->size();
  const SimplexId oldCellSize = outputSeparatrices2_cells_->size();
  const SimplexId oldFieldSize = outputSeparatrices2_cells_sourceIds_->size();
  SimplexId totalNumberOfPoints = 0;
  SimplexId totalNumberOfCells = 0;
  {
    SimplexId npoints = 0;
    SimplexId ncells = 0;
    SimplexId nnpoints = pointId;
    SimplexId nncells = 0;
    SimplexId tmp_separatrixId = separatrixId;
    std::vector<SimplexId> offsetPoints(threadNumber_, 0);
    std::vector<SimplexId> offsetCells(threadNumber_, 0);
    std::vector<SimplexId> offsetNPoints(threadNumber_, 0);
    std::vector<SimplexId> offsetNCells(threadNumber_, 0);
    std::vector<SimplexId> offsetSeparatrixIds(threadNumber_, 0);

    for(ThreadId i = 0; i < threadNumber_; ++i) {
      offsetPoints[i] = npoints;
      offsetCells[i] = ncells;
      offsetNPoints[i] = nnpoints;
      offsetNCells[i] = nncells;
      offsetSeparatrixIds[i] = tmp_separatrixId;

      npoints += separatrices2_points[i].size();
      ncells += separatrices2_cells[i].size();
      nnpoints += numberOfPoints[i];
      nncells += numberOfCells[i];
      tmp_separatrixId += separatrixIds[i];

      totalNumberOfPoints += numberOfPoints[i];
      totalNumberOfCells += numberOfCells[i];
    }

    outputSeparatrices2_points_->resize(oldPointSize + npoints);
    outputSeparatrices2_cells_->resize(oldCellSize + ncells);
    if(outputSeparatrices2_cells_sourceIds_)
      outputSeparatrices2_cells_sourceIds_->resize(oldFieldSize
                                                   + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixIds_)
      outputSeparatrices2_cells_separatrixIds_->resize(oldFieldSize
                                                       + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixTypes_)
      outputSeparatrices2_cells_separatrixTypes_->resize(oldFieldSize
                                                         + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixFunctionMaxima)
      outputSeparatrices2_cells_separatrixFunctionMaxima->resize(
        oldFieldSize + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixFunctionMinima)
      outputSeparatrices2_cells_separatrixFunctionMinima->resize(
        oldFieldSize + totalNumberOfCells);
    if(outputSeparatrices2_cells_separatrixFunctionDiffs)
      outputSeparatrices2_cells_separatrixFunctionDiffs->resize(
        oldFieldSize + totalNumberOfCells);
    if(outputSeparatrices2_cells_isOnBoundary_)
      outputSeparatrices2_cells_isOnBoundary_->resize(oldFieldSize
                                                      + totalNumberOfCells);
#pragma omp parallel for num_threads(threadNumber_)
    for(ThreadId i = 0; i < threadNumber_; ++i) {
      // reduce: points
      const SimplexId tmp_npoints = separatrices2_points[i].size();
      const SimplexId tmp_offsetPoints = offsetPoints[i];
      for(SimplexId j = 0; j < tmp_npoints; ++j)
        (*outputSeparatrices2_points_)[oldPointSize + tmp_offsetPoints + j]
          = separatrices2_points[i][j];

      // reduce: cells
      const SimplexId tmp_ncells = separatrices2_cells[i].size();
      const SimplexId tmp_offsetCells = offsetCells[i];
      for(SimplexId j = 0; j < tmp_ncells;) {
        const SimplexId cellSize = separatrices2_cells[i][j];
        (*outputSeparatrices2_cells_)[oldCellSize + tmp_offsetCells + j]
          = cellSize;
        for(SimplexId k = 0; k < cellSize; ++k) {
          const SimplexId tmp_pointId = separatrices2_cells[i][j + k + 1];
          (*outputSeparatrices2_cells_)[oldCellSize + tmp_offsetCells + j + k
                                        + 1]
            = offsetNPoints[i] + tmp_pointId;
        }
        j += (cellSize + 1);
      }

      // reduce: fields
      for(SimplexId j = 0; j < numberOfCells[i]; ++j) {
        if(outputSeparatrices2_cells_sourceIds_)
          (*outputSeparatrices2_cells_sourceIds_)[oldFieldSize + offsetNCells[i]
                                                  + j]
            = separatrices2_cells_sourceIds[i][j];
        if(outputSeparatrices2_cells_separatrixIds_)
          (*outputSeparatrices2_cells_separatrixIds_)[oldFieldSize
                                                      + offsetNCells[i] + j]
            = offsetSeparatrixIds[i] + separatrices2_cells_separatrixIds[i][j];
        if(outputSeparatrices2_cells_separatrixTypes_)
          (*outputSeparatrices2_cells_separatrixTypes_)[oldFieldSize
                                                        + offsetNCells[i] + j]
            = separatrices2_cells_separatrixTypes[i][j];
        if(outputSeparatrices2_cells_separatrixFunctionMaxima)
          (*outputSeparatrices2_cells_separatrixFunctionMaxima)
            [oldFieldSize + offsetNCells[i] + j]
            = separatrices2_cells_separatrixFunctionMaxima[i][j];
        if(outputSeparatrices2_cells_separatrixFunctionMinima)
          (*outputSeparatrices2_cells_separatrixFunctionMinima)
            [oldFieldSize + offsetNCells[i] + j]
            = separatrices2_cells_separatrixFunctionMinima[i][j];
        if(outputSeparatrices2_cells_separatrixFunctionDiffs)
          (*outputSeparatrices2_cells_separatrixFunctionDiffs)[oldFieldSize
                                                               + offsetNCells[i]
                                                               + j]
            = separatrices2_cells_separatrixFunctionDiffs[i][j];
        if(outputSeparatrices2_cells_isOnBoundary_)
          (*outputSeparatrices2_cells_isOnBoundary_)[oldFieldSize
                                                     + offsetNCells[i] + j]
            = separatrices2_cells_isOnBoundary[i][j];
      }
    }
  }

  (*outputSeparatrices2_numberOfPoints_) += totalNumberOfPoints;
  (*outputSeparatrices2_numberOfCells_) += totalNumberOfCells;

  return 0;
}
#endif

template <typename dataType>
int ttk::MorseSmaleComplex3D::setDescendingSeparatrices2(
  const std::vector<Separatrix> &separatrices,
  const std::vector<std::vector<dcg::Cell>> &separatricesGeometry,
  const std::vector<std::set<SimplexId>> &separatricesSaddles) const {
#ifndef TTK_ENABLE_KAMIKAZE
  if(!outputSeparatrices2_numberOfPoints_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to "
                 "numberOfPoints is null."
              << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_points_) {
    std::cerr
      << "[MorseSmaleComplex3D] 2-separatrices pointer to points is null."
      << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_numberOfCells_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to "
                 "numberOfCells is null."
              << std::endl;
    return -1;
  }
  if(!outputSeparatrices2_cells_) {
    std::cerr
      << "[MorseSmaleComplex3D] 2-separatrices pointer to cells is null."
      << std::endl;
    return -1;
  }
  if(!inputScalarField_) {
    std::cerr << "[MorseSmaleComplex3D] 2-separatrices pointer to the input "
                 "scalar field is null."
              << std::endl;
    return -1;
  }
#endif

  const dataType *const scalars
    = static_cast<const dataType *>(inputScalarField_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionMaxima
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionMaxima_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionMinima
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionMinima_);
  std::vector<dataType> *outputSeparatrices2_cells_separatrixFunctionDiffs
    = static_cast<std::vector<dataType> *>(
      outputSeparatrices2_cells_separatrixFunctionDiffs_);

  SimplexId pointId = (*outputSeparatrices2_numberOfPoints_);
  SimplexId cellId = (*outputSeparatrices2_numberOfCells_);
  SimplexId separatrixId = 0;
  if(outputSeparatrices2_cells_separatrixIds_
     and outputSeparatrices2_cells_separatrixIds_->size()) {
    separatrixId
      = *std::max_element(outputSeparatrices2_cells_separatrixIds_->begin(),
                          outputSeparatrices2_cells_separatrixIds_->end())
        + 1;
  }

  const SimplexId numberOfVertices = inputTriangulation_->getNumberOfVertices();
  std::vector<SimplexId> isVisited(numberOfVertices, -1);

  const SimplexId numberOfSeparatrices = separatrices.size();
  for(SimplexId i = 0; i < numberOfSeparatrices; ++i) {
    const Separatrix &separatrix = separatrices[i];
    if(!separatrix.isValid_)
      continue;
    if(!separatrix.geometry_.size())
      continue;

    const dcg::Cell &saddle = separatrix.source_;
    const char separatrixType = 2;
    const SimplexId saddleId = saddle.id_;

    const dataType separatrixFunctionMaximum
      = discreteGradient_.scalarMax<dataType>(saddle, scalars);
    dataType separatrixFunctionMinimum{};

    // get separatrix infos
    char isOnBoundary{};
    bool isFirst = true;
    for(const SimplexId saddle1Id : separatricesSaddles[i]) {
      if(inputTriangulation_->isEdgeOnBoundary(saddle1Id))
        ++isOnBoundary;

      if(isFirst) {
        separatrixFunctionMinimum = discreteGradient_.scalarMin<dataType>(
          dcg::Cell(1, saddle1Id), scalars);
        isFirst = false;
      } else {
        separatrixFunctionMinimum = std::min(
          separatrixFunctionMinimum, discreteGradient_.scalarMin<dataType>(
                                       dcg::Cell(1, saddle1Id), scalars));
      }
    }

    const dataType separatrixFunctionDiff
      = separatrixFunctionMaximum - separatrixFunctionMinimum;

    isFirst = true;
    for(const SimplexId geometryId : separatrix.geometry_) {
      for(const dcg::Cell &cell : separatricesGeometry[geometryId]) {
        const SimplexId triangleId = cell.id_;

        outputSeparatrices2_cells_->push_back(3);
        float point[3];
        for(int k = 0; k < 3; ++k) {
          SimplexId vertexId;
          inputTriangulation_->getTriangleVertex(triangleId, k, vertexId);

          if(isVisited[vertexId] == -1) {
            inputTriangulation_->getVertexPoint(
              vertexId, point[0], point[1], point[2]);

            outputSeparatrices2_points_->push_back(point[0]);
            outputSeparatrices2_points_->push_back(point[1]);
            outputSeparatrices2_points_->push_back(point[2]);

            outputSeparatrices2_cells_->push_back(pointId);

            isVisited[vertexId] = pointId;
            ++pointId;
          } else
            outputSeparatrices2_cells_->push_back(isVisited[vertexId]);
        }
        if(outputSeparatrices2_cells_sourceIds_)
          outputSeparatrices2_cells_sourceIds_->push_back(saddleId);
        if(outputSeparatrices2_cells_separatrixIds_)
          outputSeparatrices2_cells_separatrixIds_->push_back(separatrixId);
        if(outputSeparatrices2_cells_separatrixTypes_)
          outputSeparatrices2_cells_separatrixTypes_->push_back(separatrixType);
        if(outputSeparatrices2_cells_separatrixFunctionMaxima)
          outputSeparatrices2_cells_separatrixFunctionMaxima->push_back(
            separatrixFunctionMaximum);
        if(outputSeparatrices2_cells_separatrixFunctionMinima)
          outputSeparatrices2_cells_separatrixFunctionMinima->push_back(
            separatrixFunctionMinimum);
        if(outputSeparatrices2_cells_separatrixFunctionDiffs)
          outputSeparatrices2_cells_separatrixFunctionDiffs->push_back(
            separatrixFunctionDiff);
        if(outputSeparatrices2_cells_isOnBoundary_)
          outputSeparatrices2_cells_isOnBoundary_->push_back(isOnBoundary);

        ++cellId;
        isFirst = false;
      }
    }

    if(!isFirst)
      ++separatrixId;
  }

  (*outputSeparatrices2_numberOfPoints_) = pointId;
  (*outputSeparatrices2_numberOfCells_) = cellId;

  return 0;
}

template <typename dataType, typename idType>
int ttk::MorseSmaleComplex3D::execute() {
#ifndef TTK_ENABLE_KAMIKAZE
  if(!inputScalarField_) {
    std::cerr
      << "[MorseSmaleComplex3D] Error: input scalar field pointer is null."
      << std::endl;
    return -1;
  }

  if(!inputOffsets_) {
    std::cerr
      << "[MorseSmaleComplex3D] Error: input offset field pointer is null."
      << std::endl;
    return -1;
  }
#endif
  Timer t;

  // nullptr_t is implicitly convertible and comparable to any pointer type
  // or pointer-to-member type.
  SimplexId *ascendingManifold
    = static_cast<SimplexId *>(outputAscendingManifold_);
  SimplexId *descendingManifold
    = static_cast<SimplexId *>(outputDescendingManifold_);
  SimplexId *morseSmaleManifold
    = static_cast<SimplexId *>(outputMorseSmaleManifold_);

  discreteGradient_.setThreadNumber(threadNumber_);
  discreteGradient_.setDebugLevel(debugLevel_);
  {
    Timer tmp;
    discreteGradient_.buildGradient<dataType, idType>();

    {
      std::stringstream msg;
      msg << "[MorseSmaleComplex3D] Discrete gradient overall computed in "
          << tmp.getElapsedTime() << " s." << std::endl;
      dMsg(std::cout, msg.str(), timeMsg);
    }
  }

  if(ReturnSaddleConnectors) {
    discreteGradient_.reverseGradient<dataType, idType>();
  }

  std::vector<dcg::Cell> criticalPoints;
  discreteGradient_.getCriticalPoints(criticalPoints);

  std::vector<std::vector<Separatrix>> separatrices1{};
  std::vector<std::vector<std::vector<dcg::Cell>>> separatricesGeometry1;

  // 1-separatrices
  if(ComputeDescendingSeparatrices1) {
    Timer tmp;
    separatrices1.emplace_back();
    separatricesGeometry1.emplace_back();

    getDescendingSeparatrices1(
      criticalPoints, separatrices1.back(), separatricesGeometry1.back());

    {
      std::stringstream msg;
      msg << "[MorseSmaleComplex3D] Descending 1-separatrices computed in "
          << tmp.getElapsedTime() << " s." << std::endl;
      dMsg(std::cout, msg.str(), timeMsg);
    }
  }

  if(ComputeAscendingSeparatrices1) {
    Timer tmp;
    separatrices1.emplace_back();
    separatricesGeometry1.emplace_back();

    getAscendingSeparatrices1(
      criticalPoints, separatrices1.back(), separatricesGeometry1.back());

    {
      std::stringstream msg;
      msg << "[MorseSmaleComplex3D] Ascending 1-separatrices computed in "
          << tmp.getElapsedTime() << " s." << std::endl;
      dMsg(std::cout, msg.str(), timeMsg);
    }
  }

  // saddle-connectors
  if(ComputeSaddleConnectors) {
    Timer tmp;
    separatrices1.emplace_back();
    separatricesGeometry1.emplace_back();

    getSaddleConnectors(
      criticalPoints, separatrices1.back(), separatricesGeometry1.back());

    {
      std::stringstream msg;
      msg << "[MorseSmaleComplex3D] Saddle connectors computed in "
          << tmp.getElapsedTime() << " s." << std::endl;
      dMsg(std::cout, msg.str(), timeMsg);
    }
  }

  if(ComputeDescendingSeparatrices1 || ComputeAscendingSeparatrices1
     || ComputeSaddleConnectors) {
    Timer tmp{};

    flattenSeparatricesVectors(separatrices1, separatricesGeometry1);
    setSeparatrices1<dataType>(separatrices1[0], separatricesGeometry1[0]);

    {
      std::stringstream msg;
      msg << "[MorseSmaleComplex3D] 1-separatrices set in "
          << tmp.getElapsedTime() << " s." << std::endl;
      dMsg(std::cout, msg.str(), timeMsg);
    }
  }

  // 2-separatrices
  if(ComputeDescendingSeparatrices2) {
    Timer tmp;
    std::vector<Separatrix> separatrices;
    std::vector<std::vector<dcg::Cell>> separatricesGeometry;
    std::vector<std::set<SimplexId>> separatricesSaddles;
    getDescendingSeparatrices2(
      criticalPoints, separatrices, separatricesGeometry, separatricesSaddles);
#ifdef TTK_ENABLE_OPENMP
    if(PrioritizeSpeedOverMemory)
      omp_setDescendingSeparatrices2<dataType>(
        separatrices, separatricesGeometry, separatricesSaddles);
    else
#endif
      setDescendingSeparatrices2<dataType>(
        separatrices, separatricesGeometry, separatricesSaddles);

    {
      std::stringstream msg;
      msg << "[MorseSmaleComplex3D] Descending 2-separatrices computed in "
          << tmp.getElapsedTime() << " s." << std::endl;
      dMsg(std::cout, msg.str(), timeMsg);
    }
  }

  if(ComputeAscendingSeparatrices2) {
    Timer tmp;
    std::vector<Separatrix> separatrices;
    std::vector<std::vector<dcg::Cell>> separatricesGeometry;
    std::vector<std::set<SimplexId>> separatricesSaddles;
    getAscendingSeparatrices2(
      criticalPoints, separatrices, separatricesGeometry, separatricesSaddles);
#ifdef TTK_ENABLE_OPENMP
    if(PrioritizeSpeedOverMemory)
      omp_setAscendingSeparatrices2<dataType>(
        separatrices, separatricesGeometry, separatricesSaddles);
    else
#endif
      setAscendingSeparatrices2<dataType>(
        separatrices, separatricesGeometry, separatricesSaddles);

    {
      std::stringstream msg;
      msg << "[MorseSmaleComplex3D] Ascending 2-separatrices computed in "
          << tmp.getElapsedTime() << " s." << std::endl;
      dMsg(std::cout, msg.str(), timeMsg);
    }
  }

  std::vector<SimplexId> maxSeeds;
  {
    Timer tmp;

    SimplexId numberOfMaxima{};
    SimplexId numberOfMinima{};

    if(ascendingManifold)
      setAscendingSegmentation(
        criticalPoints, maxSeeds, ascendingManifold, numberOfMaxima);

    if(descendingManifold)
      setDescendingSegmentation(
        criticalPoints, descendingManifold, numberOfMinima);

    if(ascendingManifold and descendingManifold and morseSmaleManifold)
      setFinalSegmentation(numberOfMaxima, numberOfMinima, ascendingManifold,
                           descendingManifold, morseSmaleManifold);

    if(ascendingManifold or descendingManifold) {
      std::stringstream msg;
      msg << "[MorseSmaleComplex3D] Segmentation computed in "
          << tmp.getElapsedTime() << " s." << std::endl;
      dMsg(std::cout, msg.str(), timeMsg);
    }
  }

  if(outputCriticalPoints_numberOfPoints_ and outputSeparatrices1_points_) {
    std::vector<size_t> nCriticalPointsByDim{};
    discreteGradient_.setCriticalPoints<dataType>(
      criticalPoints, nCriticalPointsByDim);

    if(ascendingManifold and descendingManifold) {
      discreteGradient_.setManifoldSize(criticalPoints, nCriticalPointsByDim,
                                        maxSeeds, ascendingManifold,
                                        descendingManifold);
    }
  }

  {
    const SimplexId numberOfVertices
      = inputTriangulation_->getNumberOfVertices();
    std::stringstream msg;
    msg << "[MorseSmaleComplex3D] Data-set (" << numberOfVertices
        << " points) processed in " << t.getElapsedTime() << " s. ("
        << threadNumber_ << " thread(s))." << std::endl;
    dMsg(std::cout, msg.str(), timeMsg);
  }

  return 0;
}

template <typename dataType, typename idType>
int ttk::MorseSmaleComplex3D::computePersistencePairs(
  std::vector<std::tuple<SimplexId, SimplexId, dataType>>
    &pl_saddleSaddlePairs) {

  const dataType *scalars = static_cast<const dataType *>(inputScalarField_);

  std::vector<std::array<dcg::Cell, 2>> dmt_pairs;
  {
    // simplify to be PL-conformant
    discreteGradient_.setDebugLevel(debugLevel_);
    discreteGradient_.setThreadNumber(threadNumber_);
    discreteGradient_.setCollectPersistencePairs(false);
    discreteGradient_.buildGradient<dataType, idType>();
    discreteGradient_.reverseGradient<dataType, idType>();

    // collect saddle-saddle connections
    discreteGradient_.setCollectPersistencePairs(true);
    discreteGradient_.setOutputPersistencePairs(&dmt_pairs);
    discreteGradient_.reverseGradient<dataType, idType>(false);
  }

  // transform DMT pairs into PL pairs
  for(const auto &i : dmt_pairs) {
    const dcg::Cell &saddle1 = std::get<0>(i);
    const dcg::Cell &saddle2 = std::get<1>(i);

    SimplexId v0 = -1;
    dataType scalar0{};
    for(int j = 0; j < 2; ++j) {
      SimplexId vertexId;
      inputTriangulation_->getEdgeVertex(saddle1.id_, j, vertexId);
      const dataType vertexScalar = scalars[vertexId];
      // get the max vertex of the edge
      if(j == 0 || scalar0 > vertexScalar) {
        v0 = vertexId;
        scalar0 = vertexScalar;
      }
    }

    SimplexId v1 = -1;
    dataType scalar1{};
    for(int j = 0; j < 3; ++j) {
      SimplexId vertexId;
      inputTriangulation_->getTriangleVertex(saddle2.id_, j, vertexId);
      const dataType vertexScalar = scalars[vertexId];
      // get the min vertex of the triangle
      if(j == 0 || scalar1 < vertexScalar) {
        v1 = vertexId;
        scalar1 = vertexScalar;
      }
    }

    const dataType persistence = scalars[v1] - scalars[v0];

    if(v0 != -1 and v1 != -1 and persistence >= 0) {
      if(!inputTriangulation_->isVertexOnBoundary(v0)
         or !inputTriangulation_->isVertexOnBoundary(v1)) {
        pl_saddleSaddlePairs.emplace_back(v0, v1, persistence);
      }
    }
  }
  return 0;
}

#endif // MORSESMALECOMPLEX3D_H
