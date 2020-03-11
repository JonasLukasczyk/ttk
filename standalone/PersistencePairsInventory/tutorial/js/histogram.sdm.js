// iComponent: for the time index

function drawSDMHistogram(iComponent, socket) {
    if ( ! Window.PPI['image-object'] ) {
        alert("please load data at first") ;
        return ;
    }
    console.log("invoke draw SDMhistogram functionaility") ;
    $("#hist-time").val(iComponent) ;
    iComponent = parseInt(iComponent) ;

    function coverShadow() {
        $(".sdm-rect-box-cover").remove() ;
        // cover the shadow
        let threshold_idx = parseInt($("#hist-threshold").val()) ;
        let max_threshold_window = parseInt($("#threshold-window").val()) ;
        var gap = x(1) - x(0) ;
        var idx_first = gap * threshold_idx
        var idx_sec = gap * (max_threshold_window + 1) ;
        
        g_main.append("rect")
            .attr("class", "sdm-rect-box-cover")
            .attr("x", idx_first)
            .attr("y", 0)
            .attr("height", height)
            .attr("width", idx_sec - idx_first)
            .style("fill", "none")
            .style("stroke-width", 1)
            .style("stroke", "rgb(0,0,0)")
            .attr("transform", "translate(0, 0)");
    }

    $("#hidden-sdm-add-window").unbind().click(function() {
        coverShadow() ;
    }) ;

    function drawVerticalColumn() {
        // Add extra column
        // extract information
        let threshold_idx = parseInt($("#hist-threshold").val()) ;
        let max_threshold_window = parseInt($("#threshold-window").val())
        relData = []
        for (let i = 0; i < Window.PPI['histogram-height']; i++) {
            let items = data.slice((i * Window.PPI['histogram-width'] + hist_time_idx) * nComponents, (i * Window.PPI['histogram-width'] + hist_time_idx) * nComponents + nComponents);
            let cal = calculateReliability(items, threshold_idx, max_threshold_window, min_persistence_pairs) ;
            relData.push([i, cal, items[threshold_idx]]);
        }

        let xAPPI = d3.scaleBand()
            .range([0, x.bandwidth()])  // [0, x.bandwidth()]
            .domain(["0"]) ;

        let yAPPI = d3.scaleBand()
            .range([height, 0])
            .domain(myVars) ;

        var sdm_svg = container
                        .append("g")
                        .attr("transform",
                            "translate("+10+", "+margin.top+")")
                        .attr('overflow', 'hidden');
        
        sdm_svg.selectAll()
            .data(relData)
            .enter()
            .append("rect")
            .attr("class", "sdm-vertical-bin")
            .attr("id", function(d, i) { return "sdm-vertical-hist-bin-" + i ; })
            .attr("x", function (d) {
                return xAPPI("0");
            })
            .attr("y", function (d) {
                return yAPPI(d[0])
            })
            .attr("width", xAPPI.bandwidth())
            .attr("height", yAPPI.bandwidth())
            .style("fill", function (d) {
                return customColor(d[1], d[2]) ;
            })
            .append("title")
            .text(function(d) { return "reliability: "+ d[1].toFixed(2) + ", range PPI: (" + min_persistence_pairs + ", "+ max_persistence_pairs+ ")" + ", PPI: " + d[2] });
    }

    $("#hist-time").val(iComponent) ;
    
    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    let nComponents = Window.PPI['numberOfThreshold-histogram'] ;
    let fieldData = Window.PPI['image-object']['FieldData'] ;
    let threshold_idx = parseInt($("#hist-threshold").val()) ;

    let vData = [];
    tmp = getRangeOfPPI();
    min_persistence_pairs = tmp[0]
    max_persistence_pairs = tmp[1]
    
    // extract information
    let hist_time_idx = iComponent ;
    for (let i = 0; i < Window.PPI['histogram-height']; i++) {
        for (let j = 0; j < Window.PPI['sdm-histogram-width']; j++) {
            let idx = (i * Window.PPI['histogram-width'] + hist_time_idx) * nComponents + j;
            // (x-axis, y-axis, PPI)
            vData.push([j + "", i + "", data[idx]]);
        }
    }

    var tmp = getRangeOfPPI() ;
    min_persistence_pairs = tmp[0] ; 
    max_persistence_pairs = tmp[1] ;

    let max_threshold_window = parseInt($("#threshold-window").val()) ;
    let dist_data = getLegendData(getHistogramFrameData(min_persistence_pairs, threshold_idx, max_threshold_window, max_persistence_pairs, nComponents)) ;
    drawLegend(dist_data, min_persistence_pairs, max_persistence_pairs) ;

    // draw histogram
    d3.select("#sdm-histogram_viz *").remove() ;
    let myGroups = getArray( Window.PPI['sdm-histogram-width'] );
    let myVars = getArray( Window.PPI['histogram-height'] );

    var containerWidth = 1201 ;
    var containerHeight = 791 ;
    var widthBase = Window.PPI['sdm-histogram-width'] ; // Window.PPI['histogram-width']
    if ( containerWidth / widthBase < containerHeight / Window.PPI['histogram-height'] ) {
        containerHeight = Window.PPI['histogram-height'] * containerWidth / widthBase ;
    } else {
        containerWidth = widthBase * containerHeight / Window.PPI['histogram-height'] ;
    }

    let margin = {
        top: 2,
        right: 0,
        bottom: 40,
        left: 90
    } ;
    var width = containerWidth - margin.left - margin.right,
        height = containerHeight - margin.top - margin.bottom;
    let container = d3.select("#sdm-histogram_viz")
        .append("svg")
        .attr("width", containerWidth)
        .attr("height", containerHeight) ;

    var svg = container
                .append("g")
                .attr("id", "sdm-hist-id-g")
                .attr("transform",
                    "translate("+margin.left+", "+margin.top+")")
                .attr('overflow', 'hidden');

    let x = d3.scaleBand()
        .range([0, width])
        .domain(myGroups) ;

    svg.append("g")
        .attr('class', 'sdm-axis--hist--x')
        .attr("transform", "translate(0," + height + ")")
        .style("font-size", "12px")
        .call(d3.axisBottom(x).ticks(2, "s"));

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars) ;

    svg.append("g")
        .attr('class', 'sdm-axis--hist--y')
        .style("font-size", "12px")
        .call(d3.axisLeft(y).ticks(2, "s"));

    let lower = getFloatValue("#histogram_viz_legend", "data-x_0"),
        upper = getFloatValue("#histogram_viz_legend", "data-x_1") ;

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'sdm-hist-clip')
        .append('rect')
        .attr('x', 0)
        .attr('y', 0)
        .attr('width', width)
        .attr('height', height);

    // scale region
    var g_main = svg.append("g").attr('clip-path', 'url(#sdm-hist-clip)');

    var main = g_main.append("g") ;

    main.selectAll()
        .data(vData)
        .enter()
        .append("rect")
        .attr("class", "sdm-bin")
        .attr("id", function(d, i) { return "sdm-hist-bin-" + i ; })
        .attr("x", function (d) {
            return x(d[0]);
        })
        .attr("y", function (d) {
            return y(d[1])
        })
        .attr("title", function(d) { return "PPI:" + d[2] ; })
        .attr("width", x.bandwidth())
        .attr("height", y.bandwidth())
        .style("fill", function (d) {
            return customColor((lower + upper) / 2, d[2], undefined, undefined, true);
        })
        .append("title")
        .text(function(d) { return "PPI: " + d[2] });

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
        d3.select(this).attr("transform", "translate(" + x + "," + y + ") scale(" + d3.select("#sdm-range_input").property("value") + ")" );
        
        var yTrans = transFormApply($(".sdm-axis--hist--y").attr("transform"), undefined, undefined, undefined, true) ;
        var yX = yTrans[0] ;
        var yY = yTrans[1] ;
        yX += d3.event.dx ;
        yY += d3.event.dy ;    
        var v = transFormApply($(".sdm-axis--hist--y").attr("transform"), 0, yY, d3.select("#sdm-range_input").property("value")) ;    
        d3.select(".sdm-axis--hist--y").attr("transform", v);

        var xTrans = transFormApply($(".sdm-axis--hist--x").attr("transform"), undefined, undefined, undefined, true) ;
        var xX = xTrans[0] ;
        var xY = xTrans[1] ;
        xX += d3.event.dx ;
        xY += d3.event.dy ;    
        var v = transFormApply($(".sdm-axis--hist--x").attr("transform"), xX, undefined, d3.select("#sdm-range_input").property("value")) ;    
        d3.select(".sdm-axis--hist--x").attr("transform", v);

        if ( $(".sdm-rect-box-cover").length > 0) {
            var coverTrans = transFormApply($(".sdm-rect-box-cover").attr("transform"), undefined, undefined, undefined, true) ;
            var coverX = coverTrans[0] ;
            var coverY = coverTrans[1] ;
            coverX += d3.event.dx;
            coverY += d3.event.dy;
            // bins of histogram
            d3.select(".sdm-rect-box-cover").attr("transform", "translate(" + coverX + "," + coverY + ") scale(" + d3.select("#sdm-range_input").property("value") + ")" );
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

    var slider = d3.select("#sdm-range_input")
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

    // Add y-axis title
    svg.append("text")
        .attr("class", "sdm-hist-yaxis-title")
        .attr("transform", "rotate(-90)")
        .attr("y", 0 - 42 )
        .attr("x", 0 - (height / 2))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Scalar") ;

    // dd x-axis title
    svg.append("text")
        .attr("class", "sdm-hist-xaxis-title")
        .attr("y", (height + 24) )
        .attr("x", (width / 2.5 + 110))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Threshold");

    // reset x-axis, y-axis, TRICKY
    removeNiceByKicks(".sdm-axis--hist--x g", undefined, ratio=Window.PPI['thresholdRatio']) ;
    removeNiceByKicks(".sdm-axis--hist--y g", undefined, ratio=Window.PPI['thresholdRatio']) ;

    d3.selectAll(".sdm-axis--hist--y path").each(function() {
        d3.select(this).remove() ;
    }) ;

    d3.selectAll(".sdm-axis--hist--x path").each(function() {
        d3.select(this).remove() ;
    }) ;

    function zoomed() { }

    function zoomstart() { } 

    function slided(d) {
        zoom.scaleTo(svg, d3.select(this).property("value"));
    }

    $("#sdm-histogram_zoom_back").unbind().click(function() {
        zoomend("zoom_back") ;
    }) ;

    function zoomend(d) {
        var currentTransform ;
        if (d === "zoom_back") {
            currentTransform = {"x":0, "y":0, "k":1}
            d3.select("#sdm-hist-id-g").attr("transform", "translate(" + [margin.left, margin.top] + ")") ;
        } else {
            currentTransform = d3.event.transform;
        }
        main.attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");
        d3.select(".sdm-rect-box-cover").attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");

        d3.select(".sdm-axis--hist--y").attr("transform", "translate(0,"+currentTransform.y+") scale("+currentTransform.k+")");
        d3.select(".sdm-axis--hist--x").attr("transform", "translate("+currentTransform.x+","+height+") scale("+currentTransform.k+")");

        d3.selectAll(".sdm-axis--hist--y g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, 1 / currentTransform.k)) ;
        }) ;

        d3.selectAll(".sdm-axis--hist--x g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, 1 / currentTransform.k)) ;
        }) ;

        slider.property("value", currentTransform.k);
    }

    coverShadow() ;
    drawVerticalColumn() ;

    $("#hidden-sdm-move-prev").unbind().click(function() {
        $('#hist-threshold option:selected').prev().prop('selected', true) ;
        $('#threshold-window option:selected').prev().prop('selected', true) ;
        $("#sdm-histogram_zoom_back").click() ;
        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

    $("#hidden-sdm-move-next").unbind().click(function() {
        $('#hist-threshold option:selected').next().prop('selected', true) ;
        $('#threshold-window option:selected').next().prop('selected', true) ;
        $("#sdm-histogram_zoom_back").click() ;
        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

    $("#hidden-sdm-move-right-prev").unbind().click(function() {
        $('#threshold-window option:selected').prev().prop('selected', true) ;
        $("#sdm-histogram_zoom_back").click() ;
        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

    $("#hidden-sdm-move-right-next").unbind().click(function() {
        $('#threshold-window option:selected').next().prop('selected', true) ;
        $("#sdm-histogram_zoom_back").click() ;
        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

    $("#sdm-histogram_zoom_back").click() ;
    $("#histogram-notification-placeholder-0").html("") ;
}

$("#hist-time").change(function () {
    $("#hist-time option:selected").each(function () {
        drawSDMHistogram(parseInt($(this).text()), Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    });
});