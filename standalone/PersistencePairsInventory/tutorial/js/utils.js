
function getMaxPersistencePairs() {
    // data: image-object => PointData => PersistencePairInventory
    // get the max PPI at iComponents
    let iComponent = parseInt($("#hist-threshold").val()) ;
    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    let nComponents = Window.PPI['numberOfThreshold-histogram'] ;

    if ( Window.PPI["max_persistence_pairs"].hasOwnProperty(iComponent) ) {
        return Window.PPI["max_persistence_pairs"][iComponent] ;
    } 
    var max_persistence_pairs = 0 ;
    for (let i = 0; i < data.length; i++) {
        if (i % nComponents == iComponent && data[i] > max_persistence_pairs) {
            max_persistence_pairs = data[i] ;
        }
    }
    Window.PPI["max_persistence_pairs"][iComponent] = max_persistence_pairs ;
    return max_persistence_pairs
}

// items: the # of bins over threshold, in the test dataset, it would be 50
// iComponent: the start index of calculating, the range of the calculated window would be [iCompent, iCompent + window.size] 
function calculateReliability(items, iComponent, windowSize) {
    var sliceItems = items.slice(iComponent, iComponent + windowSize), sum = 0;
    for (var i = 0; i < sliceItems.length; i++) {
        sum += sliceItems[i];
    }
    if (Math.max(...sliceItems) === 0) {
        return 1;
    }
    // get the multiple trpezoid area
    return ( 2 * sum - sliceItems[0] - sliceItems[sliceItems.length - 1] ) / ( ( sliceItems.length - 1 )  * Math.max(...sliceItems) * 2 );
}

function addSvgLine(svg, x1, y1, x2, y2, transform, className="zero", stroke="black", stroke_width=1) {
    svg.append("line")
        .attr("class", className)
        .attr("x1", x1)
        .attr("y1", y1)
        .attr("x2", x2)
        .attr("y2", y2)
        .style("stroke", stroke)
        .style("stroke-width", stroke_width)
        .attr("transform", transform);
}

function getArray(n, ratio=1) {
    let ans = [];
    for (let i = 0; i < n; i++) { ans.push("" + ( i * ratio)); }
    return ans;
}

function removeNiceByKicks(id, keep=-1, ratio=0) {
    var gs = $(id) ;
    if (keep === 0) {
        gs.remove() ;
        return ;
    } 
    var size = 0 ;
    var i = 0 ;
    if (keep > 0) {
        size = Math.floor(gs.length / keep) ;
        for (i = 0; i < gs.length; i ++) {
            if (i % size !== 0 && i !== gs.length - 1) {
                gs[i].remove() ;
            } else {
                if (ratio > 0) {
                    var tc = $(gs[i].getElementsByTagName("text")[0]) ;
                    tc.text(Number.parseFloat(ratio * parseInt(tc.text())).toFixed(1)) ;
                }
            }
        }
    } else {
        if (gs.length >= 40) {
            size = Math.floor(gs.length / 20) ;
            for (i = 0; i < gs.length; i ++) {
                if (i % size !== 0 && gs.length - 1 !== i) {
                    gs[i].remove() ;
                } else {
                    if (ratio > 0) {
                        var tc = $(gs[i].getElementsByTagName("text")[0]) ;
                        tc.text(Number.parseFloat(ratio * parseInt(tc.text())).toFixed(1)) ;
                    }
                }
            }
        }
    }
}

function triggerEnterInput(selector) {
    var e = jQuery.Event("keypress");
    e.which = 13; //choose the one you want
    e.keyCode = 13;
    $(selector).trigger(e);
}


/**
 * return the color
 * @param {*} reliability: three regions, [0, lower), [lower, upper), [upper, 1]
 * @param {*} ppi: # of features
 * @param {*} max_persistence_pairs
 * @param {*} lower
 * @param {*} upper
 * @param default_color
 */
function customColor(reliability, ppi, max_persistence_pairs, default_color="#ffffff") {
    //return customColorV2(reliability, ppi, max_persistence_pairs) ;
    // from light to dark
    let lower = getFloatValue("#histogram_viz_legend", "data-x_0"),
        upper = getFloatValue("#histogram_viz_legend", "data-x_1") ;
    let range_color = Window.PPI['color-green'];
    if (reliability >= 0 && reliability < lower) {
        range_color = Window.PPI['color-red'];
    } else if (reliability >= lower && reliability < upper) {
        range_color = Window.PPI['color-gray'];
    } else {
        range_color = Window.PPI['color-green'];
    }

    if (ppi === 0) {  // special for PPI == 0
        return default_color ;
    } else {
        if (Window.PPI['max_persistence_pairs']['is_custom']) {
            let lower = Window.PPI['max_persistence_pairs']['custom_lower']
            let upper = Window.PPI['max_persistence_pairs']['custom_upper']
            if ( ppi <= lower ) {
                return range_color[0]
            } else if ( upper ) {
                return range_color[4]
            } else {
                return range_color[Math.ceil((ppi - lower) / ( (max_persistence_pairs - lower) / 5)) - 1];    
            }
        } else {
            // (a, b]
            return range_color[Math.ceil(ppi / (max_persistence_pairs / 5)) - 1];
        }
    }
}

function getFloatValue(id, attr) {
    return parseFloat($(id).attr(attr)) ;
}