$ErrorActionPreference = "Stop"

$script:CommandQueue = New-Object System.Collections.ArrayList
$script:LatestRuntimeState = '{"ok":false,"message":"No runtime state received yet."}'

function Write-HttpResponse {
    param(
        [Parameter(Mandatory = $true)][System.Net.Sockets.TcpClient]$Client,
        [Parameter(Mandatory = $true)][int]$StatusCode,
        [Parameter(Mandatory = $true)][string]$Body,
        [string]$ContentType = "application/json; charset=utf-8"
    )

    $statusText = switch ($StatusCode) {
        200 { "OK" }
        204 { "No Content" }
        404 { "Not Found" }
        default { "OK" }
    }

    $stream = $Client.GetStream()
    $bodyBytes = [System.Text.Encoding]::UTF8.GetBytes($Body)
    $header = "HTTP/1.1 $StatusCode $statusText`r`nContent-Type: $ContentType`r`nContent-Length: $($bodyBytes.Length)`r`nConnection: close`r`n`r`n"
    $headerBytes = [System.Text.Encoding]::ASCII.GetBytes($header)
    $stream.Write($headerBytes, 0, $headerBytes.Length)
    if ($bodyBytes.Length -gt 0) {
        $stream.Write($bodyBytes, 0, $bodyBytes.Length)
    }
    $stream.Flush()
}

function Read-HttpRequest {
    param([Parameter(Mandatory = $true)][System.Net.Sockets.TcpClient]$Client)

    $stream = $Client.GetStream()
    $reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::UTF8, $false, 4096, $true)

    $requestLine = $reader.ReadLine()
    if ([string]::IsNullOrWhiteSpace($requestLine)) {
        return $null
    }

    $parts = $requestLine.Split(" ")
    $method = $parts[0]
    $path = $parts[1]
    $headers = @{}

    while ($true) {
        $line = $reader.ReadLine()
        if ($line -eq $null -or $line -eq "") {
            break
        }

        $separator = $line.IndexOf(":")
        if ($separator -gt 0) {
            $headerName = $line.Substring(0, $separator).Trim()
            $headerValue = $line.Substring($separator + 1).Trim()
            $headers[$headerName] = $headerValue
        }
    }

    $body = ""
    if ($headers.ContainsKey("Content-Length")) {
        $contentLength = [int]$headers["Content-Length"]
        if ($contentLength -gt 0) {
            $buffer = New-Object char[] $contentLength
            [void]$reader.ReadBlock($buffer, 0, $contentLength)
            $body = -join $buffer
        }
    }

    return @{
        Method = $method
        Path = $path
        Headers = $headers
        Body = $body
    }
}

function Get-UiHtml {
@"
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8" />
  <title>Designer Mock Sender</title>
  <style>
    body {
      margin: 0;
      font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
      background: linear-gradient(135deg, #111827, #1f2937);
      color: #f9fafb;
    }
    .page {
      display: grid;
      grid-template-columns: 420px 1fr;
      gap: 20px;
      padding: 24px;
      min-height: 100vh;
      box-sizing: border-box;
    }
    .card {
      background: rgba(17, 24, 39, 0.92);
      border: 1px solid rgba(255,255,255,0.08);
      border-radius: 16px;
      padding: 18px;
      box-shadow: 0 18px 40px rgba(0,0,0,0.28);
    }
    h1, h2 {
      margin: 0 0 12px 0;
      font-weight: 700;
    }
    h1 { font-size: 24px; }
    h2 { font-size: 16px; }
    label {
      display: block;
      font-size: 13px;
      margin: 12px 0 6px 0;
      color: #cbd5e1;
    }
    input, select, textarea, button {
      width: 100%;
      box-sizing: border-box;
      border-radius: 10px;
      border: 1px solid rgba(255,255,255,0.10);
      padding: 10px 12px;
      font-size: 14px;
    }
    input, select, textarea {
      background: #0f172a;
      color: #f8fafc;
    }
    textarea {
      min-height: 120px;
      resize: vertical;
    }
    button {
      background: #f97316;
      color: white;
      font-weight: 700;
      cursor: pointer;
      border: none;
      margin-top: 12px;
    }
    button.secondary {
      background: #334155;
    }
    .row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    .status {
      font-size: 13px;
      color: #fdba74;
      margin-top: 12px;
      white-space: pre-wrap;
    }
    pre {
      background: #020617;
      border-radius: 12px;
      padding: 14px;
      overflow: auto;
      min-height: 220px;
      border: 1px solid rgba(255,255,255,0.06);
    }
  </style>
</head>
<body>
  <div class="page">
    <div class="card">
      <h1>本地模拟发送端</h1>
      <label>Template</label>
      <select id="templateCode">
        <option value="lowerthird_basic">lowerthird_basic</option>
      </select>

      <div class="row">
        <div>
          <label>Title</label>
          <input id="title" value="Alice Chen" />
        </div>
        <div>
          <label>Subtitle</label>
          <input id="subtitle" value="Beijing Bureau" />
        </div>
      </div>

      <div class="row">
        <div>
          <label>Theme Color</label>
          <input id="themeColor" value="#FF6600FF" />
        </div>
        <div>
          <label>Show Logo</label>
          <select id="showLogo">
            <option value="true" selected>true</option>
            <option value="false">false</option>
          </select>
        </div>
      </div>

      <label>Logo Path</label>
      <input id="logo" value="" placeholder="C:\assets\logo.png" />

      <button onclick="enqueueActivate()">发送 Activate</button>
      <button onclick="enqueueApply()">发送 Apply</button>
      <div class="row">
        <button onclick="enqueueTakeIn()">发送 Take In</button>
        <button onclick="enqueueTakeOut()">发送 Take Out</button>
      </div>
      <div class="row">
        <button class="secondary" onclick="enqueueClear()">发送 Clear</button>
        <button class="secondary" onclick="refreshState()">刷新 State</button>
      </div>

      <label>Raw Command JSON</label>
      <textarea id="rawCommand">{
  "requestId": "req-custom-001",
  "command": "status"
}</textarea>
      <button class="secondary" onclick="enqueueRaw()">发送 Raw JSON</button>

      <div id="status" class="status">Ready.</div>
    </div>

    <div class="card">
      <h2>Unreal Runtime State</h2>
      <pre id="runtimeState">Waiting for state push...</pre>
      <h2>Pending Queue</h2>
      <pre id="queueState">[]</pre>
    </div>
  </div>

  <script>
    async function postJson(url, payload) {
      const response = await fetch(url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
      });
      return response.json();
    }

    function requestId(prefix) {
      return `${prefix}-${Date.now()}`;
    }

    function readFields() {
      return {
        title: document.getElementById("title").value,
        subtitle: document.getElementById("subtitle").value,
        themeColor: document.getElementById("themeColor").value,
        logo: document.getElementById("logo").value,
        showLogo: document.getElementById("showLogo").value === "true"
      };
    }

    async function enqueueCommand(command) {
      const result = await postJson("/mock/enqueue", command);
      document.getElementById("status").textContent = result.message || "Queued.";
      await refreshState();
    }

    async function enqueueActivate() {
      await enqueueCommand({
        requestId: requestId("activate"),
        command: "activate",
        templateCode: document.getElementById("templateCode").value
      });
    }

    async function enqueueApply() {
      await enqueueCommand({
        requestId: requestId("apply"),
        command: "apply",
        fields: readFields()
      });
    }

    async function enqueueTakeIn() {
      await enqueueCommand({
        requestId: requestId("takein"),
        command: "takein"
      });
    }

    async function enqueueTakeOut() {
      await enqueueCommand({
        requestId: requestId("takeout"),
        command: "takeout"
      });
    }

    async function enqueueClear() {
      await enqueueCommand({
        requestId: requestId("clear"),
        command: "clear"
      });
    }

    async function enqueueRaw() {
      const payload = JSON.parse(document.getElementById("rawCommand").value);
      await enqueueCommand(payload);
    }

    async function refreshState() {
      const response = await fetch("/mock/state");
      const payload = await response.json();
      document.getElementById("runtimeState").textContent = JSON.stringify(payload.runtimeState, null, 2);
      document.getElementById("queueState").textContent = JSON.stringify(payload.pendingQueue, null, 2);
    }

    refreshState();
    setInterval(refreshState, 1500);
  </script>
</body>
</html>
"@
}

