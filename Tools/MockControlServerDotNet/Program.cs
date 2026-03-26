using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace MockControlServerDotNet
{
    internal static class Program
    {
        private static readonly object SyncRoot = new object();
        private static readonly Queue<string> CommandQueue = new Queue<string>();
        private static string LatestRuntimeState = "{\"ok\":false,\"message\":\"No runtime state received yet.\"}";

        private static void Main()
        {
            var listener = new HttpListener();
            listener.Prefixes.Add("http://127.0.0.1:8080/");
            listener.Start();
            Console.WriteLine("Designer mock control server started at http://127.0.0.1:8080/ui");

            while (true)
            {
                var context = listener.GetContext();
                Task.Run(() => HandleContext(context));
            }
        }

        private static void HandleContext(HttpListenerContext context)
        {
            try
            {
                var request = context.Request;
                var method = request.HttpMethod ?? string.Empty;
                var path = request.Url?.AbsolutePath ?? "/";
                var body = ReadRequestBody(request);
                HandleRoute(context.Response, method, path, body);
            }
            catch (Exception ex)
            {
                WriteJson(context.Response, 500, JsonSerializer.Serialize(new
                {
                    ok = false,
                    message = ex.Message
                }));
            }
        }

        private static string ReadRequestBody(HttpListenerRequest request)
        {
            using var reader = new StreamReader(
                request.InputStream,
                request.ContentEncoding ?? Encoding.UTF8,
                detectEncodingFromByteOrderMarks: false,
                bufferSize: 4096,
                leaveOpen: false);

            return reader.ReadToEnd();
        }

        private static void HandleRoute(HttpListenerResponse response, string method, string path, string body)
        {
            if (method == "GET" && (path == "/" || path == "/ui"))
            {
                WriteText(response, 200, "text/html; charset=utf-8", GetUiHtml());
                return;
            }

            if (method == "GET" && path == "/control/next")
            {
                string next = null;
                lock (SyncRoot)
                {
                    if (CommandQueue.Count > 0)
                    {
                        next = CommandQueue.Dequeue();
                    }
                }

                if (string.IsNullOrWhiteSpace(next))
                {
                    WriteText(response, 204, "application/json; charset=utf-8", string.Empty);
                    return;
                }

                WriteJson(response, 200, next);
                return;
            }

            if (method == "POST" && path == "/control/state")
            {
                if (!string.IsNullOrWhiteSpace(body))
                {
                    lock (SyncRoot)
                    {
                        LatestRuntimeState = body;
                    }
                }

                WriteJson(response, 200, "{\"ok\":true,\"message\":\"Runtime state received.\"}");
                return;
            }

            if (method == "POST" && path == "/mock/enqueue")
            {
                using var document = JsonDocument.Parse(body);
                var commandName = document.RootElement.TryGetProperty("command", out var commandElement)
                    ? commandElement.GetString()
                    : "unknown";

                int queueLength;
                lock (SyncRoot)
                {
                    CommandQueue.Enqueue(body);
                    queueLength = CommandQueue.Count;
                }

                WriteJson(response, 200, JsonSerializer.Serialize(new
                {
                    ok = true,
                    message = $"Queued command: {commandName}",
                    queueLength
                }));
                return;
            }

            if (method == "GET" && path == "/mock/state")
            {
                string runtimeState;
                string[] pendingQueue;
                lock (SyncRoot)
                {
                    runtimeState = LatestRuntimeState;
                    pendingQueue = CommandQueue.ToArray();
                }

                using var runtimeStateDocument = JsonDocument.Parse(runtimeState);
                using var queueDocument = JsonDocument.Parse(JsonSerializer.Serialize(pendingQueue));
                using var memoryStream = new MemoryStream();
                using (var writer = new Utf8JsonWriter(memoryStream, new JsonWriterOptions { Indented = true }))
                {
                    writer.WriteStartObject();
                    writer.WritePropertyName("runtimeState");
                    runtimeStateDocument.RootElement.WriteTo(writer);
                    writer.WritePropertyName("pendingQueue");
                    queueDocument.RootElement.WriteTo(writer);
                    writer.WriteEndObject();
                }

                WriteJson(response, 200, Encoding.UTF8.GetString(memoryStream.ToArray()));
                return;
            }

            WriteJson(response, 404, "{\"ok\":false,\"message\":\"Not Found\"}");
        }

        private static void WriteJson(HttpListenerResponse response, int statusCode, string body)
        {
            WriteText(response, statusCode, "application/json; charset=utf-8", body);
        }

        private static void WriteText(HttpListenerResponse response, int statusCode, string contentType, string body)
        {
            var bodyBytes = Encoding.UTF8.GetBytes(body ?? string.Empty);
            response.StatusCode = statusCode;
            response.ContentType = contentType;
            response.ContentEncoding = Encoding.UTF8;
            response.ContentLength64 = bodyBytes.Length;
            response.KeepAlive = false;

            if (bodyBytes.Length > 0)
            {
                response.OutputStream.Write(bodyBytes, 0, bodyBytes.Length);
            }

            response.OutputStream.Close();
        }

        private static string GetUiHtml()
        {
            return @"<!doctype html>
<html lang=""en"">
<head>
  <meta charset=""utf-8"" />
  <title>Designer Mock Sender</title>
  <style>
    body {
      margin: 0;
      font-family: ""Microsoft YaHei"", ""Segoe UI"", sans-serif;
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
    input, select, button {
      width: 100%;
      box-sizing: border-box;
      border-radius: 10px;
      border: 1px solid rgba(255,255,255,0.10);
      padding: 10px 12px;
      font-size: 14px;
      background: #0f172a;
      color: #f8fafc;
    }
    input[type=""file""], input[type=""color""] {
      padding: 8px;
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
    .hint {
      font-size: 12px;
      color: #94a3b8;
      margin-top: 6px;
    }
    .preview-card {
      margin-top: 12px;
      padding: 12px;
      border-radius: 12px;
      background: #020617;
      border: 1px solid rgba(255,255,255,0.06);
    }
    .preview-title {
      font-size: 12px;
      color: #94a3b8;
      margin-bottom: 8px;
    }
    .color-preview {
      height: 48px;
      border-radius: 10px;
      border: 1px solid rgba(255,255,255,0.12);
      box-shadow: inset 0 0 0 1px rgba(255,255,255,0.04);
    }
    .image-preview {
      width: 100%;
      height: 160px;
      border-radius: 10px;
      object-fit: contain;
      background: linear-gradient(45deg, #0f172a 25%, #111827 25%, #111827 50%, #0f172a 50%, #0f172a 75%, #111827 75%, #111827 100%);
      background-size: 24px 24px;
      border: 1px solid rgba(255,255,255,0.08);
      display: block;
    }
    .image-empty {
      display: grid;
      place-items: center;
      height: 160px;
      border-radius: 10px;
      color: #94a3b8;
      background: #020617;
      border: 1px dashed rgba(255,255,255,0.12);
      text-align: center;
      padding: 12px;
      box-sizing: border-box;
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
  <div class=""page"">
    <div class=""card"">
      <h1>Local Mock Sender</h1>
      <label>Template</label>
      <select id=""templateCode"">
        <option value=""lowerthird_basic"">lowerthird_basic</option>
      </select>

      <div class=""row"">
        <div>
          <label>Title</label>
          <input id=""title"" value=""Alice Chen"" />
        </div>
        <div>
          <label>Subtitle</label>
          <input id=""subtitle"" value=""Beijing Bureau"" />
        </div>
      </div>

      <div class=""row"">
        <div>
          <label>Theme Color</label>
          <div class=""row"">
            <input id=""themeColor"" value=""#FF6600FF"" />
            <input id=""themeColorPicker"" type=""color"" value=""#ff6600"" />
          </div>
        </div>
        <div>
          <label>Show Logo</label>
          <select id=""showLogo"">
            <option value=""true"" selected>true</option>
            <option value=""false"">false</option>
          </select>
        </div>
      </div>

      <label>Logo Path</label>
      <input id=""logo"" value="""" placeholder=""Paste a local path if Unreal needs a real file path"" />
      <input id=""logoPicker"" type=""file"" accept="".png,.jpg,.jpeg,.bmp,.webp,.tga"" />
      <div class=""hint"">The browser can pick a file name, but Unreal still needs a real local path if you want to load from disk.</div>

      <div class=""preview-card"">
        <div class=""preview-title"">Theme Color Preview</div>
        <div id=""themeColorPreview"" class=""color-preview""></div>
      </div>

      <div class=""preview-card"">
        <div class=""preview-title"">Logo Preview</div>
        <img id=""logoPreviewImage"" class=""image-preview"" alt=""Logo Preview"" style=""display:none;"" />
        <div id=""logoPreviewEmpty"" class=""image-empty"">Choose an image to preview it here.</div>
      </div>

      <button onclick=""enqueueActivate()"">Send Activate</button>
      <button onclick=""enqueueApply()"">Send Apply</button>
      <div class=""row"">
        <button onclick=""enqueueTakeIn()"">Send Take In</button>
        <button onclick=""enqueueTakeOut()"">Send Take Out</button>
      </div>
      <button class=""secondary"" onclick=""enqueueClear()"">Send Clear</button>

      <div id=""status"" class=""status"">Ready.</div>
    </div>

    <div class=""card"">
      <h2>Unreal Runtime State</h2>
      <pre id=""runtimeState"">Waiting for state push...</pre>
      <h2>Pending Queue</h2>
      <pre id=""queueState"">[]</pre>
    </div>
  </div>

  <script>
    async function postJson(url, payload) {
      const response = await fetch(url, {
        method: ""POST"",
        headers: { ""Content-Type"": ""application/json"" },
        body: JSON.stringify(payload)
      });
      return response.json();
    }

    function requestId(prefix) {
      return `${prefix}-${Date.now()}`;
    }

    function normalizeThemeColor(value) {
      if (!value) {
        return """";
      }

      const trimmed = value.trim();
      if (/^#[0-9a-fA-F]{6}$/.test(trimmed)) {
        return `${trimmed}FF`.toUpperCase();
      }

      if (/^#[0-9a-fA-F]{8}$/.test(trimmed)) {
        return trimmed.toUpperCase();
      }

      return trimmed;
    }

    function readFields() {
      return {
        title: document.getElementById(""title"").value,
        subtitle: document.getElementById(""subtitle"").value,
        themeColor: normalizeThemeColor(document.getElementById(""themeColor"").value),
        logo: document.getElementById(""logo"").value,
        showLogo: document.getElementById(""showLogo"").value === ""true""
      };
    }

    async function enqueueCommand(command) {
      const result = await postJson(""/mock/enqueue"", command);
      document.getElementById(""status"").textContent = JSON.stringify(result, null, 2);
      await refreshState();
    }

    async function enqueueActivate() {
      await enqueueCommand({
        requestId: requestId(""activate""),
        command: ""activate"",
        templateCode: document.getElementById(""templateCode"").value
      });
    }

    async function enqueueApply() {
      await enqueueCommand({
        requestId: requestId(""apply""),
        command: ""apply"",
        fields: readFields()
      });
    }

    async function enqueueTakeIn() {
      await enqueueCommand({
        requestId: requestId(""takein""),
        command: ""takein""
      });
    }

    async function enqueueTakeOut() {
      await enqueueCommand({
        requestId: requestId(""takeout""),
        command: ""takeout""
      });
    }

    async function enqueueClear() {
      await enqueueCommand({
        requestId: requestId(""clear""),
        command: ""clear""
      });
    }

    function wireInteractiveInputs() {
      const colorInput = document.getElementById(""themeColor"");
      const colorPicker = document.getElementById(""themeColorPicker"");
      const colorPreview = document.getElementById(""themeColorPreview"");
      const logoInput = document.getElementById(""logo"");
      const logoPicker = document.getElementById(""logoPicker"");
      const logoPreviewImage = document.getElementById(""logoPreviewImage"");
      const logoPreviewEmpty = document.getElementById(""logoPreviewEmpty"");

      function refreshColorPreview() {
        const normalized = normalizeThemeColor(colorInput.value);
        colorPreview.style.background = normalized || ""#0f172a"";
      }

      function refreshLogoPreview(file) {
        if (!file) {
          logoPreviewImage.removeAttribute(""src"");
          logoPreviewImage.style.display = ""none"";
          logoPreviewEmpty.style.display = ""grid"";
          return;
        }

        const previewUrl = URL.createObjectURL(file);
        logoPreviewImage.src = previewUrl;
        logoPreviewImage.style.display = ""block"";
        logoPreviewEmpty.style.display = ""none"";
      }

      colorPicker.addEventListener(""input"", () => {
        colorInput.value = `${colorPicker.value.toUpperCase()}FF`;
        refreshColorPreview();
      });

      colorInput.addEventListener(""change"", () => {
        const normalized = normalizeThemeColor(colorInput.value);
        colorInput.value = normalized;
        if (/^#[0-9A-F]{8}$/.test(normalized)) {
          colorPicker.value = normalized.substring(0, 7).toLowerCase();
        }
        refreshColorPreview();
      });

      logoPicker.addEventListener(""change"", () => {
        if (logoPicker.files && logoPicker.files.length > 0) {
          const selectedFile = logoPicker.files[0];
          if (!logoInput.value) {
            logoInput.value = selectedFile.name;
          }
          refreshLogoPreview(selectedFile);
          document.getElementById(""status"").textContent = ""Selected file: "" + selectedFile.name + ""\nPaste the real local path into Logo Path if Unreal should load it from disk."";
        } else {
          refreshLogoPreview(null);
        }
      });

      colorInput.dispatchEvent(new Event(""change""));
      refreshColorPreview();
      refreshLogoPreview(null);
    }

    async function refreshState() {
      const response = await fetch(""/mock/state"");
      const payload = await response.json();
      document.getElementById(""runtimeState"").textContent = JSON.stringify(payload.runtimeState, null, 2);
      document.getElementById(""queueState"").textContent = JSON.stringify(payload.pendingQueue, null, 2);
    }

    wireInteractiveInputs();
    refreshState();
    setInterval(refreshState, 1500);
  </script>
</body>
</html>";
        }
    }
}
