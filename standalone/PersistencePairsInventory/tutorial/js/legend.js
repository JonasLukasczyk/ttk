function drawLegend(data, min_persistence_pairs, max_persistence_pairs) {
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

    // reduced_data: [(reliability, # of bins), ...]
    let x_0 = getFloatValue("#histogram_viz_legend", "data-x_0"),
        x_1 = getFloatValue("#histogram_viz_legend", "data-x_1"),
        width_partition = 20,
        height_partition = 10;

    let containerWidth = 301;
    let containerHeight = 201;
    let margin = {
        top: 60,
        right: 60,
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

    // ============================= draw the top bar chart
    let xLine = d3.scaleLinear().range([0, width]).domain([0, 1]);
    let histogram = d3.histogram()
        .value(function(d) { return d[0]; })   // I need to give the vector of value
        .domain(xLine.domain())  // then the domain of the graphic
        .thresholds(xLine.ticks(width_partition)); // then the numbers of bins

    let bins = histogram(data);
    bins = removeLastEqual(bins) ;
    let yLine = d3.scaleLinear().range([margin.top - 5, 0]);
    yLine.domain([0, d3.max(bins, function(d) { return d.length; })]);   // d3.hist has to be called before the Y axis obviously
    let lineSvg = container
        .append("g")
        .attr("transform",
            "translate(" + margin.left + ", 5)")
        .attr('overflow', 'hidden');

    lineSvg.selectAll("rect")
        .data(bins)
        .enter()
        .append("rect")
        .attr("transform", function(d) { return "translate(" + xLine(d.x0) + "," + yLine(d.length) + ")"; })
        .attr("width", function(d) {
            return xLine(d.x1) - xLine(d.x0);
        })
        .attr("height", function(d) { return margin.top - 5 - yLine(d.length); })
        .style("fill", "black") ;

    // ============================= draw the right bar chart
    let xLineRight = d3.scaleLinear().range([0, height]).domain([min_persistence_pairs, max_persistence_pairs]);

    let histogramRight = d3.histogram()
        .value(function(d) { return d[1]; })   // I need to give the vector of value
        .domain(xLineRight.domain())  // then the domain of the graphic
        .thresholds(xLineRight.ticks(height_partition)); // then the numbers of bins

    let binsRight = histogramRight(data);
    binsRight = removeLastEqual(binsRight) ;
    let yLineRight = d3.scaleLinear().range([margin.right - 5, 0]);
    yLineRight.domain([0, d3.max(binsRight, function(d) { return d.length; })]);   // d3.hist has to be called before the Y axis obviously

    let lineSvgRight = container
        .append("g")
        .attr("transform",
            "translate(" + (margin.left + width + margin.right - 5) + ", "+ margin.top+") rotate(90)")
        .attr('overflow', 'hidden');
    
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
        .attr("transform", function(d) { return "translate(" + xLineRight(d.x2) + "," + yLineRight(d.length) + ")"; })
        .attr("width", function(d) {
            return xLineRight(d.x1) - xLineRight(d.x0);
        })
        .attr("height", function(d) { return margin.right - 5 - yLineRight(d.length); })
        .style("fill", "black") ;

    // console.log("top bar bins and right bar bins: ", bins, binsRight) ;

    // updated notification
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
        .text("# of bins") ;

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
        .text("Reliability");

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
        $("#hist-threshold").change() ;
    }
}