$address = [System.Net.IPAddress]::Parse("127.0.0.1")
$listener = New-Object System.Net.Sockets.TcpListener($address, 8080)
$listener.Start()
Write-Host "Designer mock control server started at http://127.0.0.1:8080/ui" -ForegroundColor Green

try {
    while ($true) {
        $client = $listener.AcceptTcpClient()
        try {
            $request = Read-HttpRequest -Client $client
            if ($null -eq $request) {
                Write-HttpResponse -Client $client -StatusCode 404 -Body '{"ok":false,"message":"Empty request"}'
                continue
            }

            if ($request.Method -eq "GET" -and ($request.Path -eq "/" -or $request.Path -eq "/ui")) {
                Write-HttpResponse -Client $client -StatusCode 200 -Body (Get-UiHtml) -ContentType "text/html; charset=utf-8"
                continue
            }

            if ($request.Method -eq "GET" -and $request.Path -eq "/control/next") {
                if ($script:CommandQueue.Count -eq 0) {
                    Write-HttpResponse -Client $client -StatusCode 204 -Body ''
                    continue
                }

                $nextCommand = $script:CommandQueue[0]
                $script:CommandQueue.RemoveAt(0)
                Write-HttpResponse -Client $client -StatusCode 200 -Body ($nextCommand | ConvertTo-Json -Depth 10 -Compress)
                continue
            }

            if ($request.Method -eq "POST" -and $request.Path -eq "/control/state") {
                if (-not [string]::IsNullOrWhiteSpace($request.Body)) {
                    $script:LatestRuntimeState = $request.Body
                }
                Write-HttpResponse -Client $client -StatusCode 200 -Body '{"ok":true,"message":"Runtime state received."}'
                continue
            }

            if ($request.Method -eq "POST" -and $request.Path -eq "/mock/enqueue") {
                $command = $request.Body | ConvertFrom-Json
                [void]$script:CommandQueue.Add($command)
                $queueMessage = @{
                    ok = $true
                    message = "Queued command: $($command.command)"
                    queueLength = $script:CommandQueue.Count
                } | ConvertTo-Json -Depth 10 -Compress
                Write-HttpResponse -Client $client -StatusCode 200 -Body $queueMessage
                continue
            }

            if ($request.Method -eq "GET" -and $request.Path -eq "/mock/state") {
                $runtimeStateObject = $null
                try {
                    $runtimeStateObject = $script:LatestRuntimeState | ConvertFrom-Json
                }
                catch {
                    $runtimeStateObject = @{ raw = $script:LatestRuntimeState }
                }

                $payload = @{
                    runtimeState = $runtimeStateObject
                    pendingQueue = @($script:CommandQueue)
                } | ConvertTo-Json -Depth 20

                Write-HttpResponse -Client $client -StatusCode 200 -Body $payload
                continue
            }

            Write-HttpResponse -Client $client -StatusCode 404 -Body '{"ok":false,"message":"Not Found"}'
        }
        finally {
            $client.Close()
        }
    }
}
finally {
    $listener.Stop()
}



