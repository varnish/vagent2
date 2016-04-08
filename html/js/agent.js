var agent = {
	/*
	 * Keep 3 stats objects.
	 */
	stats:new Array(),
	/*
	 * Update them X times a second, useful to increase this during
	 * debugging to avoid flooding yourself. Can't be reliably set at
	 * run time since the timers will remain unchanged.
	 */
	statsInterval: 1,
	params:null,
	/*
	 * Used to have a central way to transmit "output".
	 * Use out_clear() when starting out, add stuff to agent.out, then
	 * on completion use out_up() to show the message(s).
	 */
	out:"Initializing frontend",
	vcl:"",
	vclId:"",
	vclList:null,
	activeVcl:"",
	debug: true,
	/*
	 * Global AJAX timeout (in milliseconds)
	 */
	globaltimeout: 5000,
	version: "",
	varnishtoplength: 5
};

function urlPrefix() {
    var pattern = /\/html\//;
    var index = window.location.href.match(pattern).index;
    var prefix = window.location.href.substr(0, index);
    return prefix;
}

function clog(text)
{
	if(agent.debug) {
		console.log(text);
	}
}

function assert(val)
{
	if (!(val)) {
		throw "Assertion failure";
	}
}

function assertText(val)
{
	assert(val != null);
	assert(val.length > 0);
}

function out_clear()
{
	document.getElementById("out").innerHTML = "";
	agent.out = "";
}

function out_up()
{
	document.getElementById("out").innerHTML = agent.out;
}

function topActive(head)
{
	var navs = new Array('nav-home', 'nav-vcl', 'nav-param', 'nav-backend', 'nav-varnishstatview');
	assertText(head);

	for (x in navs) {
		document.getElementById(navs[x]).className = "";
	}
	document.getElementById("nav-" + head).className = "active";
}

function showVCL()
{
	document.getElementById("params").style.display = "NONE";
	document.getElementById("home").style.display = "NONE";
	document.getElementById("vcl").style.display = "block";
    document.getElementById("backend").style.display = "NONE";
	document.getElementById("varnishstatview").style.display = "NONE";
	document.getElementById("mainblock").style.display = "block";
	stopVarnishstat();
	topActive("vcl");
}

function showHome()
{
	document.getElementById("params").style.display = "NONE";
	document.getElementById("home").style.display = "block";
	document.getElementById("vcl").style.display = "NONE";
    document.getElementById("backend").style.display = "NONE";
	document.getElementById("varnishstatview").style.display = "NONE";
	document.getElementById("mainblock").style.display = "block";
	stopVarnishstat();
	topActive("home");
}

function showBackend()
{
    document.getElementById("params").style.display = "NONE";
    document.getElementById("home").style.display = "NONE";
    document.getElementById("vcl").style.display = "NONE";
    document.getElementById("backend").style.display = "block";
	document.getElementById("varnishstatview").style.display = "NONE";
	document.getElementById("mainblock").style.display = "NONE";
        topActive("backend");
}

function showVarnishstat()
{
	document.getElementById("params").style.display = "NONE";
	document.getElementById("home").style.display = "NONE";
	document.getElementById("vcl").style.display = "NONE";
    document.getElementById("backend").style.display = "NONE";
	document.getElementById("varnishstatview").style.display = "block";
	document.getElementById("mainblock").style.display = "NONE";
	topActive("varnishstatview");
	startVarnishstat();
}

function showParam()
{
	document.getElementById("vcl").style.display = "NONE";
	document.getElementById("home").style.display = "NONE";
	document.getElementById("params").style.display = "block";
    document.getElementById("backend").style.display = "NONE";
	document.getElementById("varnishstatview").style.display = "NONE";
	document.getElementById("mainblock").style.display = "block";
	stopVarnishstat();
	list_params();
	topActive("param");
}

