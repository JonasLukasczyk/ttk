class TreeRenderer {
  constructor(treeContainer){
    this.treeContainer = treeContainer;

    this.scale = 200;
    this.padding = 50;
    this.width = 800;
    this.height = 600;

    let topN = 3;
    this.streamgraph_options = {
      // maxwidth_root: 1957,
      maxwidth_root: 100,
      // use_relative_sizes: true,
      use_relative_sizes: false,
      padding: 10,
      edgeWidth: 1,
      topN: topN,
      color_scheme: d3.schemeSet3,
      color_of_ignored: "#9f9f9f"
    }
    this.streamgraph_options.padding_scaled = this.streamgraph_options.padding;

    this.donut_options = {
      innerRadius: 8,
      outerRadius: 20,
      padding: 10,
      topN: topN,
      color_scheme: d3.schemeSet3,
      color_of_ignored: "#9f9f9f"
    }

    this.svg = this.treeContainer.append("svg")
        .attr("height", this.height)
        .attr("width", this.width)
        // .attr("style", "border:5px solid #eaeaea");

    this.root = this.svg.append("g");
    this.coordinate_system = this.root.append("g");
    this.nodelayer = this.root.append("g");

    this.vtkDataSet = null;
    this.points = [];
    this.transform = null;
  }

  changeColorScheme(cs) {
    this.streamgraph_options.color_scheme = eval(`d3.${cs}`);
    this.donut_options.color_scheme = eval(`d3.${cs}`);

    if (this.tree !== null) {
      this.tree.createColorMapping();
      kdeRenderer.setColorMap(this.streamgraph_options.color_scheme, this.tree.color_mapping);
    }
  }

  setVtkDataSet(dataset){
    this.vtkDataSet = dataset;

    // point information
    const _coords = this.vtkDataSet.points.coordinates.data;
    let coords = Array.from(_coords);  // convert from Float64Array to normal Array, so we can use `coords.shift()`
    const branchid = this.vtkDataSet.pointData.BranchId.data;
    const kde = this.vtkDataSet.pointData.KDE.data;
    const kde_i0 = this.vtkDataSet.pointData.KDE_ByType_I0.data;
    const kde_i1 = this.vtkDataSet.pointData.KDE_ByType_I1.data;

    // change the y coordinates, because ttk and svg use a different coordinate system
    const _y = coords.filter((d, i) => (i % 3) === 1);
    const _y_max = Math.max(..._y);
    const _y_min = Math.min(..._y);

    // also change the x coordinates, so our bounding box starts at (0,0). This helps for e.g. the coordinate axis rendering
    const _x = coords.filter((d, i) => (i % 3) === 0);
    const _x_min = Math.min(..._x);

    while(coords.length > 0) {
      this.points.push(new Point(
          (coords.shift() - _x_min) * this.scale,
          ((Math.abs(_y_max - _y_min)) - (coords.shift() - _y_min)) * this.scale,
          coords.shift() * this.scale)
      );
    }

    const nCompI0 = this.vtkDataSet.pointData.KDE_ByType_I0.nComponents;
    const nCompI1 = this.vtkDataSet.pointData.KDE_ByType_I1.nComponents;

    for(let i = 0; i < this.points.length; i++) {
      this.points[i].setBranchId(branchid[i]);
      this.points[i].setKDE(kde[i]);

      this.points[i].setKDE_I0(kde_i0.slice(i * nCompI0, (i + 1) * nCompI0));
      this.points[i].setKDE_I1(kde_i1.slice(i * nCompI1, (i + 1) * nCompI1));
    }

    // cell information
    this.connectivityArray = this.vtkDataSet.cells.connectivityArray.data;
    const offsetsArray = this.vtkDataSet.cells.offsetsArray.data;

    // sanity check that we only work with lines, and that the data for
    // the lines is 'nicely' arranged in the connectivityArray.
    //
    // If there is connectivity information for other geometric primitives,
    // this has to be implemented separately.
    for (let i = 0; i < offsetsArray.length; i++) {
      if (BigInt(i*2) !== offsetsArray[i]) {
        console.error("there were non-line segments in the data");
        console.error("alternatively, the data was not 'nicely' arranged (i.e. always i*2)");
        return;
      }
    }

    // for donut chart, we have to find out at which points we want to draw the donuts.
    // the locations are:
    //    - at the maximum of each branch
    //    - if two branches connect with each other, at the lower point we'd want a donut
    const unique_branchIds = [...new Set(this.points.map(p => p.branchId))];
    for (let i = 0; i < unique_branchIds.length; i++) {
      let branch = this.points.filter(p => p.branchId === unique_branchIds[i]);
      // point with lowest y-value is the maximum, i.e. the "highest" point in the view of that branch
      const max = Math.min(...branch.map(p => p.y));
      const pos = this.points.findIndex(p => p.branchId === unique_branchIds[i] && p.y === max);
      this.points[pos].drawDonut = true;
      this.points[pos].drawPoint = true;
    }

    for (let i = 0; i < this.connectivityArray.length; i+=2) {
      const p1 = this.points[this.connectivityArray[i]];
      const p2 = this.points[this.connectivityArray[i+1]];
      if (p1.branchId !== p2.branchId) {
        let idx = (p2.y > p1.y) ? 1 : 0;
        this.points[this.connectivityArray[i + idx]].drawDonut = true;
        this.points[this.connectivityArray[i + idx]].drawPoint = true;
      }
    }

    // at the absolute minimum (the root node of the tree), we also want to draw a point
    this.treeRoot = this.points[this.points.findIndex(p => p.y === Math.max(...this.points.map(p => p.y)))];
    this.treeRoot.drawPoint = true;

    // save the labels if the dataset has labels. Otherwise give default values
    if (this.vtkDataSet.fieldData.hasOwnProperty("CategoryDictionary")) {
      this.labels = this.vtkDataSet.fieldData.CategoryDictionary.data.map(s => s.replaceAll("\"", ""));
    } else {
      this.labels = Array.from(Array(kde_i0.length).keys());
    }
  }

  render(type, topN_map) {
    this.type = type;

    if (!this.vtkDataSet) {
      console.error("trying to render without dataset");
      return;
    }

    this.coordinate_system.remove();
    this.nodelayer.remove();
    this.coordinate_system = this.root.append("g");
    this.nodelayer = this.root.append("g");

    this.tree = new Tree(this.points, this.connectivityArray, this.streamgraph_options, this.donut_options, this.nodelayer);

    this.tree.findDonutData();
    this.tree.createHelperStructureForMap(topN_map);
    this.tree.createColorMapping();
    kdeRenderer.setColorMap(this.streamgraph_options.color_scheme, this.tree.color_mapping);
    kdeRenderer.setTree(this.tree);

    // draw the streamgraph
    if (type === 'streamgraph') {
      this.tree.fix_reorderKDE_I1();  // has to be called after this.tree.createColorMapping()
      this.tree.calculateLayoutStreamgraph();
      this.tree.moveToOrigin();
      this.tree.rescale(this.width, this.height, type);
      this.centerView();
      this.drawCoordinateSystem();
      this.tree.drawStreamGraph();

      this.tree.drawEdges();
      this.tree.drawPoints();
      this.tree.drawEdges(true);
    }

    // draw the donut plots
    if (type === 'donut') {
      this.tree.calculateLayoutDonut();
      this.tree.moveToOrigin();
      this.tree.rescale(this.width, this.height, type);
      this.centerView();
      this.drawCoordinateSystem();

      this.tree.drawEdges();
      this.tree.drawDonut();
      this.tree.drawPoints();
      this.tree.drawEdges(true);
    }

    // draw the pie plots
    if (type === 'pie') {
      this.tree.calculateLayoutDonut();
      this.tree.moveToOrigin();
      this.tree.rescale(this.width, this.height, type);
      this.centerView();
      this.drawCoordinateSystem();

      this.tree.drawEdges();
      this.tree.drawPie();
      this.tree.drawPoints(true);
      this.tree.drawEdges(true);
    }

    d3.selectAll(".vertices").on("click", e => {
      g_clicked_node = e.target;
      const id = parseInt(e.target.id.split("_")[1]);

      const selected_point = this.points[id];
      const selected_points = this.tree.findPointsOnClick(selected_point);

      this.tree.highlightEdges(selected_points['points']);

      kdeRenderer.computeSelection(selected_points['points'].map(p => p.branchId), selected_points['min_scalar_value']);
      kdeRenderer.update_render();
    });

    this.previous_id = null;
    d3.selectAll(".edge[id^='click_']").on("click", e => {
      const clicked_edge = e.target;
      const id = clicked_edge.id;
      if (id === this.previous_id) {
        // deselect edges and return
        d3.selectAll(`.edge-display`)
          .attr("stroke-opacity", 1)
          .attr("stroke-width", this.streamgraph_options.edgeWidth);

        this.previous_id = null;

        // reset map
        kdeRenderer.computeSelection([], 0);
        kdeRenderer.computeMaskNoSelection();
        kdeRenderer.update_render();
        return;
      }

      this.previous_id = id;
      const b_id = parseInt(id.split("_")[1]);
      const connecting_point_index = parseInt(id.split("_")[2]);

      const branch = this.tree.returnBranchWithBranchId(b_id);
      let point;
      if (connecting_point_index === 0) {
        // it was the top point
        point = branch.top;
      } else {
        // it was some connecting point
        const connecting_points = branch.branch_points_sorted.filter(p => p.is_connecting_point);
        point = connecting_points[connecting_point_index - 1];
      }
      d3.select(`#vertex_${this.points.indexOf(point)}`).node().dispatchEvent(new Event("click"));
    });

    // this is needed that after another call to `render()` the branches stay highlighted
    if (g_clicked_node !== null) {
      g_clicked_node.dispatchEvent(new Event("click"));
    }
  }

  resetLayoutingCoords() {
    this.points.forEach(p => p.x_layout = p.x);
    this.points.forEach(p => p.y_layout = p.y);
    this.streamgraph_options.padding_scaled = this.streamgraph_options.padding;
  }

  centerView() {
    // change x_layout and y_layout coordinates such that they are centered for this particular viewbox configuration
    const x_max = Math.max(...this.points.map(p => p.x_layout))
        + ((this.type === 'streamgraph') ? this.streamgraph_options.maxwidth_root : this.donut_options.outerRadius);
    const y_max = Math.max(...this.points.map(p => p.y_layout));

    if (x_max >= this.width - 0.1) {
      // this.tree.all_points.forEach(p => p.y_layout += (this.height - y_max) / 2)
      this.tree.all_points.forEach(p => p.y_layout *= (this.height / y_max));
    }

    if (y_max >= this.height - 0.1) {
      this.tree.branches.forEach(b => b.setXLayout(b.x_layout + (this.width - x_max) / 2));
    }

    this.svg
        .attr("viewBox", [
          -this.padding,
          -this.padding,
          this.width + 2 * this.padding,
          this.height + 2 * this.padding
          ]
        );
  }

  drawCoordinateSystem() {
    this.coordinate_system.append("text")
        .attr("x", -35)
        .attr("y", this.height / 2)
        .attr("font-size", 10)
        .attr("font-weight", "lighter")
        .attr("transform", `rotate(-90, ${-35}, ${this.height / 2})`)
        .text("Density");

    const y_max = Math.max(...this.points.map(p => p.y_layout));
    const y_min = Math.min(...this.points.map(p => p.y_layout));

    const _tmp = y_min / (y_max - y_min);
    this.yScale = d3.scaleLinear()
        .domain([-_tmp, 1 + _tmp])  // this ensures that the top of our graph is at 1 and the bottom at 0
        .range([this.height, 0]);
    this.yAxis = d3.axisRight(this.yScale)
        .ticks(10)
        .tickSize(this.width + this.padding);
    this.gY = this.coordinate_system.append("g")
        .call(this.yAxis)
        .call(g => g.select(".domain")
            .remove())
        .call(g => g.selectAll(".tick line")
            .attr("stroke-opacity", 0.25))
        .call(g => g.selectAll(".tick text")
            .attr("x", -30))
        .attr("font-weight", "lighter");

    this.svg.call(d3.zoom()
        // .extent([[0, 0], [500, 500]])
        // .scaleExtent([1, 8])
        .on("zoom", ({transform}) => {
          this.transform = transform;
          this.doTransform();
        })
    );
  }

  doTransform() {
    if (this.transform === null) {
      return;
    }

    this.nodelayer.attr("transform", this.transform);
    this.gY.call(this.yAxis.scale(this.transform.rescaleY(this.yScale)))
        .call(g => g.select(".domain")
            .remove())
        .call(g => g.selectAll(".tick line")
            .attr("stroke-opacity", 0.25))
        .call(g => g.selectAll(".tick text")
            .attr("x", -30))
        .attr("font-weight", "lighter");
  }
}

