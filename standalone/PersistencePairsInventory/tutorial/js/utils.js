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

/**
 * return the color 
 * @param {*} reliability: three regions, [0, lower), [lower, upper), [upper, 1] 
 * @param {*} ppi: # of features
 * @param {*} max_persistence_pairs 
 * @param {*} lower 
 * @param {*} upper 
 */
function customColor(reliability, ppi, max_persistence_pairs, lower=0.3, upper=0.6, default_color="#ffffff") {
    //return customColorV2(reliability, ppi, max_persistence_pairs) ;
    // from light to dark
    let range_color = COLOR_GREEN;
    if (reliability >= 0 && reliability < lower) {
        range_color = COLOR_RED;
    } else if (reliability >= lower && reliability < upper) {
        range_color = COLOR_GREY;
    } else {
        range_color = COLOR_GREEN;
    }

    if (ppi == 0) {
        return default_color ;
    } else {
        return range_color[Math.ceil(ppi / (max_persistence_pairs / 5)) - 1];
    }
}