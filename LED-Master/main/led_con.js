function set_duty(board_id, led_id) {
	var ele_id = board_id+'_'+led_id;
	var ele = document.getElementById(ele_id);
	var res = document.getElementById("res");
	res.src='duty?led=' + board_id + ',' + led_id + ',' + ele.value;
}

function set_txt(board_id, led_id) {
	var ele_id = board_id+'_'+led_id;
	var ele = document.getElementById(ele_id);
	var txt = document.getElementById('v_' + ele_id);
	txt.innerText = ele.value;
}

function write_one(board_id, led_id) {
	document.write('<input id="'+board_id+'_'+led_id+'" type="range" min="0" max="99" value="0" oninput="set_txt('+board_id+','+led_id+')" onchange="set_duty('+board_id+','+led_id+')" /><span id="v_'+board_id+'_'+led_id+'">0</span>&nbsp;&nbsp;');
}
var n = 1;
for (i = 100; i < 102; ++i) {
	for (j = 2; j < 12; ++j) {
		document.write(n + '号:');
		n++;
		write_one(i, j);
		if ((n % 2) == 1) document.write('<br />');
	}
}