function getSortedIndices(arr) {
  // see https://stackoverflow.com/questions/3730510/javascript-sort-array-and-return-an-array-of-indices-that-indicates-the-positio
  return Array.from(Array(arr.length).keys())  // create array [0, ..., arr.length - 1]
      .sort((a, b) => arr[b] - arr[a]);  // sort the array according to the entries in arr
}

function sum(arr) {
  return arr.reduce((c, k) => c + k, 0);
}

class Tree {
  constructor(points, connectivity, streamgraph_options, donut_options, nodelayer) {
    this.all_points = points;
    this.connectivity = connectivity;
    this.streamgraph_options = streamgraph_options;
    this.donut_options = donut_options;
    this.nodelayer = nodelayer;

    this.no_of_categories = this.all_points[0].kde_i1.length;

    this.unique_branchIds = this.all_points
        .map(p => p.branchId)
        .filter((v, i, self) => self.indexOf(v) === i)
        .sort((a, b) => this.xCoordFromBranchId(a) - this.xCoordFromBranchId(b))  // sort by the x coordinate of the branches
        .reverse();

    this.treeRoot = this.all_points[this.all_points.findIndex(p => p.y === Math.max(...this.all_points.map(p => p.y)))];

    // We want each subtree to have the same order of categories (i.e. "colors") as
    // the main tree. The order is specified by the size at the root node
    this.color_order = getSortedIndices(this.treeRoot.kde_i1);

    // create a scale for mapping of the points, which is shared at each subbranch if "relative mapping" is chosen
    this.mapping = d3.scaleLinear()
        .domain([0, sum(this.treeRoot.kde_i1)])
        .range([0, this.streamgraph_options.maxwidth_root - this.streamgraph_options.padding_scaled]);

    this.branches = this.unique_branchIds
        .map((id, i) => new Branch(this, id, i === 0));

    // for the donut chart, we want to reorder the KDE at each point, so we can draw the donuts more easily.
    this.all_points.forEach(p => p.reorderKDE_I1(this.color_order, this.streamgraph_options.topN));

    this.lowest_connecting_point = this.all_points.filter(p => p.drawDonut).sort((a, b) => b.y - a.y)[0];
  }

