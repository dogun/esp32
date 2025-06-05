<?php
date_default_timezone_set('Asia/Hong_Kong');

function pt($v, $b = 0, $s = 'PT') {
	//echo $v.' '.$b.' '.$s;
	$c_data[0]['PT']['R150'] = 150;
	$c_data[0]['PT']['vol'] = 3.26;
	
	$c_data[2]['pt100']['R150'] = 150;
	$c_data[2]['pt100']['vol'] = 3.26;
	
	if (!@$c_data[$b][$s]) {
		$b = 0;
		$s = 'PT';
	}
	
	$r150 = $c_data[$b][$s]['R150'];
	$vol = $c_data[$b][$s]['vol'];

	$v = $v*$r150/($vol*4095/3.3-$v);
	$v = ($v-100)/0.3908-($v-100)*($v-100)/2.46e6;
	// $v = ($v-100)/0.384;
	
	//echo ' '.$v."\n";
	
	return intval($v * 10 + 0.5) / 10;
}

include('pw.php');
$mysqli = new mysqli('localhost', 'root', $pw, 'tofu');

// 处理表单提交
$b = isset($_POST['board']) ? $_POST['board'] : '1';
$d = isset($_POST['date']) ? $_POST['date'] : date('Y-m-d');
$s = isset($_POST['sensors']) ? $_POST['sensors'] : ['pt100'];

$b = intval($b);

$q = $mysqli->query("SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'tofu' AND TABLE_NAME = 'sensor'");
$cs = array();
$i = 0;
while (($row = $q->fetch_assoc()) != NULL) {
	$i++;
	if ($i > 4)
		$cs[] = $row['COLUMN_NAME'];
}

$stime = strtotime($d.' 0:0:0');
$etime = strtotime($d.' 23:59:59');

$ss = '';
foreach ($s as $sn) {
	$ss .= ", $sn";
}

$sql = "select timestamp, type $ss from sensor where timestamp >= $stime and timestamp <= $etime and board_no=$b order by timestamp asc";
//echo $sql;
$q = $mysqli->query($sql);
$data = array();
while (($row = $q->fetch_assoc()) != NULL) {
	$m = date('H:i', $row['timestamp']);
	$t = $row['type'];
	foreach ($s as $sn) {
		if (strstr($sn, 'current') && $t == 0) continue;
		if (!strstr($sn, 'current') && $t == 1) continue;
		if (!@$data[$sn]) $data[$sn] = array();
		if (!@$data[$sn][$m]) $data[$sn][$m] = ['cnt' => 0, 'total' => 0, 'max' => 0, 'min' => 10000];
		$data[$sn][$m]['cnt'] ++;
		$data[$sn][$m]['total'] += $row[$sn];
		if ($row[$sn] > $data[$sn][$m]['max']) $data[$sn][$m]['max'] = $row[$sn];
		if ($row[$sn] < $data[$sn][$m]['min']) $data[$sn][$m]['min'] = $row[$sn];
	}
}

$times = array();
$datas = array();
foreach ($data as $sn => $row) {
	$datas[$sn] = array();
	foreach ($data[$sn] as $m=>$row1) {
		if (!in_array($m, $times)) $times[] = $m;
		$data[$sn][$m]['val'] = $row1['total'] / $row1['cnt'];
		if (strstr($sn, 'pt10')) {
			$data[$sn][$m]['val'] = pt($data[$sn][$m]['max'], $b, $sn);
		}
		$datas[$sn][] = $data[$sn][$m]['val'];
		//echo $sn.' '.$m.' '.$data[$sn][$m]['val']."\n";
	}
}

$js_data = [
        'timestamps' => $times,
        'data' => $datas
    ];
?>

<!-- 
select FROM_UNIXTIME(timestamp), pt101, (om-100)/0.384 as t1, (om-100)/0.3908-(om-100)*(om-100)/2.46e6 as t2 from (SELECT timestamp, pt101, pt101*150/(4096-pt101) as om FROM `sensor`  
WHERE board_no=2 and type=0 and timestamp>unix_timestamp() - 86400) t order by timestamp;
-->

