function drawLegend(data, max_persistence_pairs) {
    // more complicated
    // data processing
    // reduced_data: [(reliability, # of bins), ...]
    var x_0 = 0.3,
        x_1 = 0.6;
        width_view = 10;
        width_partition = 100;
        height_view = 5;
        height_partition = 30;
    
    var reduced_data = [];
    for (var i = 0; i < width_view; i++) {
        reduced_data.push(0);
    }

    for (var i = 0; i < data.length; i++) {
        if (data[i][0] == 1) {
            reduced_data[width_view - 1] += 1;
        } else {
            reduced_data[Math.floor(data[i][0] / (1 / width_view))] += 1;
        }
    }

    // define the container and svg
    d3.select("#my_dataviz_legend *").remove();
    
    let containerWidth = 501;
    let containerHeight = 401;
    let margin = {
        top: 150,
        right: 150,
        bottom: 20,
        left: 20
    };

    var width = containerWidth - margin.left - margin.right,
        height = containerHeight - margin.top - margin.bottom;

    let container = d3.select("#my_dataviz_legend")
                    .append("svg")
                    .attr("width", containerWidth)
                    .attr("height", containerHeight);

    // draw the top line SVG
    var lineSvg = container
                .append("g")
                .attr("transform",
                    "translate(" + margin.left + ", 5)")
                .attr('overflow', 'hidden');

    let xLine = d3.scaleLinear().range([0, width]);
    let yLine = d3.scaleLinear().range([margin.top - 5, 0]);

    let dl = d3.line().x(function(d, i) { return xLine(i); })
                      .y(function(d) { return yLine(d); });

    xLine.domain([0, width_view - 1]).nice();
    yLine.domain([0, d3.max(reduced_data, function(d) { return d; })]);

    reduced_data[0] = 80;
    lineSvg.append("path")
        .attr("class", "line2")
        .attr("d", dl(reduced_data))
        .style("stroke-width", 2)
        .style("stroke", "red");

    // draw the right SVG
    var lineSvgRight = container
        .append("g")
        .attr("transform",
            "translate(" + (margin.left + width + margin.right - 5) + ", "+ margin.top+") rotate(90)")
        .attr('overflow', 'hidden');

    let xLineRight = d3.scaleLinear().range([0, height]);
    let yLineRight = d3.scaleLinear().range([margin.right - 5, 0]);

    let dlRight = d3.line().x(function(d, i) { return xLineRight(i); })
                      .y(function(d) { return yLineRight(d); });

    xLineRight.domain([0, width_view - 1]).nice();
    yLineRight.domain([0, d3.max(reduced_data, function(d) { return d; })]);

    reduced_data[0] = 80;
    lineSvgRight.append("path")
        .attr("class", "line2")
        .attr("d", dlRight(reduced_data))
        .style("stroke-width", 2)
        .style("stroke", "red");
    
    // draw the main body SVG
    let widthGroups = getArray(width_partition);
    let heightGroups = getArray(height_partition);
    let vData = getArray(width_partition * height_partition);

    var svg = container
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
        .attr("x", function(d, i) {
            return x(i % width_partition);
        })
        .attr("y", function(d, i) {
            return y(Math.ceil(i / width_partition));
        })
        .attr("width", x.bandwidth())
        .attr("height", y.bandwidth())
        .style("fill", function(d, i) {
            return customColor((i % width_partition) / width_partition, Math.max(1, Math.ceil(i / width_partition)), height_partition, x_0, x_1);
        })
    removeNiceByKicks(".axis--legend--y", 0);
    removeNiceByKicks(".axis--legend--x g", 10);
}