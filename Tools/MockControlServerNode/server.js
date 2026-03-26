const http = require('http');
const fs = require('fs');
const net = require('net');
const path = require('path');
const { URL } = require('url');

const HOST = '127.0.0.1';
const PORT = 8080;
const TCP_PORT = 18081;
const LONG_POLL_TIMEOUT_MS = 25000;
const SSE_RETRY_MS = 1000;
const PUBLIC_DIR = path.join(__dirname, 'public');
const UPLOAD_DIR = path.join(__dirname, 'uploads');
const LOG_PATH = path.join(__dirname, 'node-server.log');

let latestRuntimeState = {
  ok: false,
  message: 'No runtime state received yet.'
};

const commandQueue = [];
const pendingPolls = [];
const eventClients = new Set();
const runtimeClients = new Set();

fs.mkdirSync(UPLOAD_DIR, { recursive: true });

function logLine(message) {
  const line = `[${new Date().toISOString()}] ${message}\n`;
  fs.appendFileSync(LOG_PATH, line);
  console.log(message);
}

function sendJson(res, statusCode, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(statusCode, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(body),
    'Cache-Control': 'no-store',
    Connection: 'close'
  });
  res.end(body);
}

function sendText(res, statusCode, contentType, body) {
  res.writeHead(statusCode, {
    'Content-Type': contentType,
    'Content-Length': Buffer.byteLength(body),
    'Cache-Control': 'no-store',
    Connection: 'close'
  });
  res.end(body);
}

function sendNoContent(res) {
  res.writeHead(204, {
    'Cache-Control': 'no-store',
    Connection: 'close'
  });
  res.end();
}

function getSafeUploadExtension(filename) {
  const ext = path.extname(filename || '').toLowerCase();
  const allowed = new Set(['.png', '.jpg', '.jpeg', '.bmp', '.webp', '.tga']);
  return allowed.has(ext) ? ext : '.png';
}

function saveUploadedLogo(filename, dataUrl) {
  if (typeof dataUrl !== 'string' || !dataUrl.startsWith('data:')) {
    throw new Error('Invalid image payload.');
  }

  const commaIndex = dataUrl.indexOf(',');
  if (commaIndex < 0) {
    throw new Error('Invalid image payload.');
  }

  const base64Payload = dataUrl.slice(commaIndex + 1);
  const fileBuffer = Buffer.from(base64Payload, 'base64');
  const timestamp = Date.now();
  const ext = getSafeUploadExtension(filename);
  const savedPath = path.join(UPLOAD_DIR, `logo-${timestamp}${ext}`);
  fs.writeFileSync(savedPath, fileBuffer);
  return savedPath;
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    req.on('data', (chunk) => chunks.push(chunk));
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    req.on('error', reject);
  });
}

function buildStatePayload() {
  return {
    runtimeState: latestRuntimeState,
    pendingQueue: commandQueue.slice(),
    realtimeConnected: runtimeClients.size > 0
  };
}

function broadcastEvent(type, payload) {
  const serialized = `event: ${type}\ndata: ${JSON.stringify(payload)}\n\n`;
  for (const client of eventClients) {
    client.write(serialized);
  }
}

function removeRuntimeClient(client) {
  if (!runtimeClients.has(client)) {
    return;
  }

  runtimeClients.delete(client);
  broadcastEvent('state', buildStatePayload());
}

function handleRuntimeMessage(message) {
  try {
    const payload = JSON.parse(message);
    if (payload.type === 'runtime_state' && payload.state) {
      latestRuntimeState = payload.state;
      broadcastEvent('state', buildStatePayload());
    }
  } catch (error) {
    logLine(`Failed to parse runtime TCP message: ${error.message}`);
  }
}

function sendRuntimeClientJson(client, payload) {
  client.socket.write(`${JSON.stringify(payload)}\n`);
}

function dispatchCommand(command) {
  if (runtimeClients.size > 0) {
    const payload = { type: 'command', payload: command };
    for (const client of runtimeClients) {
      sendRuntimeClientJson(client, payload);
    }
    broadcastEvent('state', buildStatePayload());
    return true;
  }

  return false;
}

function flushNextCommand() {
  while (pendingPolls.length > 0) {
    const poll = pendingPolls.shift();
    clearTimeout(poll.timeoutId);

    if (poll.res.writableEnded || poll.res.destroyed) {
      continue;
    }

    if (commandQueue.length === 0) {
      sendNoContent(poll.res);
      continue;
    }

    const next = commandQueue.shift();
    sendJson(poll.res, 200, next);
    broadcastEvent('state', buildStatePayload());
    return;
  }
}

function queueCommand(command) {
  if (dispatchCommand(command)) {
    return { directDispatch: true, queueLength: 0 };
  }

  commandQueue.push(command);
  flushNextCommand();
  broadcastEvent('state', buildStatePayload());
  return { directDispatch: false, queueLength: commandQueue.length };
}

function handleLongPoll(res) {
  if (commandQueue.length > 0) {
    const next = commandQueue.shift();
    sendJson(res, 200, next);
    broadcastEvent('state', buildStatePayload());
    return;
  }

  const poll = {
    res,
    timeoutId: setTimeout(() => {
      const index = pendingPolls.indexOf(poll);
      if (index >= 0) {
        pendingPolls.splice(index, 1);
      }
      if (!res.writableEnded && !res.destroyed) {
        sendNoContent(res);
      }
    }, LONG_POLL_TIMEOUT_MS)
  };

  pendingPolls.push(poll);
  res.on('close', () => {
    const index = pendingPolls.indexOf(poll);
    if (index >= 0) {
      pendingPolls.splice(index, 1);
    }
    clearTimeout(poll.timeoutId);
  });
}

