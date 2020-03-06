// Add tooltips for histogram
d3.select("body")
    .append("div")
    .attr("id", "tooltip")
    .attr("class", "tooltipsHist")
    .append("svg")
    .attr("id", "tooltipSvg")
    .style("height", "160px")
    .style("padding-top", "4px");

// iComponent: for the threshold index
// socket: webSocketIO object
// returned: used for retrieving data purely
function drawHistogram(iComponent, socket) {
    console.log("invoke draw histogram functionaility") ;
    
    function coverShadow(time_idx) {
        time_idx = parseInt(time_idx) ;
        $(".rect-cover").remove() ;
        // cover the shadow
        var idx_first = x(time_idx)
        var idx_sec = x(time_idx + 1) ;
        
        svg.append("rect")
            .attr("class", "rect-cover")
            .attr("x", x(time_idx))
            .attr("y", 0)
            .attr("height", height)
            .attr("width", idx_sec - idx_first)
            .style("fill", "none")
            .style("stroke-width", 1)
            .style("stroke", "rgb(0,0,0)")
            .attr("transform", "translate(0, 0)");
    }

    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    let nComponents = Window.PPI['numberOfThreshold-histogram'] ;
    let fieldData = Window.PPI['image-object']['FieldData'] ;

    tmp = getRangeOfPPI()
    min_persistence_pairs = tmp[0]
    max_persistence_pairs = tmp[1]

    let vData = [];
    let dist_data = [] ;
    
    // extract information
    var t0 = performance.now()
    let max_threshold_window = parseInt($("#threshold-window").val())
    for (let i = 0; i < Window.PPI['histogram-height']; i++) {
        for (let j = 0; j < Window.PPI['histogram-width']; j++) {
            let idx = (i * Window.PPI['histogram-width'] + j) * nComponents + iComponent;
            let items = data.slice((i * Window.PPI['histogram-width'] + j) * nComponents, (i * Window.PPI['histogram-width'] + j) * nComponents + nComponents);
            let cal = calculateReliability(trim(items, min_persistence_pairs, max_persistence_pairs), iComponent, max_threshold_window) ;
            // (x-axis, y-axis, PPI, tuples, idx in the entire data, reliability)
            vData.push([j + "", i + "", data[idx], items, idx, cal]);
            if (data[idx] != 0) {
                dist_data.push([cal, trim(data[idx], min_persistence_pairs, max_persistence_pairs)]) ;
            }
        }
    }
    var t1 = performance.now()
    console.log("retrieve related data for rendering histogram took " + (t1 - t0) + " milliseconds.") ;

    drawLegend(dist_data, min_persistence_pairs, max_persistence_pairs) ;

    // draw histogram
    d3.select("#histogram_viz *").remove() ;
    let myGroups = getArray( Window.PPI['histogram-width']);
    let myVars = getArray( Window.PPI['histogram-height']);

    let containerWidth = 1201;
    let containerHeight = containerWidth * Window.PPI['histogram-height'] / Window.PPI['histogram-width'];
    if (containerHeight > 724) {
        containerHeight = 724;
    }
    let margin = {
        top: 2,
        right: 0,
        bottom: 40,
        left: 50
    } ;
    var width = containerWidth - margin.left - margin.right,
        height = containerHeight - margin.top - margin.bottom;
    let container = d3.select("#histogram_viz")
        .append("svg")
        .attr("width", containerWidth)
        .attr("height", containerHeight) ;

    var svg = container
                .append("g")
                .attr("transform",
                    "translate("+margin.left+", "+margin.top+")")
                .attr('overflow', 'hidden');

    let x = d3.scaleBand()
        .range([0, width])
        .domain(myGroups) ;

    svg.append("g")
        .attr('class', 'axis--hist--x')
        .attr("transform", "translate(0," + height + ")")
        .call(d3.axisBottom(x).ticks(2, "s"));

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars) ;

    svg.append("g")
        .attr('class', 'axis--hist--y')
        .call(d3.axisLeft(y).ticks(2, "s"));

    svg.selectAll()
        .data(vData)
        .enter()
        .append("rect")
        .attr("class", "bin")
        .attr("id", function(d, i) { return "hist-bin-" + i ; })
        .attr("x", function (d) {
            return x(d[0]);
        })
        .attr("y", function (d) {
            return y(d[1])
        })
        .attr("width", x.bandwidth())
        .attr("height", y.bandwidth())
        .style("fill", function (d) {
            // if the range is customed
            return customColor(d[5], d[2]);
        })
        .on("mouseover", function (d, i) {
            d3.select("#tooltipSvg").selectAll("*").remove();
            drawCurveLine("tooltipSvg", d[3], iComponent, d[5], d[2]);
            return d3.select("#tooltip").style("visibility", "visible");
        })
        .on("mousemove", function () {
            return d3.select("#tooltip").style("top", (d3.event.pageY - 10) + "px").style("left", (d3.event.pageX + 10) + "px");
        })
        .on("mouseout", function () {
            return d3.select("#tooltip").style("visibility", "hidden");
        })
        .on("click", function (d, i) {
            // alt + click
            if (event.ctrlKey) {  // click & ctrl then selected column to SDM
                Window.PPI['selected-time-id'] = d[0] ;
                coverShadow(d[0]) ;
                triggerCtrlV() ;
                return ;
            }

            // SELECT one bin
            Window.PPI['selected-bin-id'] = $(this).attr("id")
            d3.selectAll(".bin").style("stroke-width", 0.2).attr("bin-selected", "off");
            $(this).parent()[0].append($(this)[0]) ;
            d3.select(this).style("stroke-width", 2).attr("bin-selected", "on");
            var actual_scalar = ((fieldData['ScalarBounds'].Values[1] - fieldData['ScalarBounds'].Values[0]) * parseInt(d[1]) / (Window.PPI['histogram-height'] - 1) + fieldData['ScalarBounds'].Values[0]) ;
            var actual_time = fieldData['Time'].Values[parseInt(d[0])] ;
            var backMsg = 'updateUnstructuredGrid:{"FieldData": ' +
                '{"idx_time": [' + d[0] + '], "actual_time": [' + actual_time + '], ' +
                '"idx_scalar": [' + d[1] + '], "actual_scalar": [' + actual_scalar + '],' +
                '"PPI": [' + d[2] + '] }}';

            console.log("the values of selected bins, ", d[3]) ;

            $("#histogram-notification-placeholder-0").html($("#histogram-notification").attr("data-pattern-0").replace("{Scalar}", actual_scalar.toFixed(2)).replace("{Time}", actual_time.toFixed(2)) + ", &nbsp;") ;

            console.log(backMsg) ;
            if (!Window.PPI['DEV']) {
                Window.socket = socket ;
                socket.send(backMsg) ;
            }
     }) ;

    // Add y-axis title
    svg.append("text")
        .attr("class", "hist-yaxis-title")
        .attr("transform", "rotate(-90)")
        .attr("y", 0 - 42 )
        .attr("x", 0 - (height / 2))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Scalar") ;

    // // Add x-axis title
    svg.append("text")
        .attr("class", "hist-xaxis-title")
        .attr("y", (height + 24) )
        .attr("x", (width / 2.5 + 110))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Time");

    // reset x-axis, y-axis, TRICKY
    removeNiceByKicks(".axis--hist--x g") ;
    removeNiceByKicks(".axis--hist--y g") ;

    if ( Window.PPI['selected-time-id'] != -1 ) {
        coverShadow(Window.PPI['selected-time-id']) ;
    }
}