  fix_reorderKDE_I1() {
    // for the streamgraph, we want to reorder the KDE at each point ACCORDING TO THE ORDER AT THE BOTTOM NODE OF THE
    // CORRESPONDING BRANCH!!!
    // otherwise, this would introduce bugs...
    //
    // Note that this function has to be called after the createColorMapping() because this code is shitty
    this.branches.forEach(b => b.reorderKDE_I1_streamgraph(this.color_order, this.streamgraph_options.topN));
  }

  calculateLayoutStreamgraph() {
    if (this.streamgraph_options.use_relative_sizes) {
      this.branches.forEach(b => {
        if (!b.is_root_branch) {
          const space_needed = this.streamgraph_options.maxwidth_root + this.streamgraph_options.padding_scaled;
          const new_x = this.findNeighborBranch(b).x_layout - space_needed;
          b.setXLayout(new_x);
        }
      });
    } else {
      this.branches.forEach(b => {
        if (!b.is_root_branch) {
          const space_needed = this.mapping(b.bottom.kde_i1_sorted_and_ordered_cumsum[this.no_of_categories - 1]) + this.streamgraph_options.padding_scaled;
          const new_x = this.findNeighborBranch(b).x_layout - space_needed;
          b.setXLayout(new_x);
        }
      });
    }
  }

