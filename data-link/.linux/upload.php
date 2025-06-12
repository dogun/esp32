<?php
date_default_timezone_set('Asia/Hong_Kong');
$pos_r = array();
$offset = 0;
while (true) {
	$time = time() - $offset;
	$now = date('Ymd', $time);
	$file = $now.'.data';
	$fp = fopen($file, 'r');
	if (!$fp) {
		echo "file not found: $fp \n";
		sleep(10);
		continue;
	}
	$pos = intval(@$pos_r[$file]);
	fseek($fp, $pos);
	
	$line = '';
	while (false !== ($char = fgetc($fp))) {
		$pos++;
	    if ($char == "\n") {
	    	break;
	    }
		$line .= $char;
	}
	//echo "$line \n";
	if (strlen($line) > 0) {
		$pos_r[$file] = $pos;
		$arr = explode(':', $line);
		$ts = intval($arr[0]);
		if ($ts <= 1648235485) continue;
		$ds = trim($arr[1]);
		$ds = explode(',', $ds);
		$board = $ds[0];
		$type = $ds[1];
		if ($type == 0) {
			$pt100 = $ds[2];
			$pt101 = $ds[3];
			$pt102 = $ds[4];
			$pt103 = $ds[5];
			$flow1 = $ds[6];
			$flow2 = $ds[7];
			$flow3 = $ds[8];
			$flow4 = $ds[9];
			
			$current1 = 0;
			$current2 = 0;
			$current3 = 0;
			$current4 = 0;
			$mcu_tem = 0;
			$vref = 0;
		} else {
			$pt100 = 0;
			$pt101 = 0;
			$pt102 = 0;
			$pt103 = 0;
			$flow1 = 0;
			$flow2 = 0;
			$flow3 = 0;
			$flow4 = 0;
			
			$current1 = $ds[2];
			$current2 = $ds[3];
			$current3 = $ds[4];
			$current4 = $ds[5];
			$mcu_tem = $ds[6];
			$vref = $ds[7];
		}
		//echo "$ts $board $type $pt100 $pt101 $pt102 $pt103 $flow1 $flow2 $flow3 $flow4 $current1 $current2 $current3 $current4 $mcu_tem $vref\n";
		$params = "ts=$ts&board=$board&type=$type&pt100=$pt100&pt101=$pt101&pt102=$pt102&pt103=$pt103&flow1=$flow1&flow2=$flow2&flow3=$flow3&flow4=$flow4&current1=$current1&current2=$current2&current3=$current3&current4=$current4&mcu_tem=$mcu_tem&vref=$vref";
		$url = "http://yueyue.com/dl/d.php?$params";
		echo date('Y-m-d H:i:s', $ts);
		echo ':';
		echo file_get_contents($url);
	}else {
		sleep(1);
		echo "wait\n";
	}
	fclose($fp);
}
?>