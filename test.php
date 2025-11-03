<?php
// 建议保存为 ./test.php（与你运行服务的当前工作目录一致）
header('Content-Type: text/plain; charset=utf-8');

$method = $_SERVER['REQUEST_METHOD'] ?? '';
$uri    = $_SERVER['REQUEST_URI'] ?? '';
$ct     = $_SERVER['CONTENT_TYPE'] ?? '';
$cl     = $_SERVER['CONTENT_LENGTH'] ?? '';

$raw = file_get_contents('php://input');
$json = null;
if ($ct && stripos($ct, 'application/json') !== false) {
    $json = json_decode($raw, true);
}

echo "Method: {$method}\n";
echo "URI: {$uri}\n";
echo "Content-Type: {$ct}\n";
echo "Content-Length: {$cl}\n\n";

echo "_GET: ";
var_export($_GET);
echo "\n";

echo "_POST: ";
var_export($_POST);
echo "\n";

if ($json !== null) {
    echo "JSON: ";
    var_export($json);
    echo "\n";
}

echo "Raw-Body-Bytes: " . strlen($raw) . "\n\n";

echo "_FILES:\n";
if (!empty($_FILES)) {
    foreach ($_FILES as $name => $f) {
        echo "- {$name}: name={$f['name']} size={$f['size']} tmp_name={$f['tmp_name']} error={$f['error']}\n";
        if (is_uploaded_file($f['tmp_name'])) {
            $dest = sys_get_temp_dir() . '/' . basename($f['name']);
            if (@move_uploaded_file($f['tmp_name'], $dest)) {
                echo "  -> saved to {$dest}\n";
            } else {
                echo "  -> move failed\n";
            }
        }
    }
} else {
    echo "(none)\n";
}

echo "\nDone.\n";