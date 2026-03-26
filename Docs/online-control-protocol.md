# Designer Base Online Control Protocol v1

## Overview

This project uses a simple HTTP polling protocol between:

- control sender: local mock sender or future external service
- Unreal runtime: `Designer_Base`

Current Unreal behavior:

- polls `GET /control/next`
- posts runtime state to `POST /control/state`

Base URL example:

```text
http://127.0.0.1:8080
```

## Endpoints

### `GET /control/next`

Used by Unreal to fetch the next pending control command.

Response:

- `200 OK` with one JSON command
- `204 No Content` when no pending command exists

Example:

```json
{
  "requestId": "req-1001",
  "command": "activate",
  "templateCode": "lowerthird_basic"
}
```

### `POST /control/state`

Used by Unreal to push its current runtime state back to the sender side.

Request body example:

```json
{
  "hasActiveTemplate": true,
  "isOnAir": false,
  "activeTemplateCode": "lowerthird_basic",
  "activeTemplateName": "Lower Third Basic",
  "activeFields": {
    "title": "Alice Chen",
    "subtitle": "Beijing Bureau",
    "themeColor": "#FF6600FF",
    "logo": "C:\\assets\\logo.png",
    "showLogo": "true"
  },
  "availableTemplates": []
}
```

Response example:

```json
{
  "ok": true
}
```

## Command Envelope

Every command follows this structure:

```json
{
  "requestId": "req-1002",
  "command": "apply",
  "templateCode": "lowerthird_basic",
  "fields": {
    "title": "Alice Chen",
    "subtitle": "Beijing Bureau",
    "themeColor": "#FF6600FF",
    "logo": "C:\\assets\\logo.png",
    "showLogo": true
  }
}
```

Fields:

- `requestId`: optional unique id from sender
- `command`: required action name
- `templateCode`: required for `activate`, optional otherwise
- `fields`: parameter dictionary for `apply`

## Supported Commands

### `activate`

Activates the target template.

```json
{
  "requestId": "req-activate-01",
  "command": "activate",
  "templateCode": "lowerthird_basic"
}
```

### `apply`

Applies parameter values to the active template.

```json
{
  "requestId": "req-apply-01",
  "command": "apply",
  "fields": {
    "title": "Alice Chen",
    "subtitle": "Beijing Bureau",
    "themeColor": "#FF6600FF",
    "logo": "C:\\assets\\logo.png",
    "showLogo": true
  }
}
```

### `takein`

Starts the lower-third enter animation.

```json
{
  "requestId": "req-in-01",
  "command": "takein"
}
```

### `takeout`

Starts the lower-third exit animation.

```json
{
  "requestId": "req-out-01",
  "command": "takeout"
}
```

### `clear`

Clears the current active template.

```json
{
  "requestId": "req-clear-01",
  "command": "clear"
}
```

### `refresh`

Refreshes the template catalog.

```json
{
  "requestId": "req-refresh-01",
  "command": "refresh"
}
```

### `status`

Requests current runtime status on next poll cycle.

```json
{
  "requestId": "req-status-01",
  "command": "status"
}
```

## Unreal Result Format

When Unreal executes a command internally, result JSON follows this shape:

```json
{
  "success": true,
  "requestId": "req-apply-01",
  "command": "apply",
  "message": "Fields applied.",
  "state": {
    "hasActiveTemplate": true,
    "isOnAir": true,
    "activeTemplateCode": "lowerthird_basic",
    "activeTemplateName": "Lower Third Basic",
    "activeFields": {
      "title": "Alice Chen"
    },
    "availableTemplates": []
  }
}
```

## Local Mock Sender

Local mock sender options:`r`n`r`n- [StartMockControlServer.bat](F:\FoX_CodeX\Designer_Base\Tools\StartMockControlServer.bat) (recommended, launches the Node.js sender UI)`r`n- [MockControlServer.ps1](F:\FoX_CodeX\Designer_Base\Tools\MockControlServer.ps1) (legacy PowerShell sender)

Run:`r`n`r`n```bat`r`nF:\FoX_CodeX\Designer_Base\Tools\StartMockControlServer.bat`r`n```

Open in browser:

```text
http://127.0.0.1:8080/ui
```

This mock sender provides:

- queueing commands to `/control/next`
- receiving Unreal state on `/control/state`
- a browser UI for local testing