  calculateLayoutDonut() {
    this.branches.forEach(b => {
      if (!b.is_root_branch) {
        const space_needed = this.donut_options.outerRadius * 2 + this.donut_options.padding;
        const new_x = this.findNeighborBranch(b).x_layout - space_needed;
        b.setXLayout(new_x);
      }
    });
  }

  createHelperStructureForMap(n=0) {
    this.helperStructure = {};
    this.branches.forEach(b => {
      this.helperStructure[b.branchId] = new Map();

      let idx = b.top.donut_data_from_point.kde_i1_sorted_indices[n];
      this.helperStructure[b.branchId].set(b.top.kde, idx);
      for (const p of b.branch_points_sorted) {
        if (p.drawDonut) {
          if (p.donut_data_from_point.kde_i1_sorted_indices[n] !== idx) {
            idx = p.donut_data_from_point.kde_i1_sorted_indices[n];
            this.helperStructure[b.branchId].set(p.kde, idx);
          }
        }
      }
    });
  }

  getColorIDbyBranchIdAndScalar(branchID, scalar) {
    if (scalar <= this.lowest_connecting_point.kde) {
      return 255;
    }

    const map = this.helperStructure[branchID];
    if (map.size === 1) {
      return map.values().next().value;
    } else {
      let __ret = map.values().next().value;
      for (const [k, idx] of map) {
        if (k <= scalar) {
          return __ret;
        }
        __ret = idx;
      }
      return __ret;
    }
  }

  moveToOrigin() {
    const _x_min = Math.min(...this.all_points.map(p => p.x_layout));
    this.branches.forEach(b => b.setXLayout(b.x_layout - _x_min));

    const _y_min = Math.min(...this.all_points.map(p => p.y_layout));
    this.all_points.forEach(p => p.y_layout -= _y_min);
  }

