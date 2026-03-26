const titleInput = document.getElementById('title');
const subtitleInput = document.getElementById('subtitle');
const themeColorInput = document.getElementById('themeColor');
const themeColorPicker = document.getElementById('themeColorPicker');
const themeColorPreview = document.getElementById('themeColorPreview');
const logoInput = document.getElementById('logo');
const logoPicker = document.getElementById('logoPicker');
const logoPreviewImage = document.getElementById('logoPreviewImage');
const logoPreviewEmpty = document.getElementById('logoPreviewEmpty');
const showLogoSelect = document.getElementById('showLogo');
const templateCodeSelect = document.getElementById('templateCode');
const statusBox = document.getElementById('statusBox');
const runtimeStateBox = document.getElementById('runtimeState');
const queueStateBox = document.getElementById('queueState');
const realtimeBadge = document.getElementById('realtimeBadge');

let currentPreviewUrl = null;
let uploadedLogoPath = '';

function requestId(prefix) {
  return `${prefix}-${Date.now()}`;
}

function normalizeThemeColor(value) {
  if (!value) return '';
  const trimmed = value.trim();
  if (/^#[0-9a-fA-F]{6}$/.test(trimmed)) return `${trimmed}FF`.toUpperCase();
  if (/^#[0-9a-fA-F]{8}$/.test(trimmed)) return trimmed.toUpperCase();
  return trimmed;
}

function setStatus(payload) {
  statusBox.textContent = typeof payload === 'string' ? payload : JSON.stringify(payload, null, 2);
}

function renderState(payload) {
  runtimeStateBox.textContent = JSON.stringify(payload.runtimeState, null, 2);
  queueStateBox.textContent = JSON.stringify(payload.pendingQueue, null, 2);
  realtimeBadge.textContent = payload.realtimeConnected ? 'Realtime: Connected' : 'Realtime: Waiting for Unreal';
  realtimeBadge.classList.toggle('live', Boolean(payload.realtimeConnected));
}

function refreshColorPreview() {
  const normalized = normalizeThemeColor(themeColorInput.value);
  themeColorPreview.style.background = normalized || '#101a2c';
  if (/^#[0-9A-F]{8}$/.test(normalized)) {
    themeColorPicker.value = normalized.slice(0, 7).toLowerCase();
  }
}

function refreshLogoPreview(file) {
  if (currentPreviewUrl) {
    URL.revokeObjectURL(currentPreviewUrl);
    currentPreviewUrl = null;
  }

  if (!file) {
    logoPreviewImage.style.display = 'none';
    logoPreviewEmpty.style.display = 'grid';
    logoPreviewImage.removeAttribute('src');
    return;
  }

  currentPreviewUrl = URL.createObjectURL(file);
  logoPreviewImage.src = currentPreviewUrl;
  logoPreviewImage.style.display = 'block';
  logoPreviewEmpty.style.display = 'none';
}

function readFields() {
  return {
    title: titleInput.value,
    subtitle: subtitleInput.value,
    themeColor: normalizeThemeColor(themeColorInput.value),
    logo: logoInput.value.trim() || uploadedLogoPath,
    showLogo: showLogoSelect.value === 'true'
  };
}

async function postJson(url, payload) {
  const response = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  return response.json();
}

async function enqueueCommand(command) {
  const result = await postJson('/mock/enqueue', command);
  setStatus(result);
}

async function uploadLogoFile(file) {
  const reader = new FileReader();
  const dataUrl = await new Promise((resolve, reject) => {
    reader.onload = () => resolve(reader.result);
    reader.onerror = () => reject(reader.error || new Error('Failed to read file.'));
    reader.readAsDataURL(file);
  });

  const result = await postJson('/mock/upload-logo', {
    filename: file.name,
    dataUrl
  });

  if (!result.ok) {
    throw new Error(result.message || 'Logo upload failed.');
  }

  uploadedLogoPath = result.savedPath || '';
  if (!logoInput.value.trim()) {
    logoInput.value = uploadedLogoPath;
  }

  return result;
}

async function refreshState() {
  const response = await fetch('/mock/state');
  const payload = await response.json();
  renderState(payload);
}

function wireEvents() {
  document.getElementById('sendActivate').addEventListener('click', async () => {
    await enqueueCommand({ requestId: requestId('activate'), command: 'activate', templateCode: templateCodeSelect.value });
  });

  document.getElementById('sendApply').addEventListener('click', async () => {
    await enqueueCommand({ requestId: requestId('apply'), command: 'apply', fields: readFields() });
  });

  document.getElementById('sendTakeIn').addEventListener('click', async () => {
    await enqueueCommand({ requestId: requestId('activate'), command: 'activate', templateCode: templateCodeSelect.value });
    await enqueueCommand({ requestId: requestId('apply'), command: 'apply', fields: readFields() });
    await enqueueCommand({ requestId: requestId('takein'), command: 'takein' });
  });

  document.getElementById('sendTakeOut').addEventListener('click', async () => {
    await enqueueCommand({ requestId: requestId('takeout'), command: 'takeout' });
  });

  document.getElementById('sendClear').addEventListener('click', async () => {
    await enqueueCommand({ requestId: requestId('clear'), command: 'clear' });
  });

  document.getElementById('refreshState').addEventListener('click', refreshState);

  document.getElementById('sendRaw').addEventListener('click', async () => {
    const payload = JSON.parse(document.getElementById('rawCommand').value);
    await enqueueCommand(payload);
  });

  themeColorPicker.addEventListener('input', () => {
    themeColorInput.value = `${themeColorPicker.value.toUpperCase()}FF`;
    refreshColorPreview();
  });

  themeColorInput.addEventListener('change', refreshColorPreview);

  logoInput.addEventListener('input', () => {
    uploadedLogoPath = '';
  });

  logoPicker.addEventListener('change', () => {
    if (logoPicker.files && logoPicker.files.length > 0) {
      const selectedFile = logoPicker.files[0];
      refreshLogoPreview(selectedFile);
      setStatus(`Uploading logo: ${selectedFile.name}`);
      uploadLogoFile(selectedFile)
        .then((result) => {
          logoInput.value = result.savedPath || '';
          setStatus(`Logo uploaded.\nSaved to: ${result.savedPath}`);
        })
        .catch((error) => {
          uploadedLogoPath = '';
          setStatus(`Logo upload failed: ${error.message}`);
        });
      return;
    }
    uploadedLogoPath = '';
    refreshLogoPreview(null);
  });
}

function connectEvents() {
  const eventSource = new EventSource('/mock/events');
  eventSource.addEventListener('state', (event) => {
    renderState(JSON.parse(event.data));
  });
  eventSource.onerror = () => {
    setStatus('Event stream disconnected. Trying to reconnect...');
  };
}

wireEvents();
refreshColorPreview();
refreshLogoPreview(null);
refreshState();
connectEvents();