function reset_status()
{
	var but = document.getElementById("status-btn");
	assert(but != null);
	var stat;
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/status",
		dataType: "text",
		timeout: agent.globaltimeout,
		success: function (data, textStatus, jqXHR) {
			stat = data;
			assertText(stat);
			but.textContent = stat;
		},
		error: function( jqXHR, textStatus, errorThrown) {
			stat = "Error communicating with agent. " + errorThrown;
			clog("error: "+stat);
			clog(textStatus);
			clog(jqXHR);
		},
		complete: function( jqXHR, textStatus) {
   			but.textContent = stat;
			if (stat == "Child in state running") {
				but.className = "btn btn-primary btn-block disabled";
			} else {
				but.className = "btn btn-danger btn-block disabled";
			}
		}
	});
}

function show_status(state,message)
{
	assertText(state);
	assertText(message);
	but = document.getElementById("status-btn");
	if (state == "ok")
		but.className = "btn btn-success btn-block disabled";
	if (state == "warn")
		but.className = "btn btn-warning btn-block disabled";
	if (state == "error")
		but.className = "btn btn-danger btn-block disabled";
	but.textContent = message;
	setTimeout(function() { reset_status(); }, 3000);
}

function uploadVCL()
{
	var id = document.getElementById("vclID").value;
	var vcl = document.getElementById("vcl-text").value;
	assertText(vcl);
	assertText(id);
   	out_clear();
	$.ajax({
		type: "PUT",
		url: urlPrefix() + "/vcl/" + id,
		timeout: agent.globaltimeout,
		contentType: "application/xml",
		data: vcl,
		success: function (data, textStatus, jqXHR) {
			agent.out = jqXHR.responseText;
		},
		error: function( jqXHR, textStatus, errorThrown) {
				agent.out = "Com. errors with agent: \n" + jqXHR.responseText;
		},
		complete: function( jqXHR, textStatus) {
			out_up();
			if( jqXHR.status == 201) {
				show_status("ok","VCL stored");
			} else {
				show_status("warn", "VCL save failed");
			}
			listVCL();
		}
	});
}

function loadVCL()
{
	out_clear();
	var id = document.getElementById("loader").value;
	assertText(id);
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/vcl/" + id,
		timeout: agent.globaltimeout,
		success: function (data, textStatus, jqXHR) { },
		error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Com. errors with agent: \n" + errorThrown;
			clog("error");
			clog(errorThrown);
			clog(textStatus);
			clog(jqXHR);
		},
		complete: function( jqXHR, textStatus) {
			if( jqXHR.status == 200) {
				agent.vcl = jqXHR.responseText;
				agent.vclId = id;
				show_status("ok", "VCL loaded");
				document.getElementById("vcl-text").value = agent.vcl;
				document.getElementById("vclID").value = agent.vclId;
			} else {
				agent.out = jqXHR.responseText;
				show_status("warn","VCL load failed");
			}
			out_up();
		}
	});
}


function listVCL()
{
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/vcljson/",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			var vclList = JSON.parse(data);
			agent.vclList = vclList;
			var txt = "";
			for (x in agent.vclList["vcls"]) {
				v = agent.vclList["vcls"][x];
				if (v.status == "discarded") {
					continue;
				}
				if (v.status == "active") {
					agent.activeVcl = v.name;
					document.getElementById("vcl-btn").innerHTML = "Active VCL: " + v.name;
				}
				txt = txt + "\n" + "<option>" + v.name + "</option>";
			}
			var d = document.getElementById("loader");
			d.innerHTML = txt;
			if (document.getElementById("vcl-text").value == "") {
				d.value = agent.activeVcl;
				document.getElementById("vclID").value = agent.activeVcl;
				loadVCL();
			}
		},
		error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Listing VCL failed.";
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		}
	});
}

function list_params()
{
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/paramjson/",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			if( jqXHR.status == 200) {
				var paramlist = JSON.parse(jqXHR.responseText);
				agent.params = paramlist;
				var list = document.getElementById("param-sel");
				var v = list.value;
				var arry = new Array();
				for (x in agent.params) {
					arry.push(x);
				}
				arry.sort();
				list.options.length = 0;
				for (x in arry) {
					list.add(new Option(arry[x]));
				}
				if (v)
					list.value = v;
				paramChange();
			}
		},
		error: function( jqXHR, textStatus, errorThrown) {
			clog("error");
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
			out_clear();
			agent.out = "Error listing parameters.\n";
			agent.out += "Communication error with agent: \n";
			agent.out += errorThrown;
		},
		complete: function( jqXHR, textStatus) {
			if (jqXHR.status != 200) {
				agent.out += "Varnish-Agent returned " +
					jqXHR.status + ":" + jqXHR.responseText;
			}
			out_up();
		}
	});

}

