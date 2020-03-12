function trim(v, lower, higher) {
    if (typeof v === "object") {
        var ans = [] ;
        for (var i = 0; i < v.length; i ++) {
            ans.push(trim(v[i], lower, higher)) ;
        }
        return ans ;
    }
    return Math.max(Math.min(v, higher), lower)
}

// 1 - 20 => (0.1 - 1) and (1 - 10)
function scaleConvert(u) {
    if ( u < 1 || u > 20 ) {
        alert("error of the range: " + u) ; // should not happen
    }
    if ( u < 10 ) {
        return u / 10 ;
    } else if ( u > 10 ) {
        return u - 10 ;
    } else {
        return 1 ;
    }
}

// 1 - 20 <= (0.1 - 1) and (1 - 10)
function scaleReverse(v) {
    if ( v < 0.1 || v > 10 ) {
        alert("error of the range: " + v) ;
    }
    if ( v < 1 ) {
        return v * 10 ;
    } else if ( v > 1 ) {
        return v + 10 ;
    } else {
        return 10 ;
    }
}

function getHistogramFrameData(min_persistence_pairs, left_threshold, right_threshold, max_persistence_pairs, nComponents) {
    var iComponent = left_threshold ;
    var max_threshold_window = right_threshold ;
    var vData = [] ;
    var key = min_persistence_pairs + ":" + left_threshold + ":" + right_threshold + ":" + max_persistence_pairs ;
    if ( Window.PPI['histogram_frame_data'].hasOwnProperty(key) ) {
        return Window.PPI['histogram_frame_data'][key] ;
    }

    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    var t0 = performance.now()
    for (let i = 0; i < Window.PPI['histogram-height']; i++) {
        for (let j = 0; j < Window.PPI['histogram-width']; j++) {
            let idx = (i * Window.PPI['histogram-width'] + j) * nComponents + iComponent;
            let items = data.slice((i * Window.PPI['histogram-width'] + j) * nComponents, (i * Window.PPI['histogram-width'] + j) * nComponents + nComponents);
            items = trim(items, min_persistence_pairs, max_persistence_pairs) ;
            let cal = calculateReliability(items, iComponent, max_threshold_window, min_persistence_pairs) ;
            // (x-axis, y-axis, PPI, tuples, idx in the entire data, reliability)
            vData.push([j + "", i + "", data[idx], items, idx, cal]);
        }
    }
    var t1 = performance.now() ;
    Window.PPI['histogram_frame_data'][key] = vData ;
    console.log("retrieve related data for rendering histogram took " + (t1 - t0) + " milliseconds.") 
    return vData ;
}

function getLegendData(vData) {
    var tmp = getRangeOfPPI()
    min_persistence_pairs = tmp[0]
    max_persistence_pairs = tmp[1]
    var dist_data = [] ;
    for (var i = 0; i < vData.length; i ++) {
        if (vData[2] != 0) {
            dist_data.push([vData[i][5], trim(vData[i][2], min_persistence_pairs, max_persistence_pairs)]) ;
        }
    }
    return dist_data ;
}


function transFormApply(u, x=-1000000, y=-1000000, k=-1000000, val=false) {
    var xx, yy, kk ;
    if ( u.indexOf("translate") === -1 ) {
        xx = -1000000 ;
        yy = -1000000 ;
        kk = -1000000 ;
    } else {
        var vItems = u.split(" scale(") ;
        var trans = vItems[0].split(",") ;
        xx = parseFloat(trans[0].replace("translate(", "")) ;
        yy = parseFloat(trans[1].replace(")", "")) ;
        kk = -1000000 ;
        if (vItems.length > 1) {
            kk = parseFloat(vItems[1].replace(")", "")) ;
        }
    }
    
    xx = x == -1000000 ? xx: x ;
    yy = y == -1000000 ? yy: y ;
    kk = k == -1000000 ? kk: k ;

    if (val) {
        return [xx, yy, kk] ;
    }

    var ans = "" ;
    if (xx != -1000000 && yy != -1000000) {
        ans += "translate(" + [xx, yy] + ")" ;
    } 

    if (kk != -1000000) {
        ans += " scale(" + kk + ")" ;
    }

    return ans ;
}

