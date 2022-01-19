// move this file to the root html directory
// change const's EXURL & PASSURL
var KEY;
const PASSURL="https://ishtar.sao.ru/pass";
const EXURL = "https://ishtar.sao.ru/cgi-bin/auth";
function $(id){
	return document.getElementById(id);
}
function checkcookie(){
	var txt = document.cookie;
	if(txt.length==0 || txt.indexOf('KEY')<0){
		$("inout").innerHTML = "Войти";
		 return 0;
	}
	else{
		$("inout").innerHTML = "Выйти";
		return 1;
	}
}
function getcookie(){
/*	без аргументов - для текущей страницы,
	каждый аргумент - доп. "печенька"
*/
	var i, newurl = PASSURL+"?URL="+document.location.href;
	for(i = 0; i < getcookie.arguments.length; i++)
		newurl += "&URL=" + getcookie.arguments[i];
	if(!checkcookie())
		document.location.href = newurl;
}
function onEX(){
	var d = new Date();
	d.setTime(d.getTime() - 1000);
	var str = "KEY=; expires="+d.toGMTString()+"; path="+document.location.pathname;
	document.cookie = str;
	window.location.reload();
}
function exit(){
	var request = new XMLHttpRequest();
	request.open("POST", EXURL, true);
	request.setRequestHeader("Accept-Charset", "koi8-r");
	request.setRequestHeader("Cookie", document.cookie);
	request.overrideMimeType("multipart/form-data; charset=koi8-r"); 
	request.onreadystatechange=function(){
		if (request.readyState == 4){
			if (request.status == 200){
				onEX();
			}
			else alert("Ошибка соединения");
		}
	}
	request.send("")
}
function inout(){
	if(checkcookie()) exit();
	else getcookie();
}