function paramChange()
{
	var pname = document.getElementById("param-sel").value;
	assertText(pname);
	document.getElementById("param-val").value = agent.params[pname].value;
	out_clear();
	agent.out =  "Default: " + agent.params[pname].default + "\n";
	agent.out += "Value:   " + agent.params[pname].value + "\n";
	agent.out += "Unit:    " + agent.params[pname].unit + "\n";
	agent.out += agent.params[pname].description;
	out_up();
}

function paramListDiff()
{
	out_clear();
	agent.out = "Non-default parameters:\n\n";
	assert(agent.params != null);
	for (x in agent.params) {
		if (agent.params[x].unit == "seconds" || agent.params[x].unit == "s" || agent.params[x].unit == "bitmap") {
			var one = Number(agent.params[x].value);
			var two = Number(agent.params[x].default);
			if (one != two) {
				agent.out +=  x + "(";
				agent.out += one + " vs ";
				agent.out += two + ")\n";
			}
		} else if (agent.params[x].unit == "bytes") {
			var one = agent.params[x].value == "-1" ? "unlimited" : agent.params[x].value;
			var two = agent.params[x].default == "-1" ? "unlimited" : agent.params[x].default;
			if (one != two) {
				agent.out += x + "(";
				agent.out +=  one + " vs ";
				agent.out +=  two + ")\n";
			}

		} else if (agent.params[x].value != agent.params[x].default) {
			agent.out += x + "(";
			agent.out += agent.params[x].value + " vs ";
			agent.out += agent.params[x].default + ")\n";
		}
	}
	out_up();
}

function saveHealth()
{
    var tuple = new Array();
    var keyVal = new Array();
    var pkeyval = document.getElementById("name-health").value;
    tuple=pkeyval.split(",");
    out_clear();
    for(x in tuple) {
        keyVal = tuple[x].split(":");
        $.ajax({
            type: "PUT",
            url: urlPrefix() + "/backend/"+keyVal[0],
            timeout: agent.globaltimeout,
            contentType: "application/xml",
            data: keyVal[1],
            complete: function( jqXHR, textStatus) {
                agent.out = jqXHR.responseText;
                out_up();
                if (jqXHR.status == 200) {
                    show_status("ok","Admin status saved");
                    list_backends();
                } else {
                    show_status("warn","Couldn't save admin status");
                }
            }
        });
        x++;
    }
}

function saveParam()
{
	var pname = document.getElementById("param-sel").value;
	var pval = document.getElementById("param-val").value;
	assertText(pname);
	assertText(pval);
	out_clear();

	$.ajax({
		type: "PUT",
		url: urlPrefix() + "/param/"+pname,
		timeout: agent.globaltimeout,
		contentType: "application/xml",
		data: pval,
		complete: function( jqXHR, textStatus) {
			agent.out = jqXHR.responseText;
			out_up();
			if (jqXHR.status == 200) {
				show_status("ok","Parameter saved");
				list_params();
			} else {
				show_status("warn","Couldn't save parameter");
			}
		}
	});
}

function setParamDef()
{
	var pname = document.getElementById("param-sel").value;
	var pval = document.getElementById("param-val");
	assertText(pname);
	assertText(agent.params[pname].default);
	pval.value = agent.params[pname].default;
}

function deployVCL()
{
	out_clear();
	var id = document.getElementById("loader").value;
	assertText(id);
	$.ajax({
		type: "PUT",
		url: urlPrefix() + "/vcldeploy/" + id,
		timeout: agent.globaltimeout,
		success: function (data, textStatus, jqXHR) {
			agent.out = jqXHR.responseText;
		},
		error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Com. errors with agent: \n" + jqXHR.responseText;
			clog("error");
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		},
		complete: function( jqXHR, textStatus) {
			if (jqXHR.status == "200") {
				agent.activeVcl = id;
				show_status("ok","VCL deployed");
			} else {
				show_status("warn","vcl deploy failed");
			}
			out_up();
		}
	});
}