// draw a curve for histogram of each bins when hovering it
function drawCurveLine(key, data, iComponent, reliability, pp) {
    tmp = getRangeOfPPI();
    let lower = tmp[0]
    let upper = tmp[1]

    data = trim(data, lower, upper) ;
    let svg = d3.select("#" + key);
    let x = d3.scaleLinear().range([0, 240]);
    let y = d3.scaleLinear().range([132, 0]);

    let dl = d3.line()
        .x(function (d, i) {
            return x(i * Window.PPI['thresholdRatio']);
        })
        .y(function (d) {
            return y(d);
        });

    // Scale the range of the data
    x.domain(d3.extent(data, function (d, i) {
        return i * Window.PPI['thresholdRatio'];
    })).nice();

    // start from 0
    y.domain([0, upper]);

    svg.append("path")
        .attr("class", "line2")
        .attr("d", dl(data))
        .style("stroke-width", 2)
        .style("stroke", "red")
        .attr("transform", "translate(28" + ", 4" + ")");

    let max_threshold_window = parseInt($("#threshold-window").val())
    var idx_first = x(iComponent * Window.PPI['thresholdRatio']);
    var idx_sec = x(max_threshold_window * Window.PPI['thresholdRatio'])
    
    addSvgLine(svg, idx_first, 0, idx_first, 136, "translate(28, 0)") ;
    addSvgLine(svg, idx_sec, 0, idx_sec, 136, "translate(28, 0)")
    
    svg.append("rect")
        .attr("class", "zero")
        .attr("x", idx_first)
        .attr("y", 0)
        .attr("height", 136)
        .attr("width", idx_sec - idx_first)
        .style("opacity", 0.3)
        .attr("transform", "translate(28, 0)");

    svg.append("text")
        .attr("x", 140)
        .attr("y", 16)
        .attr("text-anchor", "middle")
        .style("font-size", "12px")
        .style("font-family", "sans-serif")
        .text("reliability: " + reliability.toFixed(2) + ", range PPI: (" + lower + ", "+ upper+ ")" + ", PPI: " + pp);

    svg.append("g")
        .attr("transform", "translate(28, 136)")
        .call(d3.axisBottom(x));

    // Add the Y Axis
    svg.append("g")
        .attr("transform", "translate(28" + ", 4" + ")")
        .call(d3.axisLeft(y));
}