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
    if ( ! Window.PPI['image-object'] ) {
        alert("please load data at first") ;
        return ;
    }
    console.log("invoke draw histogram functionaility") ;
    
    function coverShadow(time_idx) {
        // cover shadow in a new method
        time_idx = parseInt(time_idx) ;
        $("#hist-clip-highlight g").each(function() {
            $(this).detach().appendTo("#hist-clip-opacity"); 
        }) ;

        $("#hist-clip-opacity").css("opacity", 0.1) ;

        $("[mdm-bin-time-idx=" + time_idx + "]").each(function() {
            $(this).detach().appendTo("#hist-clip-highlight") ;
        }) ;
    }

    function drawCoverBox() {
        $(".mdm-rect-box-cover").remove() ;
        // cover the shadow
        if ( Window.PPI['boxplot-timestamp-range-mode']['enable'] ) {
            var left = Window.PPI['boxplot-timestamp-range-mode']['start'] ;
            var right = Window.PPI['boxplot-timestamp-range-mode']['end'] ;
            var gap = x(1) - x(0) ;
            var idx_first = gap * left ;
            var idx_sec = gap * (right + 1) ;
            
            g_main.append("rect")
                .attr("class", "mdm-rect-box-cover")
                .attr("x", idx_first)
                .attr("y", 0)
                .attr("height", height)
                .attr("width", idx_sec - idx_first)
                .style("fill", "none")
                .style("stroke-width", 1)
                .style("stroke", "rgb(0,0,0)")
                .attr("transform", "translate(0, 0)");
            }
    }

    $("#hidden-mdm-add-window").unbind().click(function() {
        drawCoverBox() ;
    }) ;

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

    var containerWidth = 1201 ;
    var containerHeight = 791 ;
    if ( containerWidth / Window.PPI['histogram-width'] < containerHeight / Window.PPI['histogram-height'] ) {
        containerHeight = Window.PPI['histogram-height'] * containerWidth / Window.PPI['histogram-width'] ;
    } else {
        containerWidth = Window.PPI['histogram-width'] * containerHeight / Window.PPI['histogram-height'] ;
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
                .attr("id", "hist-id-g")
                .attr("transform",
                    "translate("+margin.left+", "+margin.top+")")
                .attr('overflow', 'hidden');

    let x = d3.scaleBand()
        .range([0, width])
        .domain(myGroups) ;

    svg.append("g")
        .attr('class', 'axis--hist--x')
        .attr("transform", "translate(0," + height + ")")
        .style("font-size", "12px")
        .call(d3.axisBottom(x)) ;

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars) ;

    svg.append("g")
        .attr('class', 'axis--hist--y')
        .attr("transform", "translate(0,0)")
        .style("font-size", "12px")
        .call(d3.axisLeft(y));

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'hist-clip')
        .append('rect')
        .attr('x', 0)
        .attr('y', 0)
        .attr('width', width)
        .attr('height', height);

    // scale region
    var t000 = performance.now() ;
    var g_main = svg.append("g").attr('clip-path', 'url(#hist-clip)');

    var main = g_main.append("g").attr("id", "hist-clip-opacity") ;
    g_main.append("g").attr("id", "hist-clip-highlight") ;

    main.selectAll()
        .data(vData)
        .enter()
        .append("rect")
        .attr("name", "bin")
        .attr("mdm-bin-time-idx", function(d, i) { return parseInt(d[0]) ; })
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
                if (Window.PPI["X-mode"] === 0) {
                    d3.select("#tooltipSvg").selectAll("*").remove();
                    drawCurveLine("tooltipSvg", d[3], iComponent, d[5], d[2]);
                    return d3.select("#tooltip").style("visibility", "visible");
                }
            }
        })
        .on("mousemove", function () {
            return d3.select("#tooltip").style("top", (d3.event.pageY - 10) + "px").style("left", (d3.event.pageX + 10) + "px");
        })
        .on("mouseout", function () {
            return d3.select("#tooltip").style("visibility", "hidden");
        })
        .on("mousedown", function (d, i) {
            if (event.ctrlKey) {  
                Window.PPI['selected-time-id'] = d[0] ;
                coverShadow(d[0]) ;
                // https://stackoverflow.com/questions/49808356/d3-mousedown-event-fires-but-mouseup-event-does-not-fire
                d3.event.stopPropagation();  // prevent zoom & drag consuming mouseup event while ctrl is pressed
            }
        })
        .on("mouseup", function (d, i) {
            if (event.ctrlKey) {
                triggerCtrlV() ;
            }
        })
        .on("click", function (d, i) {
            Window.PPI['selected-bin-id'] = $(this).attr("id")
            d3.selectAll("[name=bin]").style("stroke-width", 0.1).attr("bin-selected", "off");
            $(this).parent()[0].append($(this)[0]) ;
            d3.select(this).style("stroke-width", 2).attr("bin-selected", "on");
            var actual_scalar = ((fieldData['ScalarBounds'].Values[1] - fieldData['ScalarBounds'].Values[0]) * parseInt(d[1]) / (Window.PPI['histogram-height'] - 1) + fieldData['ScalarBounds'].Values[0]) ;
            var actual_time = fieldData['Time'].Values[parseInt(d[0])] ;
            var backMsg = 'updateUnstructuredGrid:{"FieldData": ' +
                '{"idx_time": [' + d[0] + '], "actual_time": [' + actual_time + '], ' +
                '"idx_scalar": [' + d[1] + '], "actual_scalar": [' + actual_scalar + '],' +
                '"PPI": [' + d[2] + '] }}';

            $("#histogram-notification-placeholder-default").html("") ;
            $("#histogram-notification-placeholder-0").html($("#histogram-notification").attr("data-pattern-0").replace("{Scalar}", actual_scalar.toFixed(2)).replace("{Time}", actual_time.toFixed(2)) + ", &nbsp;") ;

            console.log(backMsg) ;
            if (!Window.PPI['DEV']) {
                Window.socket = socket ;
                socket.send(backMsg) ;
            }
        }) ;
    
    var drag = d3.drag()
            .on("start", dragstarted)
            .on("drag", dragged)
            .on("end", dragended);

    function dragstarted(d) {
        d3.event.sourceEvent.stopPropagation();
        d3.select(this).classed("dragging", true);
    }

    function dragged(d) {
        var trans = transFormApply($(this).attr("transform"), undefined, undefined, undefined, true) ;
        var x = trans[0] ;
        var y = trans[1] ;
        x += d3.event.dx;
        y += d3.event.dy;
        // bins of histogram
        d3.select(this).attr("transform", "translate(" + x + "," + y + ") scale(" + d3.select("#range_input").property("value") + ")" );
        
        var yTrans = transFormApply($(".axis--hist--y").attr("transform"), undefined, undefined, undefined, true) ;
        var yX = yTrans[0] ;
        var yY = yTrans[1] ;
        yX += d3.event.dx ;
        yY += d3.event.dy ;    
        var v = transFormApply($(".axis--hist--y").attr("transform"), 0, yY, d3.select("#range_input").property("value")) ;    
        d3.select(".axis--hist--y").attr("transform", v);

        var xTrans = transFormApply($(".axis--hist--x").attr("transform"), undefined, undefined, undefined, true) ;
        var xX = xTrans[0] ;
        var xY = xTrans[1] ;
        xX += d3.event.dx ;
        xY += d3.event.dy ;    
        var v = transFormApply($(".axis--hist--x").attr("transform"), xX, undefined, d3.select("#range_input").property("value")) ;    
        d3.select(".axis--hist--x").attr("transform", v);

        if ( $(".mdm-rect-box-cover").length > 0) {
            var coverTrans = transFormApply($(".mdm-rect-box-cover").attr("transform"), undefined, undefined, undefined, true) ;
            var coverX = coverTrans[0] ;
            var coverY = coverTrans[1] ;
            coverX += d3.event.dx;
            coverY += d3.event.dy;
            // bins of histogram
            d3.select(".mdm-rect-box-cover").attr("transform", "translate(" + coverX + "," + coverY + ") scale(" + d3.select("#range_input").property("value") + ")" );
        }
    }

    function dragended(d) {
        d3.select(this).classed("dragging", false);
    }
    
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

    function callFunc() { // most time-consuming part, avg time is 1.5 seconds
        main.call(drag).call(zoom);
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

    d3.selectAll(".axis--hist--y path").each(function() {
        d3.select(this).remove() ;
    }) ;

    d3.selectAll(".axis--hist--x path").each(function() {
        d3.select(this).remove() ;
    }) ;

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
            d3.select("#hist-id-g").attr("transform", "translate(" + [margin.left, margin.top] + ")") ;
        } else {
            currentTransform = d3.event.transform;
        }
        main.attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");
        d3.select(".mdm-rect-box-cover").attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");

        d3.select(".axis--hist--y").attr("transform", "translate(0,"+currentTransform.y+") scale("+currentTransform.k+")");
        d3.select(".axis--hist--x").attr("transform", "translate("+currentTransform.x+","+height+") scale("+currentTransform.k+")");

        d3.selectAll(".axis--hist--y g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, 1 / currentTransform.k)) ;
        }) ;

        d3.selectAll(".axis--hist--x g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, 1 / currentTransform.k)) ;
        }) ;

        slider.property("value", currentTransform.k);
    }

    $("#histogram_zoom_back").click() ;
    $("#histogram-notification-placeholder-0").html("") ;
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