function clearID()
{
	document.getElementById("vclID").value = "";
	agent.vclId = "";
}

function discardVCL()
{
	out_clear();
	var id = document.getElementById("loader").value;
	assertText(id);

	$.ajax({
		type: "DELETE",
		url: urlPrefix() + "/vcl/" + id,
		timeout: agent.globaltimeout,
		success: function (data, textStatus, jqXHR) {
			agent.out = jqXHR.responseText;
		},
		error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Com. errors with agent: \n" + jqXHR.responseText;
			clog("error");
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		},
		complete: function( jqXHR, textStatus) {
			if (jqXHR.status = "200" && jqXHR.responseText == "") {
				show_status("ok","VCL discarded");
			} else {
				show_status("warn","VCL discard failed. "+jqXHR.responseText);
			}
			out_up();
		}
	});

}

function stop()
{
	$.ajax({
		type: "PUT",
		url: urlPrefix() + "/stop/",
		timeout: agent.globaltimeout,
		complete: function( jqXHR, textStatus) {
			var doc = jqXHR.responseText;
			document.getElementById("out").innerHTML = doc;
			status();
		}
	});
}

function start()
{
	$.ajax({
		type: "PUT",
		url: urlPrefix() + "/start",
		timeout: agent.globaltimeout,
		complete: function( jqXHR, textStatus) {
			var doc = jqXHR.responseText;
			document.getElementById("out").innerHTML = doc;
			status();
		}
	});

}

function status()
{
	listVCL();
	reset_status();
}

function verify_varnish_stat(version, data) {
	if (data == null)
		return false;
	if( version.indexOf("4.") != -1 )
		return ((data["MAIN.client_req"] !=null)
			|| (data["MAIN.client_req_411"] != null)
			|| (data["MAIN.client_req_413"] != null)
			|| (data["MAIN.client_req_417"] != null)
			|| (data["MAIN.client_req_400"] != null));
	else
		return (data.client_req != null);
}


function calculate_client_req(version, now, previous) {
	if( verify_varnish_stat(version, previous)) {
		if( version.indexOf("4.") != -1 ) {
			return (now["MAIN.client_req"] !== undefined ? now["MAIN.client_req"].value - previous["MAIN.client_req"].value : null)
				+ (now["MAIN.client_req_400"] !== undefined ? now["MAIN.client_req_400"].value - previous["MAIN.client_req_400"].value : null)
				+ (now["MAIN.client_req_411"] !== undefined ? now["MAIN.client_req_411"].value - previous["MAIN.client_req_411"].value : null)
				+ (now["MAIN.client_req_413"] !== undefined ? now["MAIN.client_req_413"].value - previous["MAIN.client_req_413"].value : null)
				+ (now["MAIN.client_req_417"] !== undefined ?  now["MAIN.client_req_417"].value - previous["MAIN.client_req_417"].value : null);
		} else {
			return now.client_req.value - previous.client_req.value;
		}
	}
	else {
		if( version.indexOf("4.") != -1 ) {
			return now["MAIN.client_req"].value
				+ now["MAIN.client_req_400"]
				+ now["MAIN.client_req_411"]
				+ now["MAIN.client_req_413"]
				+ now["MAIN.client_req_417"];
		} else {
			return now.client_req.value;
		}
	}
}

function update_stats()
{
	var d = document.getElementById("stats-btn");
    var version = document.getElementById("agentVersion").innerHTML;
	assert(d != null);
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/stats",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
		    be_bytes(data);
			for (i = 0; i < 3; i++) {
				agent.stats[i] = agent.stats[i+1];
				}
			agent.stats[3] = JSON.parse(data);

			var n_req = 0;
			var n_n_req = 0;
			for (i = 3; agent.stats[i-1] != null; i--) {
				if(!verify_varnish_stat(version, agent.stats[i]))
					break;
				n_req += calculate_client_req(version, agent.stats[i], agent.stats[i-1]);
				n_n_req++;
			}
			if (n_n_req>0)
				d.innerHTML = Number((n_req/n_n_req/agent.statsInterval)).toFixed(0) + "req/s\n";
		},
		error: function( jqXHR, textStatus, errorThrown) {
			d.innerHTML = "Couldn't get stats: " + err;
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		}
	});
}

