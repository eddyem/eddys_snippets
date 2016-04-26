;(function () { 'use strict';
	var Pass = {};
	var pwform = null, pw = null;
	var onenter = function(){};
	function retPass(){
		if(!pwform) return;
		var p = pw.value, c;
		while(c = pwform.firstChild) pwform.removeChild(c);
		pwform.parentNode.removeChild(pwform);
		pw = null;
		pwform = null;
		Pass.onenter(p);
	}
	function askPass(){
		pwform = document.createElement("div");
		pwform.style = "width: 100%; height: 100%; position: absolute; top: 0; left: 0; background: lightgray; opacity: 0.75;"
		document.body.appendChild(pwform);
		var d = document.createElement("div");
		d.style = "position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%);";
		d.innerHTML = "<div>Enter password:</div>"
		pw = document.createElement("input");
		pw.setAttribute("type", "password");
		pw.onchange = retPass;
		d.appendChild(pw);
		pwform.appendChild(d);
	}
	Pass.get = askPass;
	Pass.onenter = onenter;
	window.GetPass = Pass;
}());