  rescale(width, height, type) {
    const _x_max = Math.max(...this.all_points.map(p => p.x_layout))
        + ((type === 'streamgraph') ? this.streamgraph_options.maxwidth_root : this.donut_options.outerRadius);
    const _y_max = Math.max(...this.all_points.map(p => p.y_layout));

    // calculate the two possible scaling factors
    let f1 = width / _x_max;
    let f2 = height / _y_max;

    // check which scaling factors keep us in the bounds of width * height
    if (f1 * _y_max > height)
      f1 = 0;

    if (f2 * _x_max > width)
      f2 = 0;

    if (f1 === 0 && f2 === 0) {
      console.error("something went wrong trying to scale the values. This should never happen I think...")
      return;
    }

    let scaling_factor = 0;
    // check with which scaling factor we would get the largest bounding box
    if (width * _y_max * f1 > height * _x_max * f2) {
      scaling_factor = f1;
    } else {
      scaling_factor = f2;
    }

    if (scaling_factor === 0) {
      console.error("something went wrong trying to scale the values. This should never happen I think...")
      return;
    }

    // actually do the rescaling
    this.branches.forEach(b => b.setXLayout(b.x_layout * scaling_factor));
    this.all_points.forEach(p => p.y_layout *= scaling_factor);

    // we also have to apply the scaling to the mapping!
    if (type === "streamgraph") {
      this.streamgraph_options.maxwidth_root *= scaling_factor;
      this.streamgraph_options.padding_scaled *= scaling_factor;
      this.mapping.range([0, this.streamgraph_options.maxwidth_root - this.streamgraph_options.padding_scaled]);
    }
    if (type === "donut") {
      this.donut_options.padding *= scaling_factor;
      this.donut_options.innerRadius *= scaling_factor;
      this.donut_options.outerRadius *= scaling_factor;
    }
  }

  findDonutData() {
    this.branches.forEach(b => {
      b.branch_points_sorted.filter(p => p.drawDonut).forEach(p => {
        // find the point whose data we should display
        let point_of_interest = null;
        const connecting_points = b.branch_points_sorted
          .slice(b.branch_points_sorted.indexOf(p) + 1)
          .filter(p => p.is_connecting_point);
        if (connecting_points.length === 0) {
          // there are no more connections on this branch. Show the values of the bottom node
          point_of_interest = b.bottom;
        } else {
          // there is a connection on this branch. Show the values from the point before the connection
          point_of_interest = connecting_points[0];
        }

        if (point_of_interest === null) {
          console.error("Something went wrong when trying to find the donut data...")
          return;
        }

        p.donut_data_from_point = point_of_interest;
      });
    });
  }

  findPointsOnClick(selected_point) {
    // recursively find the points that are above the selected_point (also go through all of the subtrees that are above)
    // or below the selected point, up to a connecting point or the bottom, whatever comes first.

    let res = {
      'points': [],
      'min_scalar_value': Infinity,
    };
    const branch = this.returnBranchWithBranchId(selected_point.branchId);

    if (selected_point === branch.top || selected_point !== branch.bottom) {
      // points going down the branch
      let p = branch.branch_points_sorted.slice(branch.branch_points_sorted.indexOf(selected_point));

      res['points'] = res['points'].concat([selected_point]);
      res['min_scalar_value'] = Math.min(res['min_scalar_value'], selected_point.kde);

      for (let i = 1; i < p.length; i++) {
        if (p[i].is_connecting_point) {
          break;
        } else {
          res['points'] = res['points'].concat([p[i]]);
          res['min_scalar_value'] = Math.min(res['min_scalar_value'], p[i].kde);
        }

        if (p[i] === branch.bottom) {
          // scalar value must be taken from the other branch, because otherwise not everything is covered
          res['min_scalar_value'] = Math.min(res['min_scalar_value'], branch.connecting_point.kde);
          break;
        }
      }
    }

    if (selected_point === branch.bottom || selected_point !== branch.top) {
      // points going up the branch
      let p = branch.branch_points_sorted.slice(0, branch.branch_points_sorted.indexOf(selected_point) + 1);
      res['points'] = res['points'].concat(p);
      res['min_scalar_value'] = Math.min(res['min_scalar_value'], ...p.map(_p => _p.kde));
      p.filter(point => point.is_connecting_point).forEach(point => {
        const __tmp = this.findPointsOnClick(this.returnBranchWithBranchId(point.branchId_of_connection).bottom);
        res['points'] = res['points'].concat(__tmp['points']);
        res['min_scalar_value'] = Math.min(res['min_scalar_value'], __tmp['min_scalar_value']);
      });
    }

    return res;
  }

  returnBranchWithBranchId(branchId) {
    return this.branches.filter(b => b.branchId === branchId)[0];
  }