function varnishtopChange()
{
	agent.varnishtoplength = document.getElementById("varnishtoplength").value;
}
function varnishtopUpdate()
{
	varnishtopChange()
	updateTop()
}

function updateTop()
{
	var tag = document.getElementById("varnishtop-sel").value;
	assertText(tag);
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/log/100/" + tag,
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.rxtop = JSON.parse(data);
			var tmp = "";
			var list = new Object();
			for (var i = 0; i < agent.rxtop.log.length; i++) {
				if (list[agent.rxtop.log[i].value] == null)
					list[agent.rxtop.log[i].value] = 1;
				else
					list[agent.rxtop.log[i].value]++;
			}
			var arr = new Array();
			for (i in list) {
				arr.push({"url":i,"num":list[i]});
			}
			arr.sort(function(a,b) {return b.num - a.num;});
			for (var i=0; i<arr.length && i < agent.varnishtoplength; i++) {
				tmp += arr[i].url + "  (" + arr[i].num + " times)\n";
			}
			var d = document.getElementById("varnishtop");
			d.innerHTML = tmp;
		},
		error: function( jqXHR, textStatus, errorThrown) {
			var d = document.getElementById("out");
			d.innerHTML = "Couldn't get stats: " + errorThrown;
			clog(jqXHR);
			clog(textStatus);
			clog(errorThrown);
		}
	});
}

function banSmart()
{
	var banUrl = document.getElementById("smartBan").value;
	assertText(banUrl);
	$.ajax({
		type: "POST",
		url: urlPrefix() + "/ban" + banUrl,
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = "OK!"+  data;
			out_up();

		},
        error: function( jqXHR, textStatus, errorThrown) {
			agent.out = "Couldn't ban: " + errorThrown;
			out_up();
        }
	});
}

function banList()
{
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/ban",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = data;
			out_up();
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Failed to list!\n" + errorThrown;
			out_up();
		}
	});
}

function panicShow()
{
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/panic",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = data;
			out_up();
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Failed to run panic.show\n" + errorThrown + "\n" + textStatus + "\n" + jqXHR;
			agent.out += jqXHR.responseText;
			if (jqXHR.responseText == "Child has not panicked or panic has been cleared")
				agent.out = jqXHR.responseText;

			clog(jqXHR);
			clog(errorThrown);
			out_up();
		}
	});
}

function panicTest()
{
	$.ajax({
		type: "POST",
		url: urlPrefix() + "/direct",
		data: "debug.panic.worker",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = data;
			out_up();
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Panic command failed\n" + errorThrown + "\n" + textStatus + "\n" + jqXHR;
			agent.out += jqXHR.responseText;

			clog(jqXHR);
			clog(errorThrown);
			out_up();
		}
	});
}
function panicClear()
{
	$.ajax({
		type: "DELETE",
		url: urlPrefix() + "/panic",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.out = data;
			out_up();
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Failed to run panic.clear\n" + errorThrown;
			if (jqXHR.responseText == "No panic to clear")
				agent.out = jqXHR.responseText;
			out_up();
		}
	});
}

function getVersion()
{
	$.ajax({
		type: "GET",
		url: urlPrefix() + "/version",
		timeout: agent.globaltimeout,
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			agent.version = data;
			document.getElementById("agentVersion").innerHTML = agent.version;
		},
		error: function (jqXHR, textStatus, errorThrown) {
			agent.out = "Failed to fetch agent version\n" + errorThrown + "\n" + textStatus + "\n" + jqXHR;
			agent.out += jqXHR.responseText;
			clog(jqXHR);
			clog(errorThrown);
			out_up();
		}
	});
}

