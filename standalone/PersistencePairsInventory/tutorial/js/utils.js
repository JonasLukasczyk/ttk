function getArray(n, ratio=1) {
    let ans = [];
    for (let i = 0; i < n; i++) { ans.push("" + ( i * ratio)); }
    return ans;
}

function removeNiceByKicks(id, keep=-1) {
    var gs = $(id) ;
    if (keep == 0) {
        gs.remove() ;
        return ;
    } 

    if (keep > 0) {
        var size = Math.floor(gs.length / keep) ;
        for (var i = 0; i < gs.length ; i ++) {
            if (i % size != 0) {
                gs[i].remove() ;
            }
        }
    } else {
        if (gs.length >= 40) {
            var size = Math.floor(gs.length / 20) ;
            for (var i = 0; i < gs.length; i ++) {
                if (i % size != 0) {
                    gs[i].remove() ;
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