#pragma once

// base code includes
#include <Debug.h>

#include <unordered_map>

namespace ttk {

  class MapData : virtual public Debug {

  public:
    typedef std::tuple<long long, long long> Key;
    struct tuple_hash {
      template <typename T0, typename T1>
      std::size_t operator()(const std::tuple<T0, T1> &tuple) const {
        return std::hash<T0>()(std::get<0>(tuple))
               ^ std::hash<T1>()(std::get<1>(tuple));
      }
    };
    typedef std::unordered_map<Key, double, tuple_hash> Map;

    MapData() {
      this->setDebugMsgPrefix("MapData");
    };
    ~MapData() override{};

    template <typename T0, typename T1, typename T2>
    int generateMap(Map &map,
                    const int n,
                    const T0 *domain0,
                    const T1 *domain1,
                    const T2 *codomain) const {
      ttk::Timer timer;
      const std::string msg{"Generating Map"};
      this->printMsg(msg, 0, 0, 1, ttk::debug::LineMode::REPLACE);

      for(int i = 0; i < n; i++) {
        const auto d0 = static_cast<long long>(domain0[i]);
        const auto d1 = static_cast<long long>(domain1[i]);
        const auto v = static_cast<double>(codomain[i]);
        map.insert({{d0, d1}, v});
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), 1);
      return 1;
    };

    template <typename T0, typename T1, typename T2>
    int performLookup(T0 *outputArray,
                      Map &map,
                      const int m,
                      const T1 *lookup0,
                      const int nL0,
                      const T2 *lookup1,
                      const int nL1,
                      const T0 missingValue) const {
      ttk::Timer timer;
      const std::string msg{"Mapping Data"};
      this->printMsg(msg, 0, 0, 1, ttk::debug::LineMode::REPLACE);

      for(int i = 0; i < m; i++) {
        const auto l0 = static_cast<long long>(lookup0[std::min(i, nL0)]);
        const auto l1 = static_cast<long long>(lookup1[std::min(i, nL1)]);
        const auto &it = map.find({l0, l1});
        outputArray[i] = it == map.end() ? missingValue : it->second;
      }

      this->printMsg(msg, 1, timer.getElapsedTime(), 1);
      return 1;
    };
  };
} // namespace ttk