function list_backends()
{
    $.ajax({
        type: "GET",
        url: urlPrefix() + "/backendjson/",
        timeout: agent.globaltimeout,
        dataType: "text",
        success: function (data, textStatus, jqXHR) {
            var json = JSON.parse(data);
            var list = document.getElementById("name_backend");
            var arry = new Array();
            var health = new Array();
            var probe = new Array();
            document.getElementById("name_backend").innerHTML= "";
            document.getElementById("health_backend").innerHTML= "";
            document.getElementById("probe_backend").innerHTML= "";

            for (x in json.backends) {
	        arry.push(json.backends[x].name)+'<br />';
                agent.out = arry[x];
                out_be();

                health.push(json.backends[x].admin)+'<br />';
                agent.out = health[x];
                out_health();

                probe.push(json.backends[x].probe)+'<br />';
                agent.out = probe[x];
                out_probe();

            }
        },
        error: function (jqXHR, textStatus, errorThrown) {
            agent.out = "Failed to list!\n" + errorThrown;
            out_be();
        }
    });
}

function be_bytes(data)
{
    var arry = Array();
    var arry2 = Array();
    var json = JSON.parse(data);
    document.getElementById("bereq_backend").innerHTML= "";
    document.getElementById("beresp_backend").innerHTML= "";

    for (x in json.be_bytes) {

        arry.push(json.be_bytes[x].bereq_tot)+'<br />';
        agent.out = arry[x];
        out_bereq();

        arry2.push(json.be_bytes[x].beresp_tot)+'<br />';
        agent.out = arry2[x];
        out_beresp();

    }
}

function backendName()
{
        out_clear();
        agent.out =  + agent.out.backend.name + "\n";
        out_be();
}

function out_bereq()
{
    var tr_0 = document.createElement('tr');
    var td_0 = document.createElement('td');
    td_0.appendChild( document.createTextNode(agent.out) );
    tr_0.appendChild( td_0 );
    document.getElementById("bereq_backend").appendChild( tr_0 );
}

function out_beresp()
{
    var tr_0 = document.createElement('tr');
    var td_0 = document.createElement('td');
    td_0.appendChild( document.createTextNode(agent.out) );
    tr_0.appendChild( td_0 );
    document.getElementById("beresp_backend").appendChild( tr_0 );
}

function out_probe()
{
    var tr_0 = document.createElement('tr');
    var td_0 = document.createElement('td');
    td_0.appendChild( document.createTextNode(agent.out) );
    tr_0.appendChild( td_0 );
    document.getElementById("probe_backend").appendChild( tr_0 );
}

function out_health()
{
    var tr_0 = document.createElement('tr');
    var td_0 = document.createElement('td');
    td_0.appendChild( document.createTextNode(agent.out) );
    tr_0.appendChild( td_0 );
    document.getElementById("health_backend").appendChild( tr_0 );
}



function out_be()
{
    var tr_0 = document.createElement('tr');
    var td_0 = document.createElement('td');
    td_0.appendChild( document.createTextNode(agent.out) );
    tr_0.appendChild( td_0 );
    document.getElementById("name_backend").appendChild( tr_0 );
}

function saveParam()
{
        var pname = document.getElementById("param-sel").value;
        var pval = document.getElementById("param-val").value;
        assertText(pname);
        assertText(pval);
        out_clear();

        $.ajax({
                type: "PUT",
                url: urlPrefix() + "/backend/"+pname,
                timeout: agent.globaltimeout,
                contentType: "application/xml",
                data: pval,
                complete: function( jqXHR, textStatus) {
                        agent.out = jqXHR.responseText;
                        out_up();
                        if (jqXHR.status == 200) {
                                show_status("ok","Parameter saved");
                                list_params();
                        } else {
                                show_status("warn","Couldn't save parameter");
                        }
                }
        });
}


$('.btn').button();
setInterval(function(){status()},10000);
setInterval(function(){update_stats()},agent.statsInterval * 1000);
setInterval(function(){updateTop()},5000);
updateTop();
listVCL();
list_params();
getVersion();
setInterval(function(){list_backends()},1000)
