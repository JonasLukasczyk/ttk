function drawLegend(data, min_persistence_pairs, max_persistence_pairs, tag=0) {
    // tag: default 0, 1 => use current medium for top and right bar
    function removeLastEqual(bins) {
        if ( bins.length > 0 ) {
            let lastEle = bins.slice(-1)[0] ;
            let lastSecondEle = bins.slice(-2, -1)[0] ;
            if ( lastEle.x0 === lastEle.x1 ) {
                for (let i = 0; i < lastEle.length ; i ++) {
                    lastSecondEle.push(lastEle[i]) ;
                }
                bins = bins.slice(0, -1) ;
            }
        }
        return bins ;
    }

    function getMediumFromHistogramBins(bins) {
        var values = [] ;

        for ( var i = 0; i < bins.length; i ++ ) {
            if ( bins[i].length > 0 ) {
                values.push(bins[i].length) ;
            }
        }

        values.sort(function(a, b) {return a - b ; }) ;
        return values.length > 0? values[ Math.floor(values.length / 2) ]: 0 ;
    }


    // reduced_data: [(reliability, # of bins), ...]
    let x_0 = getFloatValue("#histogram_viz_legend", "data-x_0"),
        x_1 = getFloatValue("#histogram_viz_legend", "data-x_1"),
        width_partition = 20,
        height_partition = 10;
    var colorBar = "gray" ;

    let containerWidth = 241;
    let containerHeight = 161;
    let margin = {
        top: 40,
        right: 40,
        bottom: 20,
        left: 40
    };

    // define the container and svg
    d3.select("#histogram_viz_legend *").remove();
    let container = d3.select("#histogram_viz_legend")
        .append("svg")
        .attr("width", containerWidth)
        .attr("height", containerHeight);

    // http://bl.ocks.org/nnattawat/8916402
    let width = containerWidth - margin.left - margin.right,
        height = containerHeight - margin.top - margin.bottom;

    let xLine = d3.scaleLinear().range([0, width]).domain([0, 1]);

    // ============================= draw the top bar chart
    function drawLegendTop() {
        d3.select("#container-legend-top").remove() ;
        let histogram = d3.histogram()
            .value(function(d) { return d[0]; })   // I need to give the vector of value
            .domain(xLine.domain())  // then the domain of the graphic
            .thresholds(xLine.ticks(width_partition)); // then the numbers of bins

        var lHeight = margin.top - 5 ;
        let bins = histogram(data);
        bins = removeLastEqual(bins) ;
        let yLine = d3.scaleLinear().range([lHeight, 0]);
        // Y domain should be fixed
        var mediumTop = Window.PPI['persistence_pairs_range']['medium_top'] ;
        if ( tag === 1) {
            Window.PPI['persistence_pairs_range']['medium_top'] = getMediumFromHistogramBins(bins) ;
            mediumTop = Window.PPI['persistence_pairs_range']['medium_top'] ;
        }
        if ( mediumTop === -1 ) {
            mediumTop = getMediumFromHistogramBins(bins) ;
        }

        yLine.domain([0, mediumTop]);   // d3.hist has to be called before the Y axis obviously
        $("#legend-slider-top").slider("value", mediumTop) ;
        $("#legend-custom-handle-top").html("<span style='font-size:10px;'>"+mediumTop+"</span>") ;
        let lineSvg = container
            .append("g")
            .attr("id", "container-legend-top")
            .attr("transform",
                "translate(" + margin.left + ", 5)")
            .attr('overflow', 'hidden');

        // let dragTop = d3.drag()
        //     .on('start', topDragstarted)
        //     .on('drag', topDragged)
        //     .on('end', topDragended);

        // function topDragstarted() {
        //     d3.select(this).attr('class', 'active-d3-item-line2');
        // }

        // function topDragged(d) {
        //     d = yLine.invert(d3.event.y);
        //     if ( d <= 1) {
        //         return ;
        //     }
        //     if ( d3.event.y <= 1) {
        //         return ;
        //     }
        //     d3.select(this)
        //         .attr('y1', yLine(d))
        //         .attr('y2', yLine(d)) ;
        //     d3.select(this).attr("data-value", yLine(d)) ;
        // }

        // function topDragended() {
        //     d3.select(this).attr('class', 'inactive-d3-item-line2');
        //     d3.select("#top-legend-rect-cover").attr("height", parseFloat(d3.select(this).attr("data-value"))) ;
        // }

        lineSvg.append("g")
            .selectAll("rect")
            .data(bins)
            .enter()
            .append("rect")
            .attr("transform", function(d) { return "translate(" + xLine(d.x0) + "," + yLine(Math.min(d.length, mediumTop)) + ")"; })
            .attr("width", function(d) {
                return xLine(d.x1) - xLine(d.x0);
            })
            .attr("height", function(d) { return lHeight - yLine(Math.min(d.length, mediumTop)); })
            .style("fill", colorBar)
            .append("title")
            .text(function(d) { return "# is "+ d.length + ", range is (" + d.x0 + ", " + d.x1 +")"  });

        // lineSvg.append('rect')
        //     .attr("x", 0)
        //     .attr("id", "top-legend-rect-cover")
        //     .attr("y", 0)
        //     .attr("width", width)
        //     .attr("height", 0.4 * lHeight)
        //     .attr("fill", "white") ;

        // // Add a Line
        // lineSvg.append('line')
        //     .attr("x1", 0)
        //     .attr("y1", 0.4 * lHeight)
        //     .attr("x2", width)
        //     .attr("y2", 0.4 * lHeight)
        //     .attr("class", "inactive-d3-item-line2")
        //     .style("cursor", "pointer")
        //     .call(dragTop) ;
    }
    drawLegendTop() ;

    // ============================= draw the right bar chart
    let xLineRight = d3.scaleLinear().range([0, height]).domain([min_persistence_pairs, max_persistence_pairs]);
    function drawLegendRight() {
        d3.select("#container-legend-right").remove() ;
        let histogramRight = d3.histogram()
        .value(function(d) { return d[1]; })   // I need to give the vector of value
        .domain(xLineRight.domain())  // then the domain of the graphic
        .thresholds(xLineRight.ticks(height_partition)); // then the numbers of bins

        let binsRight = histogramRight(data);
        binsRight = removeLastEqual(binsRight) ;
        let yLineRight = d3.scaleLinear().range([margin.right - 5, 0]);
        var mediumRight = Window.PPI['persistence_pairs_range']['medium_right'] ;
        if ( tag === 1) {
            Window.PPI['persistence_pairs_range']['medium_right'] = getMediumFromHistogramBins(binsRight) ;
            mediumRight = Window.PPI['persistence_pairs_range']['medium_right'] ;
            tag = 0 ;
        }
        if ( mediumRight === -1 ) {
            mediumRight = getMediumFromHistogramBins(binsRight) ;
        }
        var v =  Window.PPI['histogram-width'] * Window.PPI['histogram-height'] + 1 - mediumRight ;
        $("#legend-slider-right").slider("value", v) ;
        $("#legend-custom-handle-right").html("<span style='font-size:10px;'>"+mediumRight+"</span>").attr("value", mediumRight) ;
        yLineRight.domain([0,  mediumRight]);   // d3.hist has to be called before the Y axis obviously
        rHeight = margin.right - 5 ;
        let lineSvgRight = container
            .append("g")
            .attr("id", "container-legend-right")
            .attr("transform",
                "translate(" + (margin.left + width + margin.right - 5) + ", "+ margin.top+") rotate(90)")
            .attr('overflow', 'hidden');

        // let dragRight = d3.drag()
        //     .on('start', rightDragstarted)
        //     .on('drag', rightDragged)
        //     .on('end', rightDragended);

        // function rightDragstarted() {
        //     d3.select(this).attr('class', 'active-d3-item-line2');
        // }

        // function rightDragged(d) {
        //     d = yLineRight.invert(d3.event.y);
        //     if ( d <= 1) {
        //         return ;
        //     }

        //     if ( d3.event.y <= 1) {
        //         return ;
        //     }
        //     d3.select(this)
        //         .attr('y1', yLineRight(d))
        //         .attr('y2', yLineRight(d)) ;
        //     d3.select(this).attr("data-value", yLineRight(d)) ;
        // }

        // function rightDragended() {
        //     d3.select(this).attr('class', 'inactive-d3-item-line2');
        //     d3.select("#right-legend-rect-cover").attr("height", parseFloat(d3.select(this).attr("data-value"))) ;
        // }

        binsRight = binsRight.reverse()
        var x2 = min_persistence_pairs ;
        for (var i = 0; i < binsRight.length; i ++) {
            binsRight[i].x2 = x2 ;
            x2 += (binsRight[i].x1 - binsRight[i].x0)
        }
        lineSvgRight.selectAll("rect")
            .data(binsRight)
            .enter()
            .append("rect")
            .attr("transform", function(d) { return "translate(" + xLineRight(d.x2) + "," + yLineRight(Math.min(mediumRight, d.length)) + ")"; })
            .attr("width", function(d) {
                return xLineRight(d.x1) - xLineRight(d.x0);
            })
            .attr("height", function(d) { return margin.right - 5 - yLineRight(Math.min(mediumRight, d.length)); })
            .style("fill", colorBar)
            .append("title")
            .text(function(d) { return "# is "+ d.length + ", range is (" + d.x0 + ", " + d.x1 +")"  });

        // lineSvgRight.append('rect')
        //     .attr("x", 0)
        //     .attr("id", "right-legend-rect-cover")
        //     .attr("y", 0)
        //     .attr("width", height)
        //     .attr("height", 0.4 * rHeight)
        //     .attr("fill", "white") ;

        // // Add a Line
        // lineSvgRight.append('line')
        //     .attr("x1", 0)
        //     .attr("y1", 0.4 * rHeight)
        //     .attr("x2", height)
        //     .attr("y2", 0.4 * rHeight)
        //     .attr("class", "inactive-d3-item-line2")
        //     .style("cursor", "pointer")
        //     .call(dragRight) ;
    }
    drawLegendRight() ;

    // ===================================================================================================================

    // updated notification
    $("#histogram-notification-placeholder-default").html("") ;
    $("#histogram-notification-placeholder-1").html($("#histogram-notification").attr("data-pattern-1").replace("{Left}", x_0.toFixed(2)).replace("{Right}", x_1.toFixed(2))) ;

    // draw the main body SVG
    let widthGroups = getArray(3);
    let heightGroups = getArray(5);
    let vData = getArray(15);

    let svg = container
        .append("g")
        .attr("transform",
            "translate(" + margin.left + ", " + margin.top + ")")
        .attr('overflow', 'hidden');

    // share the same x axis
    let x = d3.scaleBand()
        .range([0, width])
        .domain(widthGroups);

    svg.append("g")
        .attr('class', 'axis--legend--x')
        .attr("transform", "translate(0," + height + ")")
        .call(d3.axisBottom(x));

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(heightGroups);

    svg.append("g")
        .attr('class', 'axis--legend--y')
        .call(d3.axisLeft(y));

    svg.selectAll()
        .data(vData)
        .enter()
        .append("rect")
        .style("stroke-width", 0.1)
        .style("stroke", "black")
        .attr("x", function(d, i) {
            let total_width = x.bandwidth() * 3;
            if ( i % 3 === 0 ) {
                return  0;
            } else if ( i % 3 === 1 ) {
                return total_width * x_0  ;
            } else {
                return total_width * x_1  ;
            }
        })
        .attr("y", function(d, i) {
            return y.bandwidth() * ( 4 - Math.floor(i / 3) ) ;
        })
        .attr("width", function (d, i) {
            let total_width = x.bandwidth() * 3;
            if ( i % 3 === 0 ) {
                return total_width * x_0 ;
            } else if ( i % 3 === 1 ) {
                return total_width * ( x_1 - x_0 )  ;
            } else {
                return total_width * ( 1- x_1 ) ;
            }
        })
        .attr("height", y.bandwidth())
        .style("fill", function(d, i) {
            if ( i % 3 === 0) {
                return customColor(x_0 / 2, undefined, undefined, Math.floor(i / 3));
            } else if ( i % 3 === 1) {
                return customColor(x_0 + (x_1 - x_0) / 2, undefined, undefined, Math.floor(i / 3));
            } else {
                return customColor(x_1 + (1 - x_1) / 2, undefined, undefined, Math.floor(i / 3));
            }
        });
    removeNiceByKicks(".axis--legend--y", 0);
    removeNiceByKicks(".axis--legend--x", 0);

    svg.append("text")
        .attr("transform", "rotate(-90)")
        .attr("y", -20)
        .attr("x", 0 - (height / 2))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .style("font-size", "small")
        .text("#Bins") ;

    svg.append("text")
        .attr("y", height - 10)
        .attr("x", 0 - 10)
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .style("font-size", "small")
        .text(min_persistence_pairs + "-") ;

    svg.append("text")
        .attr("y", 0 - 4)
        .attr("x", 0 - 14)
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .style("font-size", "small")
        .text(max_persistence_pairs + "+") ;

    // Add x-axis title
    svg.append("text")
        .attr("y", (height ) )
        .attr("x", (width / 2))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .style("font-size", "small")
        // .text("Reliability");
        .text("Similiarity") ;

    // Add two vertical line for reliability
    lines = [
        {"x1": xLine(x_0), "x2": xLine(x_0)} ,
        {"x1": xLine(x_1), "x2": xLine(x_1)} ,
    ] ;

    let drag = d3.drag()
        .on('start', dragstarted)
        .on('drag', dragged)
        .on('end', dragended);

    svg.selectAll('line')
        .data(lines)
        .enter()
        .append('line')
        .attr("x1", function(d) {return d.x1;})
        .attr("y1", 0)
        .attr("x2", function(d) {return d.x2;})
        .attr("y2", height)
        .attr("class", "inactive-d3-item-line")
        .style("cursor", "pointer")
        .attr("data-id", "legend-lines")
        .attr("data-value", function(d, i) { if ( i === 0) { return x_0; } else { return x_1 ; } } )
        .call(drag) ;

    function dragstarted() {
        d3.select(this).attr('class', 'active-d3-item-line');
    }

    function dragged(d) {
        d = xLine.invert(d3.event.x);
        d3.select(this)
            .attr('x1', xLine(d))
            .attr('x2', xLine(d))
            .attr('data-value', d) ;
    }

    function dragended() {
        d3.select(this).attr('class', 'inactive-d3-item-line');
        let values = [] ;
        $("[data-id=legend-lines]").each(function() {
            values.push(parseFloat($(this).attr("data-value"))) ;
        } ) ;

        if (values[0] > values[1]) {
            [values[0], values[1]] = [values[1], values[0]] ;
        }

        $("#histogram_viz_legend").attr("data-x_0", values[0]).attr("data-x_1", values[1]) ;

        if (Window.PPI['histogram-mode'] == "multi") {
            $("#hist-threshold").change() ;
        } else {
            $("#hidden-sdm-draw-vertical-column-lonely").click() ;
            $("#hidden-sdm-draw-legend-lonely").click() ;
        }
    }

    $("#hidden-legend-redraw-top").unbind().click(function() {
        Window.PPI['persistence_pairs_range']['medium_top'] = parseInt($("#legend-custom-handle-top").attr("value")) ;
        drawLegendTop() ;
    }) ;

     $("#hidden-legend-redraw-right").unbind().click(function() {
        Window.PPI['persistence_pairs_range']['medium_right'] = parseInt($("#legend-custom-handle-right").attr("value")) ;
        drawLegendRight() ;

    }) ;
}