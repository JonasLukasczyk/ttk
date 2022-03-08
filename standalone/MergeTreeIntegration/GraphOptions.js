class GraphOptions {
  constructor() {
  }

  setDonutGraph(donut) {
    this.donutGraph = donut;
  }

  setPieGraph(pie) {
    this.pieGraph = pie;
  }

  setStreamGraphAbs(stream) {
    this.streamGraphAbs = stream;
  }

  setStreamGraphRel(stream) {
    this.streamGraphRel = stream;
  }

  render() {
    d3.select("#color_scheme").on("input", e => {
      this.streamGraphAbs.changeColorScheme(e.target.value);
      this.streamGraphAbs.render("streamgraph");
      this.streamGraphAbs.doTransform();

      this.streamGraphRel.changeColorScheme(e.target.value);
      this.streamGraphRel.render("streamgraph");
      this.streamGraphRel.doTransform();

      this.donutGraph.changeColorScheme(e.target.value);
      this.donutGraph.render("donut");
      this.donutGraph.doTransform();

      this.pieGraph.changeColorScheme(e.target.value);
      this.pieGraph.render("pie");
      this.pieGraph.doTransform();

      // so the color legend will be updated as well
      d3.select("#topN").node().dispatchEvent(new Event("input"));
    });

    d3.select("#topN").on("input", e => {
      const topN = e.target.value;
      d3.select("#label_topN").text(`${topN}`);

      this.streamGraphAbs.streamgraph_options.topN = parseInt(topN);
      this.streamGraphAbs.render("streamgraph");
      this.streamGraphAbs.doTransform();

      this.streamGraphRel.streamgraph_options.topN = parseInt(topN);
      this.streamGraphRel.render("streamgraph");
      this.streamGraphRel.doTransform();

      this.donutGraph.donut_options.topN = parseInt(topN);
      this.donutGraph.render("donut");
      this.donutGraph.doTransform();

      this.pieGraph.donut_options.topN = parseInt(topN);
      this.pieGraph.render("pie");
      this.pieGraph.doTransform();

      // render the color legend
      let labels = this.donutGraph.tree.getLabels();

      // transform the labels to an array that is sorted by color_order
      let labels_array = [];
      for (let key in labels) {
        labels_array.push([key, labels[key]])
      }
      labels_array = labels_array.sort((a, b) =>
        this.donutGraph.tree.color_order.indexOf(parseInt(a[0])) - this.donutGraph.tree.color_order.indexOf(parseInt(b[0])));

      d3.selectAll("#color_legend .entry").remove();

      labels_array.forEach(d => {
        let entry = d3.select("#color_legend")
          .append("div")
          .attr("class", "entry row")

        entry.append("div")
          .attr("style", `background-color: ${d[1]}; border: solid #aaaaaa`)
          .attr("class", "col-1 rounded-circle");

        entry.append("div")
          .attr("class", "col-10")
          .text(this.donutGraph.labels[d[0]]);
      });
    });

    d3.select("#topN_map").on("input", e => {
      const topN_map = parseInt(e.target.value);
      d3.select("#label_topN_map").text(`${topN_map}`);

      this.streamGraphAbs.render("streamgraph", topN_map);
      this.streamGraphRel.render("streamgraph", topN_map);
      this.donutGraph.render("donut", topN_map);
      this.pieGraph.render("pie", topN_map);
      kdeRenderer.computeMaskNoSelection();
      kdeRenderer.update_render();
    });

    d3.select("#maxwidthRoot").on("input", e => {
      const maxwidthRoot = e.target.value;
      d3.select("#label_maxwidthRoot").text(`${maxwidthRoot}`);

      this.streamGraphAbs.streamgraph_options.maxwidth_root = parseInt(maxwidthRoot);
      this.streamGraphAbs.resetLayoutingCoords();
      this.streamGraphAbs.render("streamgraph");
      this.streamGraphAbs.doTransform();

      this.streamGraphRel.streamgraph_options.maxwidth_root = parseInt(maxwidthRoot);
      this.streamGraphRel.resetLayoutingCoords();
      this.streamGraphRel.render("streamgraph");
      this.streamGraphRel.doTransform();
    });
    //
    // d3.select("#absolute_width").on("change", e => {
    //   const absolute_width = e.target.checked;
    //
    //   this.streamGraph.streamgraph_options.use_relative_sizes = !absolute_width;
    //   this.streamGraph.streamgraph_options.maxwidth_root = parseInt(d3.select("#maxwidthRoot").node().value);
    //   this.streamGraph.resetLayoutingCoords();
    //   this.streamGraph.render("streamgraph");
    //   this.streamGraph.doTransform();
    // });

    d3.select("#contour_count").on("input", e => {
      kdeRenderer.update_nContours(parseInt(e.target.value));
      d3.select("#label_contour_count").text(`${e.target.value}`);
    });

    d3.select("#hatching_count").on("input", e => {
      kdeRenderer.update_nHatching(parseInt(e.target.value));
      d3.select("#label_hatching_count").text(`${e.target.value}`);
    });

    d3.select("#hatching_width").on("input", e => {
      kdeRenderer.update_HatchingWidth(parseInt(e.target.value));
      d3.select("#label_hatching_width").text(`${e.target.value}`);
    });

    d3.select("#graph_type").on("input", e => {
      if (e.target.value === "streamgraph_abs") {
        d3.select("#treeContainerDonut").attr("style", "display: none");
        d3.select("#treeContainerPie").attr("style", "display: none");
        d3.select("#treeContainerStreamgraphAbs").attr("style", "");
        d3.select("#treeContainerStreamgraphRel").attr("style", "display: none");
      }

      if (e.target.value === "streamgraph_rel") {
        d3.select("#treeContainerDonut").attr("style", "display: none");
        d3.select("#treeContainerPie").attr("style", "display: none");
        d3.select("#treeContainerStreamgraphAbs").attr("style", "display: none");
        d3.select("#treeContainerStreamgraphRel").attr("style", "");
      }

      if (e.target.value === "donut") {
        d3.select("#treeContainerDonut").attr("style", "");
        d3.select("#treeContainerPie").attr("style", "display: none");
        d3.select("#treeContainerStreamgraphAbs").attr("style", "display: none");
        d3.select("#treeContainerStreamgraphRel").attr("style", "display: none");
      }

      if (e.target.value === "pie") {
        d3.select("#treeContainerDonut").attr("style", "display: none");
        d3.select("#treeContainerPie").attr("style", "");
        d3.select("#treeContainerStreamgraphAbs").attr("style", "display: none");
        d3.select("#treeContainerStreamgraphRel").attr("style", "display: none");
      }
    });

    d3.select("#more_toggle").on("click", e => {
      if (d3.selectAll(".more").attr("style") === "display: none") {
        d3.selectAll(".more").attr("style", "display: inherit");
      } else {
        d3.selectAll(".more").attr("style", "display: none");
      }
    });

    // so the values are displayed when the side is loaded
    d3.select("#maxwidthRoot").node().dispatchEvent(new Event("input"));
    // d3.select("#absolute_width").node().dispatchEvent(new Event("change"));
    d3.select("#topN").node().dispatchEvent(new Event("input"));
    d3.select("#color_scheme").node().dispatchEvent(new Event("input"));
    d3.select("#contour_count").node().dispatchEvent(new Event("input"));
    d3.select("#hatching_count").node().dispatchEvent(new Event("input"));
    d3.select("#hatching_width").node().dispatchEvent(new Event("input"));
    d3.select("#graph_type").node().dispatchEvent(new Event("input"));
    d3.select("#topN_map").node().dispatchEvent(new Event("input"));
  }
}