function getRangeOfPPIByThreshold() {
    // data: image-object => PointData => PersistencePairInventory
    // get the max PPI at iComponents
    let iComponent = parseInt($("#hist-threshold").val()) ;
    let data = Window.PPI['image-object']['PointData'][Window.PPI['APPIAttrName']].Values ;
    let nComponents = Window.PPI['numberOfThreshold-histogram'] ;

    if ( Window.PPI["persistence_pairs_range"].hasOwnProperty(iComponent) ) {
        return Window.PPI["persistence_pairs_range"][iComponent] ;
    } 
    var max_persistence_pairs = 0, min_persistence_pairs = Number.MAX_SAFE_INTEGER ;
    for (let i = 0; i < data.length; i++) {
        if (i % nComponents == iComponent ) {
            if( data[i] > max_persistence_pairs) {
                max_persistence_pairs = data[i] ;
            }

            if ( data[i] < min_persistence_pairs) {
                min_persistence_pairs = data[i] ;
            }
        }
    }
    Window.PPI["persistence_pairs_range"][iComponent] = [min_persistence_pairs, max_persistence_pairs] ;
    return [min_persistence_pairs, max_persistence_pairs]
}

function getRangeOfPPI() {
    if (Window.PPI['persistence_pairs_range']['is_custom']) {
        return [Window.PPI['persistence_pairs_range']['custom_lower'], Window.PPI['persistence_pairs_range']['custom_upper']] ;
    } else {
        return getRangeOfPPIByThreshold()
    }
}

// items: the # of bins over threshold, in the test dataset, it would be 50
// iComponent: the start index of calculating, the range of the calculated window would be [iCompent, iCompent + window.size] 
function calculateReliability(items, iComponent, max_threshold_window, min_persistence_pairs) {
    var sliceItems = items.slice(iComponent, max_threshold_window + 1), sum = 0;
    var vBase = min_persistence_pairs ;
    for (var i = 0; i < sliceItems.length; i ++) {
        sliceItems[i] = sliceItems[i] - vBase ;
    }

    if (Math.max(...sliceItems) === 0 || sliceItems.length === 1) {
        return 1 ;
    }

    for (var i = 0; i < sliceItems.length; i++) {
        sum += sliceItems[i];
    }
    // get the multiple trpezoid area
    let rel = ( 2 * sum - sliceItems[0] - sliceItems[sliceItems.length - 1] ) / ( ( sliceItems.length - 1 )  * Math.max(...sliceItems) * 2 );
    return rel;
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
                       // tc.text(Number.parseFloat(ratio * parseInt(tc.text())).toFixed(1)) ;
                       tc.text(Number.parseInt(ratio * parseInt(tc.text()))) ;
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
 */
function customColor(reliability, ppi=0, default_color="#ffffff", index = -1, grayReplaceBlue=false, useClass=false) {
    // from light to dark
    let left = getFloatValue("#histogram_viz_legend", "data-x_0"),
        right = getFloatValue("#histogram_viz_legend", "data-x_1") ;
    let range_color = Window.PPI['color-green'];
    let prefix = "color-green-" ;
    if (reliability >= 0 && reliability < left) {
        range_color = Window.PPI['color-red'];
        prefix = "color-red-" ;
    } else if (reliability >= left && reliability < right) {
        if (grayReplaceBlue) {
            range_color = Window.PPI['color-blue'] ;
            prefix = "color-blue-" ;
        } else {
            range_color = Window.PPI['color-gray'];
            prefix = "color-gray-" ;
        }
    } else {
        range_color = Window.PPI['color-green'];
        prefix = "color-green-" ;
    }

    if (index != -1) {
        if (useClass) {
            return prefix + index ;
        }
        return range_color[index] ;
    }

    if (ppi === 0) {  // special for PPI == 0
        if (useClass) {
            return "color-default" ;
        }
        return default_color ;
    } else {
        tmp = getRangeOfPPI()
        let lower = tmp[0] ;
        let upper = tmp[1] ;
        ppi = Math.min(upper, ppi)
        ppi = Math.max(lower + 0.01, ppi)
        var idx = Math.ceil((ppi - lower) / ( (upper - lower) / 5)) - 1 ;
        if (useClass) {
            return prefix + idx ;
        }
        return range_color[idx];    
    }
}

function getFloatValue(id, attr) {
    return parseFloat($(id).attr(attr)) ;
}