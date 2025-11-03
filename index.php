<?php
header('Content-Type: text/html; charset=utf-8');

$method = $_SERVER['REQUEST_METHOD'] ?? '';
$reqUri = $_SERVER['REQUEST_URI'] ?? '';
$client = $_SERVER['REMOTE_ADDR'] ?? '';
$server = ($_SERVER['SERVER_NAME'] ?? 'localhost') . ':' . ($_SERVER['SERVER_PORT'] ?? '');
$now    = date('Y-m-d H:i:s');

if (isset($_GET['phpinfo'])) { phpinfo(); exit; }

function h($s){ return htmlspecialchars((string)$s, ENT_QUOTES, 'UTF-8'); }
function fsize($b){
    $u=['B','KB','MB','GB','TB']; $i=0; $b=max(0,(int)$b);
    while($b>=1024 && $i<count($u)-1){ $b/=1024; $i++; }
    return sprintf($i? '%.2f %s':'%d %s', $b, $u[$i]);
}

$uploadResults = [];
if ($method === 'POST' && !empty($_FILES)) {
    $targetDir = __DIR__ . '/uploads';
    if (!is_dir($targetDir)) @mkdir($targetDir, 0777, true);

    $handleOne = function($field, $name, $tmp, $err, $size) use ($targetDir, &$uploadResults) {
        if ($err !== UPLOAD_ERR_OK) {
            $uploadResults[] = ['name'=>$name, 'ok'=>false, 'msg'=>"error=$err"];
            return;
        }
        $safe = preg_replace('/[^\w\.\-\@]+/','_', (string)$name);
        $dest = $targetDir . '/' . $safe;
        if (@move_uploaded_file($tmp, $dest)) {
            $uploadResults[] = ['name'=>$safe, 'ok'=>true, 'msg'=>"saved to uploads/$safe (".fsize($size).")"];
        } else {
            $uploadResults[] = ['name'=>$safe, 'ok'=>false, 'msg'=>"move_failed"];
        }
    };

    foreach ($_FILES as $field => $f) {
        if (is_array($f['name'])) {
            foreach ($f['name'] as $i => $n) {
                $handleOne($field, $n, $f['tmp_name'][$i], $f['error'][$i], $f['size'][$i]);
            }
        } else {
            $handleOne($field, $f['name'], $f['tmp_name'], $f['error'], $f['size']);
        }
    }
}

