<!--
// frame_adjust.js
function frame_adjust(frm) {
	if(document.getElementById(frm)) {
		var elemFrm = document.getElementById(frm);
		var elemDoc = elemFrm.contentWindow.document.documentElement;
		var sHgt = elemDoc.scrollHeight;
		var sWdt = elemDoc.ScrollWidth;
		if (elemDoc.offsetHeight > sHgt) {
			sHgt = elemDoc.offsetHeight;
		}
		if (elemDoc.offsetWidth > sWdt) {
			sWdt = elemDoc.offsetWidth;
		}
		elemFrm.style.height = sHgt+"px";
		elemFrm.style.width = sWdt+"px";
	}
}
window.onload = function() {
	frame_adjust('wbuild');
};
-->
