function drawLegend(data, max_persistence_pairs) {
    function removeLastEqual(bins) {
        if ( bins.length > 0 ) {
            let lastEle = bins.slice(-1)[0] ;
            let lastSecondEle = bins.slice(-2, -1)[0] ;
            if ( lastEle.x0 === lastEle.x1 ) {
                for (let i = 0; i < lastEle.length ; i ++) {
                    lastSecondEle.push(lastEle[i]) ;
                }
            }
            bins = bins.slice(0, -1) ;
        }
        return bins ;
    }

    // more complicated
    // data processing
    // reduced_data: [(reliability, # of bins), ...]
    let x_0 = getFloatValue("#my_dataviz_legend", "data-x_0"),
        x_1 = getFloatValue("#my_dataviz_legend", "data-x_1"),
        width_partition = 50,
        height_partition = 30;
    
    // define the container and svg
    d3.select("#my_dataviz_legend *").remove();
    
    let containerWidth = 301;
    let containerHeight = 201;
    let margin = {
        top: 60,
        right: 60,
        bottom: 20,
        left: 20
    };

    // http://bl.ocks.org/nnattawat/8916402
    let width = containerWidth - margin.left - margin.right,
        height = containerHeight - margin.top - margin.bottom;

    let container = d3.select("#my_dataviz_legend")
                    .append("svg")
                    .attr("width", containerWidth)
                    .attr("height", containerHeight);

    // draw the top line SVG
    let lineSvg = container
                .append("g")
                .attr("transform",
                    "translate(" + margin.left + ", 5)")
                .attr('overflow', 'hidden');

    let xLine = d3.scaleLinear().range([0, width]).domain([0, 1]);

    let histogram = d3.histogram()
        .value(function(d) { return d[0]; })   // I need to give the vector of value
        .domain(xLine.domain())  // then the domain of the graphic
        .thresholds(xLine.ticks(width_partition)); // then the numbers of bins
    let bins = histogram(data);
    bins = removeLastEqual(bins) ;

    let yLine = d3.scaleLinear().range([margin.top - 5, 0]);
    yLine.domain([0, d3.max(bins, function(d) { return d.length; })]);   // d3.hist has to be called before the Y axis obviously

    lineSvg.selectAll("rect")
        .data(bins)
        .enter()
        .append("rect")
        .attr("transform", function(d) { return "translate(" + xLine(d.x0) + "," + yLine(d.length) + ")"; })
        .attr("width", function(d) {
            if ( d.x0 === d.x1 ) {
                return xLine(0.01) - xLine(0);
            }
            return xLine(d.x1) - xLine(d.x0);
        })
        .attr("height", function(d) { return margin.top - 5 - yLine(d.length); })
        .style("fill", "#69b3a2") ;

    // draw the right SVG
    let lineSvgRight = container
        .append("g")
        .attr("transform",
            "translate(" + (margin.left + width + margin.right - 5) + ", "+ margin.top+") rotate(90)")
        .attr('overflow', 'hidden');

    let xLineRight = d3.scaleLinear().range([0, height]).domain([0, max_persistence_pairs]);

    let histogramRight = d3.histogram()
        .value(function(d) { return d[1]; })   // I need to give the vector of value
        .domain(xLineRight.domain())  // then the domain of the graphic
        .thresholds(xLineRight.ticks(height_partition)); // then the numbers of bins

    let binsRight = histogramRight(data);
    binsRight = removeLastEqual(binsRight) ;
    let yLineRight = d3.scaleLinear().range([margin.right - 5, 0]);
    yLineRight.domain([0, d3.max(binsRight, function(d) { return d.length; })]);   // d3.hist has to be called before the Y axis obviously

    lineSvgRight.selectAll("rect")
        .data(binsRight)
        .enter()
        .append("rect")
        .attr("transform", function(d) { return "translate(" + xLineRight(d.x0) + "," + yLineRight(d.length) + ")"; })
        .attr("width", function(d) {
            if ( d.x0 === d.x1 ) {
                return xLineRight(max_persistence_pairs * 0.01) - xLineRight(0) ;
            }
            return xLineRight(d.x1) - xLineRight(d.x0);
        })
        .attr("height", function(d) { return margin.right - 5 - yLineRight(d.length); })
        .style("fill", "#4575b4") ;

    // GET errors
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
                return customColor(x_0 / 2, Math.floor(i / 3) + 0.01, 5);
            } else if ( i % 3 === 1) {
                return customColor(x_0 + (x_1 - x_0) / 2, Math.floor(i / 3) + 0.01, 5);
            } else {
                return customColor(x_1 + (1 - x_1) / 2, Math.floor(i / 3) + 0.01, 5);
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
        .text("Color Scalar") ;

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

    svg.selectAll('line')
        .data(lines)
        .enter()
        .append('line')
        .attr("x1", function(d) {return d.x1;})
        .attr("y1", 0)
        .attr("x2", function(d) {return d.x2;})
        .attr("y2", height)
        .style("stroke", "#01665e")
        .style("stroke-width", 2) ;
}