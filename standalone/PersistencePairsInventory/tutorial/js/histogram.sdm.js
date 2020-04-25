// iComponent: for the time index

function drawSDMHistogram(socket) {

    function getSDMHistogramKey() {
        var tmp = getRangeOfPPI()
        min_persistence_pairs = tmp[0] ; 
        max_persistence_pairs = tmp[1] ;
        // key: iComponent + left-right relibility + min-max threshold + min-max range of the bins
        return Window.PPI['selected-time-id'] + ":" + getFloatValue("#histogram_viz_legend", "data-x_0") + "-" + getFloatValue("#histogram_viz_legend", "data-x_1") + 
                    $("#hist-threshold").val() + "-" + $("#threshold-window").val() + ":" +  
                    min_persistence_pairs + "-" + max_persistence_pairs ;
    }

    var key = getSDMHistogramKey() ;
    if ( key === Window.PPI['redraw-SDM-key'] ) {
        return ;
    }
    Window.PPI['redraw-SDM-key'] = key ;

    $("#sdm-histogram_viz").loading({theme: 'light'});
    if ( ! Window.PPI['image-object'] ) {
        alert("please load data at first") ;
        return ;
    }
    iComponent = Window.PPI['selected-time-id'] ;  // selected time stamp
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
            .attr("transform", transFormApply(d3.select("#sdm-hist-clip-g").attr("transform")));
    }

    $("#hidden-sdm-add-window").unbind().click(function() {
        coverShadow() ;
    }) ;

    function drawVerticalColumn() {
        // Add extra column
        // extract information
        var sdm_vertical_column_g_transform = $(".sdm-vertical-column-g").attr("transform");
        d3.select("#sdm-vertical-column-g-id").remove() ;
        let threshold_idx = parseInt($("#hist-threshold").val()) ;
        let max_threshold_window = parseInt($("#threshold-window").val())
        relData = []
        for (let i = 0; i < Window.PPI['histogram-height']; i++) {
            let items = data.slice((i * Window.PPI['histogram-width'] + hist_time_idx) * nComponents, (i * Window.PPI['histogram-width'] + hist_time_idx) * nComponents + nComponents);
            // reliability version
            // items = trim(items, min_persistence_pairs, max_persistence_pairs) ;
            // let cal = calculateReliability(items, threshold_idx, max_threshold_window, min_persistence_pairs) ;
            
            // similarity version
            let cal = calculateSimilarity(items, threshold_idx, max_threshold_window) ;
            relData.push([i, cal, items[threshold_idx]]);
        }

        let xAPPI = d3.scaleBand()
            .range([0, x.bandwidth()])  // [0, x.bandwidth()]
            .domain(["0"]) ;

        let yAPPI = d3.scaleBand()
            .range([height, 0])
            .domain(myVars) ;

        var sdm_svg = container
                        .append("svg")
                        .attr("id", "sdm-vertical-column-g-id")
                        .attr("width", 100)
                        .attr("height", Math.max(height, fixedHeight))

        var sdm_g = sdm_svg.append("g")
                        .attr("class", "sdm-vertical-column-g")
                        .attr("transform",
                            "translate("+10+", "+ margin.top +")")
                        .attr('overflow', 'hidden');

        sdm_g.selectAll()
            .data(relData)
            .enter()
            .append("rect")
            .attr("class", "sdm-vertical-bin")
            .attr("id", function(d, i) { return "sdm-vertical-hist-bin-" + i ; })
            .attr("x", function (d) {
                return xAPPI("0");
            })
            .attr("y", function (d) {
                return yAPPI(d[0]) ;
            })
            .attr("width", xAPPI.bandwidth() / scaleConvert(d3.select("#sdm-range_input").property("value")))
            .attr("ori-width", xAPPI.bandwidth())
            .attr("height", yAPPI.bandwidth())
            .style("fill", function (d) {
                return customColor(d[1], d[2]) ;
            })
            .append("title")
            .text(function(d) { return "similarity: "+ d[1].toFixed(2) + ", range PPI: (" + min_persistence_pairs + ", "+ max_persistence_pairs+ ")" + ", PPI: " + d[2] }) ;
            
        if ( sdm_vertical_column_g_transform ) {
            $(".sdm-vertical-column-g").attr("transform", sdm_vertical_column_g_transform) ;
        }
    }

    $("#hist-time").val(iComponent) ;
    
    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    let nComponents = Window.PPI['numberOfThreshold-histogram'] ;
    let fieldData = Window.PPI['image-object']['FieldData'] ;
    let threshold_idx = parseInt($("#hist-threshold").val()) ;
    
    var cx = 0,
        cy = 0 ;

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

    // FLAG, draw histogram, remove all previous data
    var sdm_hist_clip_g_transform = $("#sdm-hist-clip-g").attr("transform") ;
    var sdm_rect_box_cover_transform = $(".sdm-rect-box-cover").attr("transform");
    var sdm_axis_hist_y = $(".sdm-axis--hist--y").attr("transform");
    var sdm_axis_hist_x = $(".sdm-axis--hist--x").attr("transform");
    var sdm_axis_hist_y_g = $(".sdm-axis--hist--y g").length > 0? $($(".sdm-axis--hist--y g")[0]).attr("transform"): undefined ;
    var sdm_axis_hist_x_g = $(".sdm-axis--hist--x g").length > 0? $($(".sdm-axis--hist--x g")[0]).attr("transform"): undefined ;

    d3.select("#sdm-histogram_viz *").remove() ;
    let myGroups = getArray( Window.PPI['sdm-histogram-width'] );
    let myVars = getArray( Window.PPI['histogram-height'] );

    // var fixedContainerWidth = 1251,
    //     fixedContainerHeight = 841 ;
    var fixedContainerWidth = $("#histogram_viz").outerWidth(),
        fixedContainerHeight = Window.PPI['main-container-height'] ;

    var containerHeight = Window.PPI['main-container-height'] ;
    var hBase = containerHeight / Window.PPI['histogram-height'] ;
    hBase = Math.min(60, hBase) ;
    containerHeight = hBase * Window.PPI['histogram-height'] ;
    var containerWidth = hBase * Window.PPI['sdm-histogram-width'] + hBase + 50;

    let margin = {
        top: 2,
        right: 0,
        bottom: 40,
        left: hBase + 90
    } ;
    var width = containerWidth - margin.left - margin.right,
        height = containerHeight - margin.top - margin.bottom;

    var fixedWidth = fixedContainerWidth - margin.left - margin.right,
        fixedHeight = fixedContainerHeight - margin.top - margin.bottom;

    let container = d3.select("#sdm-histogram_viz")
        .append("svg")
        .attr("width", Math.max(containerWidth, fixedContainerWidth))
        .attr("height", Math.max(containerHeight, fixedContainerHeight)) ;

    var svg = container
                .append("g")
                .attr("id", "sdm-hist-id-g")
                .attr("transform",
                    "translate("+margin.left+", "+margin.top+")")
                .attr('overflow', 'hidden');

    let x = d3.scaleBand()
        .range([0, width])
        .domain(myGroups) ;

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'sdm-hist-clip-x')
        .append('rect')
        .attr('x', 0)
        .attr('y', fixedHeight)
        .attr('width', fixedWidth)
        .attr('height', margin.bottom);

    var xg = svg.append("g")
        .attr('clip-path', 'url(#sdm-hist-clip-x)')

    xg.append("g")
        .attr('class', 'sdm-axis--hist--x')
        .attr("transform", "translate(0," + fixedHeight + ")")
        .style("font-size", "16px")
        .call(d3.axisBottom(x).ticks(2, "s"));

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars) ;

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'sdm-hist-clip-y')
        .append('rect')
        .attr('x', -50)
        .attr('y', 0)
        .attr('width', margin.left)
        .attr('height', fixedHeight);

    var yg = svg.append("g")
        .attr('clip-path', 'url(#sdm-hist-clip-y)')

    yg.append("g")
        .attr('class', 'sdm-axis--hist--y')
        .style("font-size", "16px")
        .call(d3.axisLeft(y).ticks(2, "s"));

    let lower = getFloatValue("#histogram_viz_legend", "data-x_0"),
        upper = getFloatValue("#histogram_viz_legend", "data-x_1") ;

    svg.append('defs')
        .append('clipPath')
        .attr('id', 'sdm-hist-clip')
        .append('rect')
        .attr('x', 0)
        .attr('y', 0)
        .attr('width', fixedWidth)
        .attr('height', fixedHeight);

    var parentSvg = svg.append("svg") ;
    parentSvg.on("mouseover", function () {
        var tmp = d3.mouse(this) ;
        cx = tmp[0] ;
        cy = tmp[1] ;
    } )
    .on("mousemove", function() {
        var tmp = d3.mouse(this) ;
        cx = tmp[0] ;
        cy = tmp[1] ;
    });
    var g_main = parentSvg.append("g").attr('clip-path', 'url(#sdm-hist-clip)');

    var main = g_main.append("g").attr("id", "sdm-hist-clip-g") ;

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
        
        .on("click", function (d, i) {
            $("#RendererContainer").loading({theme: "light"}) ;
            Window.PPI['selected-bin-id-sdm'] = $(this).attr("id")
            d3.selectAll(".sdm-bin").style("stroke-width", 0.1).attr("bin-selected-sdm", "off");
            $(this).parent()[0].append($(this)[0]) ;
            d3.select(this).style("stroke-width", 2).attr("bin-selected-sdm", "on");
            var actual_scalar = getScalarArray(parseInt(d[1]));
            var actual_time = $("#hist-time :selected").text()
            var idx_time = $("#hist-time :selected").val()
            var actual_threshold = parseInt(d[0]) * Window.PPI['thresholdRatio'] ;
            var backMsg = 'updateUnstructuredGrid:{"FieldData": ' +
                '{"idx_time": [' + idx_time + '], "actual_time": [' + actual_time + '], ' +
                '"idx_scalar": [' + d[1] + '], "actual_scalar": [' + actual_scalar + '],' +
                '"idx_threshold": [' + d[0] + '], "actual_threshold": [' + actual_threshold + '],' +
                '"PPI": [' + d[2] + '] }}';
            console.log(backMsg) ;
            if (!Window.PPI['DEV']) {
                Window.socket = socket ;
                socket.send(backMsg) ;
            }

            $("#histogram-notification-placeholder-default").html("") ;
            $("#histogram-notification-placeholder-0").html($("#histogram-notification").attr("data-pattern-0").replace("{Scalar}", actual_scalar.toFixed(2)).replace("{Time}", actual_time).replace("{Threshold}", actual_threshold)) ;
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
        var scale = scaleConvert(d3.select("#sdm-range_input").property("value"))

        var trans = transFormApply($(this).attr("transform"), undefined, undefined, undefined, true) ;
        var x = trans[0] ;
        var y = trans[1] ;
        x += d3.event.dx;
        y += d3.event.dy;
        // bins of histogram
        d3.select(this).attr("transform", "translate(" + x + "," + y + ") scale(" + scale + ")" );

        $(".sdm-vertical-column-g").attr("transform", transFormApply($(".sdm-vertical-column-g").attr("transform"), undefined, y, scale)) ;
        $(".sdm-vertical-column-g rect").each(function() {
            $(this).attr("width", parseFloat($(this).attr("ori-width")) / scale) ;
        }) ;
         
        var yTrans = transFormApply($(".sdm-axis--hist--y").attr("transform"), undefined, undefined, undefined, true) ;
        var yX = yTrans[0] ;
        var yY = yTrans[1] ;
        yX += d3.event.dx ;
        yY += d3.event.dy ;    
        var v = transFormApply($(".sdm-axis--hist--y").attr("transform"), 0, yY, scale) ;    
        d3.select(".sdm-axis--hist--y").attr("transform", v);

        var xTrans = transFormApply($(".sdm-axis--hist--x").attr("transform"), undefined, undefined, undefined, true) ;
        var xX = xTrans[0] ;
        var xY = xTrans[1] ;
        xX += d3.event.dx ;
        xY += d3.event.dy ;    
        var v = transFormApply($(".sdm-axis--hist--x").attr("transform"), xX, undefined, scale) ;    
        d3.select(".sdm-axis--hist--x").attr("transform", v);

        if ( $(".sdm-rect-box-cover").length > 0) {
            var coverTrans = transFormApply($(".sdm-rect-box-cover").attr("transform"), undefined, undefined, undefined, true) ;
            var coverX = coverTrans[0] ;
            var coverY = coverTrans[1] ;
            coverX += d3.event.dx;
            coverY += d3.event.dy;
            // bins of histogram
            d3.select(".sdm-rect-box-cover").attr("transform", "translate(" + coverX + "," + coverY + ") scale(" + scale + ")" );
        }
    }

    function dragended(d) {
        d3.select(this).classed("dragging", false);
    }

    var zoom = d3.zoom()
                 .scaleExtent([0.1, 10])
                 .on("zoom", zoomed)
                 .on("start", zoomstart)
                 .on("end", zoomend) ;

    var slider = d3.select("#sdm-range_input")
        .datum({})
        .attr("value", 10)
        .attr("min", 1)
        .attr("max", 20)
        .attr("step", 0.1)
        .on("input", slided);

    function callFunc() { // most time-consuming part, avg time is 1.5 seconds
        main.call(drag).call(zoom);
    }
    setTimeout(callFunc, 0) ;

    // Add y-axis title
    svg.append("text")
        .attr("class", "sdm-hist-yaxis-title")
        .attr("transform", "rotate(-90)")
        .attr("y", 0 - 62 )
        .attr("x", 0 - (fixedHeight / 2))
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Scalar") ;

    // dd x-axis title
    svg.append("text")
        .attr("class", "sdm-hist-xaxis-title")
        .attr("y", fixedHeight + 24)
        .attr("x", fixedWidth / 2.5 + 110)
        .attr("z-index", 100)
        .attr("dy", "1em")
        .style("text-anchor", "middle")
        .text("Threshold");

    // reset x-axis, y-axis, TRICKY
    replaceTicks(".sdm-axis--hist--x g", getThresholdArray(), interval=Window.PPI['thresholdRatio']) ;
    removeNiceByKicks(".sdm-axis--hist--x g", keep=-1) ;
    replaceTicks(".sdm-axis--hist--y g", getScalarArray()) ;
    removeNiceByKicks(".sdm-axis--hist--y g") ;

    d3.selectAll(".sdm-axis--hist--y path").each(function() {
        d3.select(this).remove() ;
    }) ;

    d3.selectAll(".sdm-axis--hist--x path").each(function() {
        d3.select(this).remove() ;
    }) ;

    function zoomed() { }

    function zoomstart() { } 

    function slided(d) {
        zoom.scaleTo(svg, scaleConvert(d3.select(this).property("value")));
    }

    $("#sdm-histogram_zoom_back").unbind().click(function() {
        zoomend("zoom_back") ;
    }) ;

    function zoomend(d) {
        var currentTransform ;
        console.log("mouse position: " + cx + ", " + cy) ;
        if (d === "zoom_back") {
            currentTransform = {"x":0, "y":(fixedHeight - height), "k":1}
        } else {
            var preTmp = transFormApply(d3.select(this).attr("transform"), undefined, undefined, undefined, true) ;
            var curTmp = d3.event.transform ;
            newTx = cx - (cx - preTmp[0]) * curTmp.k / preTmp[2];
            newTy = cy - (cy - preTmp[1]) * curTmp.k / preTmp[2];
            currentTransform = {"x":newTx, "y":newTy, "k":curTmp.k}
        }

        main.attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");

        $(".sdm-vertical-column-g").attr("transform", transFormApply($(".sdm-vertical-column-g").attr("transform"), undefined, currentTransform.y, currentTransform.k)) ;
        $(".sdm-vertical-column-g rect").each(function() {
            $(this).attr("width", parseFloat($(this).attr("ori-width")) / currentTransform.k) ;
        }) ;

        d3.select(".sdm-rect-box-cover").attr("transform", "translate(" + currentTransform.x+"," +currentTransform.y+ ") scale(" + currentTransform.k + ")");

        d3.select(".sdm-axis--hist--y").attr("transform", "translate(0,"+currentTransform.y+") scale("+currentTransform.k+")");
        d3.select(".sdm-axis--hist--x").attr("transform", "translate("+currentTransform.x+","+fixedHeight+") scale("+currentTransform.k+")");

        d3.selectAll(".sdm-axis--hist--y g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, 1 / currentTransform.k)) ;
        }) ;

        d3.selectAll(".sdm-axis--hist--x g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, 1 / currentTransform.k)) ;
        }) ;

        slider.property("value", scaleReverse(currentTransform.k));
    }

    coverShadow() ;
    drawVerticalColumn() ;

    $("#hidden-sdm-draw-vertical-column-lonely").unbind().click(function() {
        drawVerticalColumn() ;
    })

    $("#hidden-sdm-move-prev").unbind().click(function() {
        $('#hist-threshold option:selected').prev().prop('selected', true) ;
        $('#threshold-window option:selected').prev().prop('selected', true) ;
        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

    $("#hidden-sdm-draw-legend-lonely").unbind().click(function() {
        drawLegend(dist_data, min_persistence_pairs, max_persistence_pairs) ;
    })

    $("#hidden-sdm-move-next").unbind().click(function() {
        $('#hist-threshold option:selected').next().prop('selected', true) ;
        $('#threshold-window option:selected').next().prop('selected', true) ;
        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

    $("#hidden-sdm-move-right-prev").unbind().click(function() {
        $('#threshold-window option:selected').prev().prop('selected', true) ;
        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

    $("#hidden-sdm-move-right-next").unbind().click(function() {
        $('#threshold-window option:selected').next().prop('selected', true) ;
        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

    $("#sdm-histogram_zoom_back").click() ;

    // replay previous settings
    if (sdm_hist_clip_g_transform) {
        $("#sdm-hist-clip-g").attr("transform", sdm_hist_clip_g_transform) ;
        var clip_g = transFormApply(sdm_hist_clip_g_transform, undefined, undefined, undefined, true) ;
        $(".sdm-vertical-column-g").attr("transform", transFormApply($(".sdm-vertical-column-g").attr("transform"), undefined, clip_g[1], clip_g[2])) ;
        $(".sdm-vertical-column-g rect").each(function() {
            $(this).attr("width", parseFloat($(this).attr("ori-width")) / clip_g[2]) ;
        }) ;
        slider.property("value", scaleReverse(clip_g[2]));
    }
    if (sdm_rect_box_cover_transform) {
        $(".sdm-rect-box-cover").attr("transform", sdm_rect_box_cover_transform);
    }
    if (sdm_axis_hist_y) {
        $(".sdm-axis--hist--y").attr("transform", sdm_axis_hist_y) ;
        
    }
    if (sdm_axis_hist_x) {
        $(".sdm-axis--hist--x").attr("transform", sdm_axis_hist_x) ;
    }

    if (sdm_axis_hist_y_g) {
        var y_g_trans = transFormApply(sdm_axis_hist_y_g, undefined, undefined, undefined, true) ;
        d3.selectAll(".sdm-axis--hist--y g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, y_g_trans[2])) ;
        }) ;
    }

    if (sdm_axis_hist_x_g) {
        var x_g_trans = transFormApply(sdm_axis_hist_x_g, undefined, undefined, undefined, true) ;
        d3.selectAll(".sdm-axis--hist--x g").each(function() {
            d3.select(this).attr("transform", transFormApply(d3.select(this).attr("transform"), undefined, undefined, x_g_trans[2])) ;
        }) ;
    }

    if (Window.PPI['selected-bin-id-sdm']) {
        d3.select("#" + Window.PPI['selected-bin-id-sdm']).dispatch("click") ;
    }

    $("#sdm-histogram_viz").loading("stop");
}

$("#hist-time").change(function () {
    $("#hist-time option:selected").each(function () {
        Window.PPI['selected-time-id'] = parseInt($(this).val()) ; 
        drawSDMHistogram(Window.PPI['DEV']? null: (ttk ? ttk.getSocketObject(): null)); 
    });
});