function serveStaticFile(res, filePath) {
  if (!fs.existsSync(filePath)) {
    sendJson(res, 404, { ok: false, message: 'Not Found' });
    return;
  }

  const ext = path.extname(filePath).toLowerCase();
  const contentType = ext === '.css'
    ? 'text/css; charset=utf-8'
    : ext === '.js'
      ? 'application/javascript; charset=utf-8'
      : 'text/html; charset=utf-8';

  sendText(res, 200, contentType, fs.readFileSync(filePath, 'utf8'));
}

const server = http.createServer(async (req, res) => {
  try {
    const requestUrl = new URL(req.url, `http://${HOST}:${PORT}`);
    const pathname = requestUrl.pathname;

    if (req.method === 'GET' && (pathname === '/' || pathname === '/ui')) {
      serveStaticFile(res, path.join(PUBLIC_DIR, 'index.html'));
      return;
    }

    if (req.method === 'GET' && pathname === '/styles.css') {
      serveStaticFile(res, path.join(PUBLIC_DIR, 'styles.css'));
      return;
    }

    if (req.method === 'GET' && pathname === '/app.js') {
      serveStaticFile(res, path.join(PUBLIC_DIR, 'app.js'));
      return;
    }

    if (req.method === 'GET' && pathname === '/control/next') {
      handleLongPoll(res);
      return;
    }

    if (req.method === 'POST' && pathname === '/control/state') {
      const body = await readBody(req);
      if (body.trim()) {
        latestRuntimeState = JSON.parse(body);
      }
      logLine('HTTP runtime state received.');
      sendJson(res, 200, { ok: true, message: 'Runtime state received.' });
      broadcastEvent('state', buildStatePayload());
      return;
    }

    if (req.method === 'POST' && pathname === '/mock/enqueue') {
      const body = await readBody(req);
      const command = JSON.parse(body || '{}');
      const result = queueCommand(command);
      logLine(`Command received via UI/API: ${command.command || 'unknown'} direct=${result.directDispatch}`);
      sendJson(res, 200, {
        ok: true,
        message: result.directDispatch ? `Dispatched command: ${command.command || 'unknown'}` : `Queued command: ${command.command || 'unknown'}`,
        queueLength: result.queueLength,
        directDispatch: result.directDispatch
      });
      return;
    }

    if (req.method === 'POST' && pathname === '/mock/upload-logo') {
      const body = await readBody(req);
      const payload = JSON.parse(body || '{}');
      const savedPath = saveUploadedLogo(payload.filename, payload.dataUrl);
      logLine(`Logo uploaded: ${savedPath}`);
      sendJson(res, 200, {
        ok: true,
        message: 'Logo uploaded.',
        savedPath
      });
      return;
    }

    if (req.method === 'GET' && pathname === '/mock/state') {
      sendJson(res, 200, buildStatePayload());
      return;
    }

    if (req.method === 'GET' && pathname === '/mock/events') {
      res.writeHead(200, {
        'Content-Type': 'text/event-stream; charset=utf-8',
        'Cache-Control': 'no-cache, no-store, must-revalidate',
        Connection: 'keep-alive',
        'X-Accel-Buffering': 'no'
      });
      res.write(`retry: ${SSE_RETRY_MS}\n\n`);
      res.write(`event: state\ndata: ${JSON.stringify(buildStatePayload())}\n\n`);
      eventClients.add(res);
      req.on('close', () => eventClients.delete(res));
      return;
    }

    sendJson(res, 404, { ok: false, message: 'Not Found' });
  } catch (error) {
    sendJson(res, 500, {
      ok: false,
      message: error instanceof Error ? error.message : String(error)
    });
  }
});

const tcpServer = net.createServer((socket) => {
  const client = { socket, buffer: '' };
  runtimeClients.add(client);
  logLine(`Realtime TCP client connected: ${socket.remoteAddress || 'unknown'}:${socket.remotePort || 0}`);
  broadcastEvent('state', buildStatePayload());

  socket.setEncoding('utf8');
  socket.on('data', (chunk) => {
    client.buffer += chunk;
    let newlineIndex = client.buffer.indexOf('\n');
    while (newlineIndex >= 0) {
      const line = client.buffer.slice(0, newlineIndex).trim();
      client.buffer = client.buffer.slice(newlineIndex + 1);
      if (line) {
        handleRuntimeMessage(line);
      }
      newlineIndex = client.buffer.indexOf('\n');
    }
  });

  socket.on('close', () => {
    logLine('Realtime TCP client closed.');
    removeRuntimeClient(client);
  });

  socket.on('error', (error) => {
    logLine(`Realtime TCP client error: ${error.message}`);
    removeRuntimeClient(client);
  });

  while (commandQueue.length > 0) {
    sendRuntimeClientJson(client, { type: 'command', payload: commandQueue.shift() });
  }
  broadcastEvent('state', buildStatePayload());
});

server.listen(PORT, HOST, () => {
  logLine(`Designer Node control server started at http://${HOST}:${PORT}/ui`);
});

tcpServer.listen(TCP_PORT, HOST, () => {
  logLine(`Designer TCP runtime bridge listening at ${HOST}:${TCP_PORT}`);
});
