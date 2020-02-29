
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

    if (ppi === 0) {
        return default_color ;
    } else {
        return range_color[Math.ceil(ppi / (max_persistence_pairs / 5)) - 1];
    }
}

function getFloatValue(id, attr) {
    return parseFloat($(id).attr(attr)) ;
}