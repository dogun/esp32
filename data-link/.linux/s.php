<?php
date_default_timezone_set('Asia/Hong_Kong');

include('pw.php');
$mysqli = new mysqli('localhost', 'root', $pw, 'tofu');

// 处理表单提交
$b = isset($_POST['board']) ? $_POST['board'] : '1';
$d = isset($_POST['date']) ? $_POST['date'] : date('Y-m-d');
$s = isset($_POST['sensor']) ? $_POST['sensor'] : 'pt100';

$b = intval($b);

$q = $mysqli->query("SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'tofu' AND TABLE_NAME = 'sensor'");
$cs = array();
while (($row = $q->fetch_assoc()) != NULL) {
	$cs[] = $row['COLUMN_NAME'];
}

$stime = strtotime($d.' 0:0:0');
$etime = strtotime($d.' 23:59:59');

$type = 0;
if (strstr($s, 'current')) {
	$type = 1;
}

$q = $mysqli->query("select timestamp, $s from sensor where timestamp >= $stime and timestamp <= $etime and type=$type and board=$board order by timestamp asc");
$data = array();
while (($row = $q->fetch_assoc()) != NULL) {
	$data[$row['timestamp']] = $row[$s];
}

?>

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
                                        <?= $selectedBoard == $i ? 'checked' : '' ?>>
                                    <label for="board<?= $i ?>"><?= $i ?>号板</label>
                                </div>
                            <?php endfor; ?>
                        </div>
                    </div>
                    
                    <div class="filter-group">
                        <h3 class="filter-title">
                            <i>📅</i> 选择日期
                        </h3>
                        <input type="date" name="date" class="date-picker" value="<?= $selectedDate ?>">
                    </div>
                    
                    <div class="filter-group">
                        <h3 class="filter-title">
                            <i>📡</i> 选择传感器
                        </h3>
                        <div class="sensor-options">
                            <?php 
                            $sensors = ['pt100' => 'PT100', 'pt101' => 'PT101', 'pt102' => 'PT102'];
                            foreach ($sensors as $id => $name): 
                            ?>
                                <div class="sensor-option sensor-<?= $id ?>">
                                    <input type="checkbox" name="sensors[]" id="sensor-<?= $id ?>" value="<?= $id ?>"
                                        <?= in_array($id, $selectedSensors) ? 'checked' : '' ?>>
                                    <div class="sensor-color"></div>
                                    <label for="sensor-<?= $id ?>"><?= $name ?> 传感器</label>
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
                            <?= $selectedDate ?> | <?= $selectedBoard ?>号板
                        </div>
                    </div>
                    
                    <div class="chart-wrapper">
                        <canvas id="temperatureChart"></canvas>
                    </div>
                    
                    <div class="data-summary">
                        <div class="summary-card">
                            <div class="summary-label">当前板子</div>
                            <div class="summary-value"><?= $selectedBoard ?>号</div>
                        </div>
                        <div class="summary-card">
                            <div class="summary-label">监测日期</div>
                            <div class="summary-value"><?= $selectedDate ?></div>
                        </div>
                        <div class="summary-card">
                            <div class="summary-label">传感器数量</div>
                            <div class="summary-value"><?= count($selectedSensors) ?></div>
                        </div>
                    </div>
                </div>
            </div>
        </form>
        
        <footer>
            <p>温度传感器数据可视化系统 &copy; <?= date('Y') ?> | 模拟数据展示</p>
        </footer>
    </div>
</body>
</html>