<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>温度传感器数据可视化</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        
        body {
            background: linear-gradient(135deg, #1a2a6c, #b21f1f, #1a2a6c);
            color: #fff;
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        
        header {
            text-align: center;
            padding: 30px 0;
            margin-bottom: 20px;
        }
        
        h1 {
            font-size: 2.8rem;
            margin-bottom: 10px;
            text-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }
        
        .subtitle {
            font-size: 1.2rem;
            opacity: 0.9;
            max-width: 600px;
            margin: 0 auto;
        }
        
        .dashboard {
            display: flex;
            gap: 30px;
            margin-bottom: 30px;
        }
        
        .filters {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            padding: 25px;
            width: 320px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .chart-container {
            flex: 1;
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            padding: 25px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .filter-group {
            margin-bottom: 25px;
        }
        
        .filter-title {
            font-size: 1.3rem;
            margin-bottom: 15px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .filter-title i {
            font-size: 1.4rem;
        }
        
        .board-options {
            display: flex;
            gap: 10px;
        }
        
        .board-option {
            flex: 1;
            text-align: center;
        }
        
        .board-option input[type="radio"] {
            display: none;
        }
        
        .board-option label {
            display: block;
            padding: 15px 10px;
            background: rgba(255, 255, 255, 0.15);
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s ease;
        }
        
        .board-option input[type="radio"]:checked + label {
            background: rgba(86, 119, 252, 0.6);
            box-shadow: 0 0 15px rgba(86, 119, 252, 0.5);
        }
        
        .board-option label:hover {
            background: rgba(255, 255, 255, 0.25);
        }
        
        .date-picker {
            width: 100%;
            padding: 15px;
            background: rgba(255, 255, 255, 0.15);
            border: none;
            border-radius: 10px;
            color: white;
            font-size: 1.1rem;
        }
        
        .sensor-options {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }
        
        .sensor-option {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 12px 15px;
            background: rgba(255, 255, 255, 0.15);
            border-radius: 10px;
            transition: all 0.3s ease;
        }
        
        .sensor-option:hover {
            background: rgba(255, 255, 255, 0.25);
        }
        
        .sensor-option input[type="checkbox"] {
            width: 20px;
            height: 20px;
        }
        
        .sensor-color {
            width: 20px;
            height: 20px;
            border-radius: 50%;
        }
        
        .sensor-pt100 .sensor-color { background: #FF6384; }
        .sensor-pt101 .sensor-color { background: #36A2EB; }
        .sensor-pt102 .sensor-color { background: #4BC0C0; }
        .sensor-pt103 .sensor-color { background: #0CC044; }
		.sensor-flow1 .sensor-color { background: #111111; }
		.sensor-flow2 .sensor-color { background: #DD11DD; }
		.sensor-flow3 .sensor-color { background: #1111DD; }
		.sensor-flow4 .sensor-color { background: #DD1111; }
		.sensor-current1 .sensor-color { background: #555555; }
		.sensor-current2 .sensor-color { background: #CC55DD; }
		.sensor-current3 .sensor-color { background: #5555CC; }
		.sensor-current4 .sensor-color { background: #CC5555; }
        
        .btn-submit {
            width: 100%;
            padding: 16px;
            background: linear-gradient(45deg, #5677fc, #7b68ee);
            border: none;
            border-radius: 10px;
            color: white;
            font-size: 1.2rem;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.3s ease;
            box-shadow: 0 4px 15px rgba(86, 119, 252, 0.4);
        }
        
        .btn-submit:hover {
            transform: translateY(-3px);
            box-shadow: 0 6px 20px rgba(86, 119, 252, 0.6);
        }
        
        .chart-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
        }
        
        .chart-title {
            font-size: 1.8rem;
        }
        
        .chart-wrapper {
            height: 500px;
            position: relative;
        }
        
        .data-summary {
            display: flex;
            gap: 20px;
            margin-top: 25px;
            flex-wrap: wrap;
        }
        
        .summary-card {
            flex: 1;
            min-width: 200px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 12px;
            padding: 20px;
            text-align: center;
        }
        
        .summary-value {
            font-size: 2.2rem;
            font-weight: bold;
            margin: 10px 0;
            color: #7b68ee;
        }
        
        .summary-label {
            font-size: 1.1rem;
            opacity: 0.8;
        }
        
        footer {
            text-align: center;
            padding: 20px;
            margin-top: 30px;
            font-size: 0.9rem;
            opacity: 0.7;
        }
        
        @media (max-width: 900px) {
            .dashboard {
                flex-direction: column;
            }
            
            .filters {
                width: 100%;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>传感器数据</h1>
            <p class="subtitle">支持按板子、日期和传感器筛选</p>
        </header>
        
        <form method="POST">
            <div class="dashboard">
                <div class="filters">
                    <div class="filter-group">
                        <h3 class="filter-title">
                            <i>📋</i> 选择板子
                        </h3>
                        <div class="board-options">
                            <?php for ($i = 1; $i <= 3; $i++): ?>
                                <div class="board-option">
                                    <input type="radio" name="board" id="board<?= $i ?>" value="<?= $i ?>" 
                                        <?= $b == $i ? 'checked' : '' ?>>
                                    <label for="board<?= $i ?>"><?= $i ?>号板</label>
                                </div>
                            <?php endfor; ?>
                        </div>
                    </div>
                    
                    <div class="filter-group">
                        <h3 class="filter-title">
                            <i>📅</i> 选择日期
                        </h3>
                        <input type="date" name="date" class="date-picker" value="<?= $d ?>">
                    </div>
                    
                    <div class="filter-group">
                        <h3 class="filter-title">
                            <i>📡</i> 选择传感器
                        </h3>
                        <div class="sensor-options">
                            <?php 
                            $sensors = $cs;
                            foreach ($sensors as $id => $name): 
                            ?>
                                <div class="sensor-option sensor-<?= $name ?>">
                                    <input type="checkbox" name="sensors[]" id="sensor-<?= $name ?>" value="<?= $name ?>"
                                        <?= in_array($name, $s) ? 'checked' : '' ?>>
                                    <div class="sensor-color"></div>
                                    <label for="sensor-<?= $name ?>"><?= $name ?> 传感器</label>
                                </div>
                            <?php endforeach; ?>
                        </div>
                    </div>
                    
                    <button type="submit" class="btn-submit">
                        <i>🔍</i> 查询数据
                    </button>
                </div>
                
                <div class="chart-container">
                    <div class="chart-header">
                        <h2 class="chart-title">数据变化曲线图</h2>
                        <div class="chart-info">
                            <?= $b ?>号板
                        </div>
                    </div>
                    
                    <div class="chart-wrapper">
                        <canvas id="temperatureChart"></canvas>
                    </div>
                    
                    <div class="data-summary">
                        <div class="summary-card">
                            <div class="summary-label">当前板子</div>
                            <div class="summary-value"><?= $b ?>号</div>
                        </div>
                        <div class="summary-card">
                            <div class="summary-label">监测日期</div>
                            <div class="summary-value"><?= $d ?></div>
                        </div>
                        <div class="summary-card">
                            <div class="summary-label">传感器</div>
                            <div class="summary-value"><?php foreach ($s as $sn) { echo " $sn "; } ?></div>
                        </div>
                    </div>
                </div>
            </div>
        </form>
        
        <footer>
            <p>传感器数据可视化系统 &copy; <?= date('Y') ?> | 数据展示</p>
        </footer>
    </div>
    
    <script>
        // 从PHP获取数据
        const chartData = <?= json_encode($js_data) ?>;
        const selectedSensors = <?= json_encode($s) ?>;
        
        // 准备图表数据
        const datasets = [];
        const colors = {
			'pt100' : '#FF6384',
			'pt101' : '#36A2EB',
			'pt102' : '#4BC0C0',
			'pt103' : '#0CC044',
			'flow1' : '#111111',
			'flow2' : '#DD11DD',
			'flow3' : '#1111DD',
			'flow4' : '#DD1111',
			'current1' : '#555555',
			'current2' : '#CC55DD',
			'current3' : '#5555CC',
			'current4' : '#CC5555'
        };
        
        selectedSensors.forEach(sensor => {
            datasets.push({
                label: sensor.toUpperCase() + ' 数据',
                data: chartData.data[sensor],
                borderColor: colors[sensor],
                backgroundColor: colors[sensor] + '20',
                borderWidth: 1,
                pointRadius: 1,
                pointBackgroundColor: colors[sensor],
                tension: 0.3,
                fill: false
            });
        });
        
        // 创建图表
        const ctx = document.getElementById('temperatureChart').getContext('2d');
        const chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: chartData.timestamps,
                datasets: datasets
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        labels: {
                            color: 'rgba(255, 255, 255, 0.9)',
                            font: {
                                size: 14
                            }
                        }
                    },
                    tooltip: {
                        mode: 'index',
                        intersect: false,
                        backgroundColor: 'rgba(30, 30, 50, 0.9)',
                        titleColor: '#fff',
                        bodyColor: '#fff',
                        borderColor: 'rgba(255, 255, 255, 0.1)',
                        borderWidth: 1
                    }
                },
                scales: {
                    x: {
                        grid: {
                            color: 'rgba(255, 255, 255, 0.1)'
                        },
                        ticks: {
                            color: 'rgba(255, 255, 255, 0.7)',
                            maxRotation: 0,
                            autoSkip: true,
                            maxTicksLimit: 24
                        },
                        title: {
                            display: true,
                            text: '时间',
                            color: 'rgba(255, 255, 255, 0.8)',
                            font: {
                                size: 14
                            }
                        }
                    },
                    y: {
                        grid: {
                            color: 'rgba(255, 255, 255, 0.1)'
                        },
                        ticks: {
                            color: 'rgba(255, 255, 255, 0.7)'
                        },
                        title: {
                            display: true,
                            text: '数据',
                            color: 'rgba(255, 255, 255, 0.8)',
                            font: {
                                size: 14
                            }
                        }
                    }
                },
                interaction: {
                    mode: 'nearest',
                    axis: 'x',
                    intersect: false
                }
            }
        });
    </script>
</body>
</html>