$files = @scandir(__DIR__) ?: [];
$files = array_values(array_filter($files, function($x){
    return $x !== '.' && $x !== '..';
}));
?>
<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>PHP CGI 首页</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root{
  --bg:#0f172a; --card:#111827; --txt:#e5e7eb; --muted:#9ca3af; --accent:#22d3ee;
  --good:#10b981; --warn:#f59e0b; --bad:#ef4444; --link:#60a5fa;
}
*{box-sizing:border-box}
body{margin:0;background:linear-gradient(135deg,#0f172a,#111827);color:var(--txt);font:14px/1.6 system-ui,-apple-system,Segoe UI,Roboto,Arial}
.container{max-width:1024px;margin:32px auto;padding:0 16px}
.header{display:flex;align-items:center;justify-content:space-between;margin-bottom:16px}
.title{font-size:20px;font-weight:700;letter-spacing:.5px}
.badge{font-size:12px;color:#000;background:var(--accent);padding:4px 8px;border-radius:999px}
.grid{display:grid;gap:16px;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));}
.card{background:rgba(255,255,255,.05);border:1px solid rgba(255,255,255,.08);border-radius:12px;backdrop-filter: blur(6px);}
.card h3{margin:0;padding:12px 14px;border-bottom:1px solid rgba(255,255,255,.08);font-size:14px;color:#cbd5e1}
.card .content{padding:14px}
.kv{display:grid;grid-template-columns:120px 1fr;gap:8px 12px}
.k{color:var(--muted)}
.v code{background:rgba(0,0,0,.35);padding:2px 6px;border-radius:6px}
ul{margin:0;padding-left:18px}
a{color:var(--link);text-decoration:none}
a:hover{text-decoration:underline}
.btns a,.btns button{display:inline-block;margin-right:8px;margin-bottom:8px;padding:8px 12px;border-radius:8px;border:1px solid rgba(255,255,255,.15);background:rgba(255,255,255,.06);color:var(--txt);cursor:pointer}
.btns a:hover,.btns button:hover{background:rgba(255,255,255,.1)}
.table{width:100%;border-collapse:collapse}
.table th,.table td{border-bottom:1px solid rgba(255,255,255,.08);text-align:left;padding:8px 6px;font-size:13px;vertical-align:top}
.ok{color:var(--good)} .bad{color:var(--bad)}
footer{margin:20px 0 40px;color:var(--muted);font-size:12px;text-align:center}
small.mono{font-family:ui-monospace, SFMono-Regular, Menlo, Consolas, monospace}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="title">PHP CGI 欢迎页</div>
    <div class="badge">运行中</div>
  </div>

  <div class="grid">
    <div class="card">
      <h3>请求信息</h3>
      <div class="content kv">
        <div class="k">时间</div><div class="v"><?=h($now)?></div>
        <div class="k">客户端</div><div class="v"><?=h($client)?></div>
        <div class="k">服务器</div><div class="v"><?=h($server)?></div>
        <div class="k">方法</div><div class="v"><code><?=h($method)?></code></div>
        <div class="k">URI</div><div class="v"><code><?=h($reqUri)?></code></div>
      </div>
    </div>

    <div class="card">
      <h3>快速操作</h3>
      <div class="content btns">
        <a href="/index.php?phpinfo=1">查看 phpinfo()</a>
        <a href="/test.php?a=1&b=2">访问 test.php</a>
        <a href="/index.php">刷新首页</a>
      </div>
    </div>

    <div class="card">
      <h3>GET 参数</h3>
      <div class="content">
        <?php if (empty($_GET)): ?>
          <small class="mono" style="color:var(--muted)">无</small>
        <?php else: ?>
          <table class="table">
            <thead><tr><th>Key</th><th>Value</th></tr></thead>
            <tbody>
              <?php foreach($_GET as $k=>$v): ?>
                <tr><td><code><?=h($k)?></code></td><td><code><?=h(is_array($v)? json_encode($v):$v)?></code></td></tr>
              <?php endforeach; ?>
            </tbody>
          </table>
        <?php endif; ?>
      </div>
    </div>

    <div class="card">
      <h3>POST 参数</h3>
      <div class="content">
        <?php if (empty($_POST)): ?>
          <small class="mono" style="color:var(--muted)">无</small>
        <?php else: ?>
          <table class="table">
            <thead><tr><th>Key</th><th>Value</th></tr></thead>
            <tbody>
              <?php foreach($_POST as $k=>$v): ?>
                <tr><td><code><?=h($k)?></code></td><td><code><?=h(is_array($v)? json_encode($v):$v)?></code></td></tr>
              <?php endforeach; ?>
            </tbody>
          </table>
        <?php endif; ?>
      </div>
    </div>

    <div class="card">
      <h3>上传文件</h3>
      <div class="content">
        <form method="post" enctype="multipart/form-data">
          <input type="file" name="files[]" multiple>
          <button type="submit">上传</button>
        </form>
        <?php if (!empty($uploadResults)): ?>
          <ul>
            <?php foreach($uploadResults as $r): ?>
              <li class="<?= $r['ok']?'ok':'bad' ?>">
                <?= $r['ok']?'✓':'✗' ?> <?=h($r['name'])?> — <?=h($r['msg'])?>
              </li>
            <?php endforeach; ?>
          </ul>
          <small class="mono">保存目录：uploads/</small>
        <?php else: ?>
          <small class="mono" style="color:var(--muted)">选择文件后提交即可上传（需 multipart/form-data）</small>
        <?php endif; ?>
      </div>
    </div>

    <div class="card" style="grid-column:1/-1">
      <h3>当前目录</h3>
      <div class="content">
        <table class="table">
          <thead><tr><th>名称</th><th>大小</th></tr></thead>
          <tbody>
          <?php foreach($files as $f):
              $path = __DIR__ . '/' . $f;
              $isDir = is_dir($path);
              $size = $isDir ? '-' : fsize(@filesize($path));
          ?>
            <tr>
              <td><a href="<?= '/'. rawurlencode($f) ?>"><?= h($f) ?><?= $isDir ? '/' : '' ?></a></td>
              <td><?= h($size) ?></td>
            </tr>
          <?php endforeach; ?>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <footer>Powered by PHP CGI • 将 index.php 放在服务根目录即可访问 /index.php</footer>
</div>
</body>
</html>