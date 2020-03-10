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
        $("[id^=hist-bin-]").each(function(e) { 
            var id = parseInt($(this).attr("id").replace("hist-bin-", ""))
            if ( id % Window.PPI['histogram-width'] === time_idx ) {
                $(this).css("opacity", 1) ;
            } else {
                $(this).css("opacity", 0.1) ;
            }
        })
       
    }

    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    let nComponents = Window.PPI['numberOfThreshold-histogram'] ;
    let fieldData = Window.PPI['image-object']['FieldData'] ;

    var tmp = getRangeOfPPI()
    min_persistence_pairs = tmp[0] ; 
    max_persistence_pairs = tmp[1] ;

    let max_threshold_window = parseInt($("#threshold-window").val())
    let vData = getHistogramFrameData(min_persistence_pairs, iComponent, max_threshold_window, max_persistence_pairs, nComponents) ;
    let dist_data = getLegendData(vData) ;

    var t00 = performance.now() ;
    drawLegend(dist_data, min_persistence_pairs, max_persistence_pairs) ;
    var t11 = performance.now() ;
    console.log("the cost of time for rendering legend: " + (t11 - t00) + " milliseconds");

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
        .call(d3.axisBottom(x)) ;

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars) ;

    svg.append("g")
        .attr('clip-path', 'url(#cc-hist-clip)')
        .attr('class', 'axis--hist--y')
        .attr("transform", "translate(0,0)")
        .call(d3.axisLeft(y));

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'hist-clip')
        .append('rect')
        .attr('x', 1)
        .attr('y', 0)
        .attr('width', width)
        .attr('height', height);

    // scale region
    var t000 = performance.now() ;
    var g_main = svg.append("g").attr('clip-path', 'url(#hist-clip)');

    var main = g_main.append("g") ;

    main.selectAll()
        .data(vData)
        .enter()
        .append("rect")
        .attr("name", "bin")
        .attr("id", function(d, i) { return "hist-bin-" + i ; })
        .attr("x", function (d) {
            return x(d[0]);
        })
        .attr("y", function (d) {
            return y(d[1])
        })
        .attr("width", x.bandwidth())
        .attr("height", y.bandwidth())
        .attr("class", function(d) {
            return customColor(d[5], d[2], undefined, undefined, undefined, true);
        }).on("mouseover", function (d, i) {
            if (!event.ctrlKey) {
                d3.select("#tooltipSvg").selectAll("*").remove();
                drawCurveLine("tooltipSvg", d[3], iComponent, d[5], d[2]);
                return d3.select("#tooltip").style("visibility", "visible");
            }
        })
        .on("mousemove", function () {
            return d3.select("#tooltip").style("top", (d3.event.pageY - 10) + "px").style("left", (d3.event.pageX + 10) + "px");
        })
        .on("mouseout", function () {
            return d3.select("#tooltip").style("visibility", "hidden");
        })
        .on("mousedown", function (d, i) {
           // d3.event.stopPropagation();
            if (event.ctrlKey) {  // click & ctrl then selected column to SDM
                Window.PPI['selected-time-id'] = d[0] ;
                coverShadow(d[0]) ;
            }
        })
        // https://stackoverflow.com/questions/49808356/d3-mousedown-event-fires-but-mouseup-event-does-not-fire 
        .on("mouseup", function (d, i) {
            if (event.ctrlKey) {
                triggerCtrlV() ;
            }
        })
        .on("click", function (d, i) {
            Window.PPI['selected-bin-id'] = $(this).attr("id")
            d3.selectAll("[name=bin]").style("stroke-width", 0.2).attr("bin-selected", "off");
            $(this).parent()[0].append($(this)[0]) ;
            d3.select(this).style("stroke-width", 2).attr("bin-selected", "on");
            var actual_scalar = ((fieldData['ScalarBounds'].Values[1] - fieldData['ScalarBounds'].Values[0]) * parseInt(d[1]) / (Window.PPI['histogram-height'] - 1) + fieldData['ScalarBounds'].Values[0]) ;
            var actual_time = fieldData['Time'].Values[parseInt(d[0])] ;
            var backMsg = 'updateUnstructuredGrid:{"FieldData": ' +
                '{"idx_time": [' + d[0] + '], "actual_time": [' + actual_time + '], ' +
                '"idx_scalar": [' + d[1] + '], "actual_scalar": [' + actual_scalar + '],' +
                '"PPI": [' + d[2] + '] }}';

            // console.log("the values of selected bins: ", d[3]) ;

            $("#histogram-notification-placeholder-0").html($("#histogram-notification").attr("data-pattern-0").replace("{Scalar}", actual_scalar.toFixed(2)).replace("{Time}", actual_time.toFixed(2)) + ", &nbsp;") ;

            console.log(backMsg) ;
            if (!Window.PPI['DEV']) {
                Window.socket = socket ;
                socket.send(backMsg) ;
            }
        }) ;
    
    var drag = d3.drag()
        .on("start", drag_start)
        .on("drag", drag_drag);

    var zoom = d3.zoom()
                 .scaleExtent([1, 10])
                 .on("zoom", zoomed)
                 .on("start", zoomstart)
                 .on("end", zoomend) ;

    var slider = d3.select("#range_input")
        .datum({})
        .attr("value", zoom.scaleExtent()[0])
        .attr("min", zoom.scaleExtent()[0])
        .attr("max", zoom.scaleExtent()[1])
        .attr("step", (zoom.scaleExtent()[1] - zoom.scaleExtent()[0]) / 100)
        .on("input", slided);

    function drag_start() {
        console.log("drag_start") ;
    }

    function drag_drag() {
        console.log("drag_drag") ;
    }

    function callFunc() { // most time-consuming part, avg time is 1.5 seconds
        svg.call(zoom) ;
        // svg.call(drag) ;
    }
    setTimeout(callFunc, 0) ;
    
    var t111 = performance.now() ;
    console.log("the cost of time for rendering bins of histogram: " + (t111 - t000) + " milliseconds");

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

    // reset x-axis, y-axis
    removeNiceByKicks(".axis--hist--x g") ;
    removeNiceByKicks(".axis--hist--y g") ;

    function zoomed() { }

    function zoomstart() { } 

    function slided(d) {
        zoom.scaleTo(svg, d3.select(this).property("value"));
    }

    $("#histogram_zoom_back").unbind().click(function() {
        zoomend("zoom_back") ;
    }) ;

    function zoomend(d) {
        var currentTransform ;
        if (d === "zoom_back") {
            currentTransform = {"x":0, "y":0, "k":1}
        } else {
            currentTransform = d3.event.transform;
        }
        main.attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");

        d3.select(".axis--hist--y").attr("transform", "translate(0,"+currentTransform.y+") scale("+currentTransform.k+")");
        d3.select(".axis--hist--x").attr("transform", "translate("+currentTransform.x+","+height+") scale("+currentTransform.k+")");
        slider.property("value", currentTransform.k);

        $(".axis--hist--x g").each(function() {
            var v = $(this).attr("transform") ;
            var vItems = v.split(" scale") ;
            $(this).attr("transform", vItems[0] + " " + "scale(" + 1 / currentTransform.k+")") ;
        }) ;

        $(".axis--hist--x path").each(function() {
            $(this).attr("transform", "scale(" + 1 / currentTransform.k+")") ;
        }) ;

        $(".axis--hist--y g").each(function() {
            var v = $(this).attr("transform") ;
            var vItems = v.split(" scale") ;
            $(this).attr("transform", vItems[0] + " " + "scale(" + 1 / currentTransform.k+")") ;
        }) ;

        $(".axis--hist--y path").each(function() {
            $(this).attr("transform", "scale(" + 1 / currentTransform.k+")") ;
        }) ;
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
    y.domain([lower, upper]);

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