  getLabels() {
    let res = {};

    this.all_points.filter(p => p.drawDonut).forEach(p => {
      for (let i = 0; i < this.donut_options.topN; i++) {
        res[p.donut_data_from_point.kde_i1_sorted_indices[i]]
          = this.donut_options.color_scheme[this.color_mapping[p.donut_data_from_point.kde_i1_sorted_indices[i]]];
      }
    });

    return res;
  }

  createColorMapping() {
    let i = 1

    // do this just to find out the i
    for (i; i < this.no_of_categories; i++) {
      let res = new Set();

      this.all_points.filter(p => p.drawDonut).forEach(p => {
        p.donut_data_from_point.kde_i1_sorted_indices.slice(0, i).map(j => res.add(j));
      });

      if (res.size > this.streamgraph_options.color_scheme.length) {
        i--;
        break;
      }
    }

    // here do the actual calculation
    let res = new Set();
    this.all_points.filter(p => p.drawDonut).forEach(p => {
      p.donut_data_from_point.kde_i1_sorted_indices.slice(0, i).map(j => res.add(j));
    });
    let indices = [...res];

    this.color_mapping = Array.from(Array(this.no_of_categories).keys()).map(i => i % this.streamgraph_options.color_scheme.length);
    indices.forEach((idx, i) => this.color_mapping[idx] = i);
  }

  drawStreamGraph() {
    this.branches.forEach(b => b.drawStreamGraph());
  }

  drawDonut() {
    this.branches.forEach(b => b.drawDonut());
  }

  drawPie() {
    this.branches.forEach(b => b.drawDonut(true));
  }

  drawEdges(draw_click_helpers=false) {
    this.branches.forEach(b => {
      let path = d3.path();

      let y_start = b.top.y_layout;
      let y_end = b.connecting_point.y_layout;
      let idx = 0;
      let i = 0;
      let connecting_points = b.branch_points_sorted.filter(p => p.is_connecting_point);
      for (i = 0; i < connecting_points.length; i++) {
        let subbranch = b.branch_points_sorted.slice(idx, b.branch_points_sorted.indexOf(connecting_points[i]) + 1);
        idx = b.branch_points_sorted.indexOf(connecting_points[i]);
        y_start = subbranch[0].y_layout;
        y_end = subbranch[subbranch.length - 1].y_layout;
        path.moveTo(b.x_layout, y_start);
        path.lineTo(b.x_layout, y_end);

        if (draw_click_helpers) {
          this.nodelayer.append("path")
            .attr("d", path)
            .attr("class", "edge")
            .attr("id", `click_${b.branchId}_${i}`)
            .attr("fill", "none")
            .attr("stroke", "black")
            .attr("stroke-opacity", 0.0)
            .attr("stroke-width", 10);
        } else {
          this.nodelayer.append("path")
            .attr("d", path)
            .attr("class", "edge edge-display")
            .attr("id", `${b.branchId}_${i}`)
            .attr("fill", "none")
            .attr("stroke", "black")
            .attr("stroke-width", this.streamgraph_options.edgeWidth);
        }

        path = d3.path();
        y_start = y_end;
      }
      path.moveTo(b.x_layout, y_start);
      path.lineTo(b.x_layout, b.connecting_point.y_layout);  // normally to b.bottom.y_layout, but we want to trick the layout drawing a bit
      path.lineTo(b.connecting_point.x_layout, b.connecting_point.y_layout);

      if (draw_click_helpers) {
        this.nodelayer.append("path")
          .attr("d", path)
          .attr("class", "edge")
          .attr("id", `click_${b.branchId}_${i}`)
          .attr("fill", "none")
          .attr("stroke", "black")
          .attr("stroke-opacity", 0.0)
          .attr("stroke-width", 10);
      } else {
        this.nodelayer.append("path")
          .attr("d", path)
          .attr("class", "edge edge-display")
          .attr("id", `${b.branchId}_${i}`)
          .attr("fill", "none")
          .attr("stroke", "black")
          .attr("stroke-width", this.streamgraph_options.edgeWidth);
      }
    });
  }

