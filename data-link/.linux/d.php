<?php
$ts = intval($_GET['ts']);
$board = intval($_GET['board']);
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

include('pw.php');
$mysqli = new mysqli('localhost', 'root', $pw, 'dofu');

$q = $mysqli->query("select * from sensor where board_no=$board and type=$type and timestamp=$ts");
$r = $q->fetch_assoc();
if ($r) {
	echo 'ex';
	die;
}
$q = $mysqli->query("insert into sensor (board_no,type,timestamp,pt100,pt101,pt102,pt103,flow1,flow2,flow3,flow4,current1,current2,current3,current4) values ($board, $type, $ts, $pt100, $pt101, $pt102, $pt103, $flow1, $flow2, $flow3, $flow4, $current1, $current2, $current3, $current4)");
echo $q;
?>

