<?php
$ts = intval($_GET['ts']);
$board = intval($_GET['borad']);
$type = intval($_GET['type']);
$pt100 = intval($_GET['pt100']);
$pt101 = intval($_GET['pt101']);
$pt102 = intval($_GET['pt102']);
$pt103 = intval($_GET['pt103']);
$flow1 = intval($_GET['flow1']);
$flow2 = intval($_GET['flow2']);
$flow3 = intval($_GET['flow3']);
$flow4 = intval($_GET['flow4']);

$current1 = intval($_GET['current1']);
$current2 = intval($_GET['current2']);
$current3 = intval($_GET['current3']);
$current4 = intval($_GET['current4']);

		$params = "ts=$ts&board=$board&type=$type&pt100=$pt100&pt101=$pt101&pt102=$pt102&pt103=$pt103&flow1=$flow1&flow2=$flow2&flow3=$flow3&flow4=$flow4&current1=$current1&current2=$current2&current3=$current3&current4=$current4";
		
		echo $params."\n";


?>