  highlightEdges(points) {
    d3.selectAll(`.edge-display`)
      .attr("stroke-opacity", 1)
      .attr("stroke-width", this.streamgraph_options.edgeWidth);

    let b_ids = points.map(p => p.branchId);
    b_ids = [...new Set(b_ids)];  // make unique
    b_ids.forEach(b_id => {
      // foreach branch_id check what the connecting point furthest down is in the list of points
      let selected_points_bid = points.filter(p => p.branchId === b_id);
      const all_points_bid = this.returnBranchWithBranchId(b_id).branch_points_sorted;

      if (selected_points_bid.length >= all_points_bid.length) {
        // easy case, the whole branch must be highlighted
        d3.selectAll(`.edge-display[id^='${b_id}_']`)
          .attr("stroke-width", 5)
          .attr("stroke-opacity", 0.7);
      } else {
        // harder case, find out which segments must be highlighted

        // find point with largest y_layout
        selected_points_bid = selected_points_bid.sort((a, b) => a.y - b.y);
        const __tmp = selected_points_bid[selected_points_bid.length - 1];
        const next_connecting_point = all_points_bid[all_points_bid.indexOf(__tmp) + 1];
        const connecting_points = all_points_bid.filter(p => p.is_connecting_point);
        for (let i = 0; i < connecting_points.indexOf(next_connecting_point) + 1; i++) {
          d3.selectAll(`.edge[id='${b_id}_${i}']`)
            .attr("stroke-width", 5)
            .attr("stroke-opacity", 0.7);
        }
      }
    });
  }

  drawPoints(hide=false) {
    this.all_points.filter(p => p.drawPoint).forEach(p =>
      this.nodelayer.append("circle")
          .attr("cx", p.x_layout)
          .attr("cy", p.y_layout)
          .attr("r", (hide) ? 0 : 4)
          .attr("fill", "black")
          .attr("class", "vertices")
          .attr("id", `vertex_${this.all_points.indexOf(p)}`)
    );
  }

  findNeighborBranch(branch) {
    // this function has to be this complicated because it can happen that
    // this.branches[this.branches.indexOf(branch) - 1] has the same x value
    // as the current branch -- thus resulting in a waste of space!
    const x = branch.x;
    let i = 1;
    let ret = this.branches[this.branches.indexOf(branch) - i];
    while (x === ret.x) {
      i += 1;
      ret = this.branches[this.branches.indexOf(branch) - i];
    }
    return ret;
  }

  xCoordFromBranchId(bId) {
    return this.all_points.filter(p => p.branchId === bId).map(p => p.x)[0];
  }

  xLayoutCoordFromBranchId(bId) {
    return this.all_points.filter(p => p.branchId === bId).map(p => p.x_layout)[0];
  }
}

class Branch {
  constructor(tree, branchId, is_root_branch) {
    this.tree = tree;
    this.branchId = branchId;
    this.is_root_branch = is_root_branch;  // boolean

    this.x = this.tree.xCoordFromBranchId(this.branchId);
    this.x_layout = this.tree.xLayoutCoordFromBranchId(this.branchId);

    this.branch_points_sorted = this.tree.all_points
        .filter(p => p.branchId === this.branchId)
        .sort((a, b) => a.y - b.y);

    this.top = this.branch_points_sorted[0];
    this.bottom = this.branch_points_sorted[this.branch_points_sorted.length - 1];

    if (!this.is_root_branch) {
      const __tmp = this.tree.all_points.indexOf(this.bottom);
      const __id = this.tree.connectivity.filter((c, i) => i % 2 === 0).indexOf(BigInt(__tmp));
      // the point to which the branch is connected to
      this.connecting_point = this.tree.all_points[this.tree.connectivity[__id * 2 + 1]];
      this.connecting_point.is_connecting_point = true;
      this.connecting_point.branchId_of_connection = this.branchId;
    } else {
      this.connecting_point = this.bottom;
    }
  }

  setXLayout(x_layout) {
    this.x_layout = x_layout;
    this.branch_points_sorted.forEach(p => p.setXLayout(x_layout));
  }

  reorderKDE_I1_streamgraph(order, topN) {
    const kde_i1_sorted_indices = getSortedIndices(this.bottom.kde_i1);
    const kde_i1_sorted = kde_i1_sorted_indices.map(i => this.bottom.kde_i1[i]);

    this.branch_points_sorted.forEach(p => p.reorderKDE_I1(order, topN, kde_i1_sorted_indices, kde_i1_sorted));
  }

