// iComponent: for the time index

function drawSDMHistogram(iComponent, socket) {
    console.log("invoke draw SDMhistogram functionaility") ;
    $("#hist-time").val(iComponent) ;
    iComponent = parseInt(iComponent) ;

    function coverShadow() {
        $(".sdm-rect-cover").remove() ;
        // cover the shadow
        let threshold_idx = parseInt($("#hist-threshold").val()) ;
        let max_threshold_window = parseInt($("#threshold-window").val())
        var idx_first = x(threshold_idx)
        var idx_sec = x(max_threshold_window + 1) ;
        
        svg.append("rect")
            .attr("class", "sdm-rect-cover")
            .attr("x", idx_first)
            .attr("y", 0)
            .attr("height", height)
            .attr("width", idx_sec - idx_first)
            .style("fill", "none")
            .style("stroke-width", 1)
            .style("stroke", "rgb(0,0,0)")
            .attr("transform", "translate(0, 0)");
    }

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

    let containerWidth = 1201;
    let maxWidth = 24 ;
    if ( maxWidth * Window.PPI['sdm-histogram-width'] <= containerWidth ) {
        containerWidth = maxWidth * Window.PPI['sdm-histogram-width'] ;
    }
    let containerHeight = containerWidth * Window.PPI['histogram-height'] / Window.PPI['sdm-histogram-width'];
    if (containerHeight > 724) {
        containerHeight = 724;
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
                .attr("transform",
                    "translate("+margin.left+", "+margin.top+")")
                .attr('overflow', 'hidden');

    let x = d3.scaleBand()
        .range([0, width])
        .domain(myGroups) ;

    svg.append("g")
        .attr('class', 'sdm-axis--hist--x')
        .attr("transform", "translate(0," + height + ")")
        .call(d3.axisBottom(x).ticks(2, "s"));

    let y = d3.scaleBand()
        .range([height, 0])
        .domain(myVars) ;

    svg.append("g")
        .attr('class', 'sdm-axis--hist--y')
        .call(d3.axisLeft(y).ticks(2, "s"));

    let lower = getFloatValue("#histogram_viz_legend", "data-x_0"),
        upper = getFloatValue("#histogram_viz_legend", "data-x_1") ;

    svg.selectAll()
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

    coverShadow() ;
    drawVerticalColumn() ;

    // FLAG, handle the edge case
    $("#hidden-sdm-move-prev").unbind().click(function() {
        $('#hist-threshold option:selected').prev().prop('selected', true) ;
        $('#threshold-window option:selected').prev().prop('selected', true) ;

        coverShadow() ;
        drawVerticalColumn() ;
    }) ;

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
}

$("#hist-time").change(function () {
    $("#hist-time option:selected").each(function () {
        drawSDMHistogram(parseInt($(this).text()), Window.PPI['DEV']? null: ttk.getSocketObject()); 
    });
});