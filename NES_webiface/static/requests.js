const Debug = true;

var elementsCache = {};
function $(id) {
    if (elementsCache[id] === undefined)
        elementsCache[id] = document.getElementById(id);
    return elementsCache[id];
}

function dbg(text){
	if(Debug) console.log("Debug message: " + text);
}

function sendrequest(req_STR, onOK, postdata){
	var request = new XMLHttpRequest(), timeout_id;
	dbg("send request " + req_STR);
	var method = postdata ? "POST" : "GET";
	request.open(method, req_STR, true);
	//request.setRequestHeader("Accept-Charset", "koi8-r");
	//request.overrideMimeType("multipart/form-data; charset=koi8-r"); 
	request.onreadystatechange=function(){
		if(request.readyState == 4){
			if(request.status == 200){
				clearTimeout(timeout_id);
				if(onOK) onOK(request);
			}
			else{
				clearTimeout(timeout_id);
				parseErr("request sending error");
			}
		}
	}
	request.send(postdata);
	timeout_id = setTimeout(function(){request.onreadystatechange=null; request.abort(); parseErr("request timeout");}, 5000);
}

function parseErr(txt){
	console.log("Error: " + txt);
	var msgDiv = $('errmsg');
	if(!msgDiv) return;
	msgDiv.innerHTML = "Error: " + txt;
	setTimeout(function(){msgDiv.innerHTML = "";}, 3500);
}