  drawStreamGraph() {
    const mapping = (!this.tree.streamgraph_options.use_relative_sizes)
      ? this.tree.mapping
      : d3.scaleLinear()
          .domain([0, sum(this.bottom.kde_i1)])
          .range([0, this.tree.streamgraph_options.maxwidth_root - this.tree.streamgraph_options.padding_scaled]);
    for (let i = this.tree.no_of_categories - 1; i >= 0; i--) {
      let path = d3.path();
      path.moveTo(
          this.top.x_layout + this.tree.streamgraph_options.edgeWidth / 2,
          this.top.y_layout
      );
      this.branch_points_sorted.forEach(point => {
        path.lineTo(
            point.x_layout + mapping(point.kde_i1_sorted_and_ordered_cumsum[i])
              + this.tree.streamgraph_options.edgeWidth / 2,
            point.y_layout
        );
      });
      const __lastPoint = this.branch_points_sorted[this.branch_points_sorted.length - 1];
      path.lineTo(
          __lastPoint.x_layout
            + mapping(__lastPoint.kde_i1_sorted_and_ordered_cumsum[i])
            + this.tree.streamgraph_options.edgeWidth / 2,
          this.connecting_point.y_layout
      );
      path.lineTo(
          this.bottom.x_layout + this.tree.streamgraph_options.edgeWidth / 2,
          this.connecting_point.y_layout
      );
      path.closePath();

      let color = this.tree.streamgraph_options.color_of_ignored;
      if (i < this.tree.streamgraph_options.topN) {
        // color = this.tree.streamgraph_options.color_scheme[this.tree.color_order.indexOf(this.bottom.topN_indices_ordered[i]) % 12];
        color = this.tree.streamgraph_options.color_scheme[this.tree.color_mapping[this.bottom.topN_indices_ordered[i]]];
      }

      this.tree.nodelayer.append("path")
          .attr("d", path)
          .attr("fill", color);
    }
  }

  drawDonut(draw_pie=false) {
    // adapted from https://www.geeksforgeeks.org/d3-js-pie-function/
    this.branch_points_sorted.filter(p => p.drawDonut).forEach(p => {
      let g = this.tree.nodelayer.append("g")
          .attr("transform", `translate(${p.x_layout}, ${p.y_layout})`);

      const pie = d3.pie();
      let arc;
      if (draw_pie) {
        arc = d3.arc()
          .innerRadius(0)
          .outerRadius(this.tree.donut_options.outerRadius);
      } else {
        arc = d3.arc()
          .innerRadius(this.tree.donut_options.innerRadius)
          .outerRadius(this.tree.donut_options.outerRadius);
      }

      let pie_data = p.donut_data_from_point.kde_i1_sorted.slice(0, this.tree.donut_options.topN);
      pie_data.push(sum(p.donut_data_from_point.kde_i1_sorted.slice(this.tree.donut_options.topN)));

      const arcs = g.selectAll("arcs")
          .data(pie(pie_data))
          .enter()
          .append("g");

      arcs.append("path")
          .attr("fill", (data, i) => {
            if (i !== this.tree.donut_options.topN) {
              return this.tree.donut_options.color_scheme[this.tree.color_mapping[p.donut_data_from_point.kde_i1_sorted_indices[i]]];
              // return this.tree.donut_options.color_scheme[this.tree.color_order.indexOf(p.donut_data_from_point.kde_i1_sorted_indices[i]) % 12];
            } else {
              return this.tree.donut_options.color_of_ignored;
            }
          })
          .attr("d", arc)
          .attr("stroke", "black")
          .style("stroke-width", 1);
    });
  }
}

class Point {
  constructor(x, y, z) {
    this.x = x;
    this.x_layout = x;
    this.y = y;
    this.y_layout = y;
    this.z = z;

    this.drawDonut = false;
    this.is_connecting_point = false;
    this.branchId_of_connection = -1;
    this.donut_data_from_point = null;
  }

  reorderKDE_I1(order, topN, kde_i1_sorted_indices = null, kde_i1_sorted = null) {
    // sort kde_i1 by size
    this.kde_i1_sorted_indices = (kde_i1_sorted_indices === null) ? getSortedIndices(this.kde_i1) : kde_i1_sorted_indices;
    this.kde_i1_sorted = (kde_i1_sorted === null) ? this.kde_i1_sorted_indices.map(i => this.kde_i1[i]) : kde_i1_sorted;

    // get the indices of the largest topN values
    this.topN_indices = this.kde_i1_sorted_indices.slice(0, topN);
    this.other_indices = this.kde_i1_sorted_indices.slice(topN);
    // reorder those indices according to the `order` array
    this.topN_indices_ordered = this.topN_indices.sort((i, j) => order.indexOf(i) - order.indexOf(j));

    this.kde_i1_sorted_and_ordered = this.topN_indices_ordered.map(i => this.kde_i1[i]);
    this.kde_i1_sorted_and_ordered = this.kde_i1_sorted_and_ordered.concat(this.other_indices.map(i => this.kde_i1[i]));

    this.kde_i1_sorted_and_ordered_cumsum = d3.cumsum(this.kde_i1_sorted_and_ordered);
  }

  setBranchId(id) {
    this.branchId = id;
  }

  setKDE(kde) {
    this.kde = kde;
  }

  setKDE_I0(kde) {
    this.kde_i0 = kde;
  }

  setKDE_I1(kde) {
    this.kde_i1 = kde;
  }

  setXLayout(x) {
    this.x_layout = x;
  }

  setYLayout(y) {
    this.y_layout = y;
  }
}
