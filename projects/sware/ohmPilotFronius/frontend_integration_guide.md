# Frontend Integration Guide: Weather Prediction and Early Heating Mode

## Required Changes in Vue (HomeView Component)

### 1. Data Properties Addition
In your HomeView component's data section, add:
```javascript
weatherPredictionUsed: false,
earlyHeatingMode: false,
```

### 2. WebSocket Message Handler Update
Locate where WebSocket messages are processed and update it to read new fields:
```javascript
// In your websocket message handler
if (message.live) {
    if (message.live.weatherPredictionUsed !== undefined) {
        this.weatherPredictionUsed = message.live.weatherPredictionUsed === 1;
    }
    if (message.live.earlyHeatingMode !== undefined) {
        this.earlyHeatingMode = message.live.earlyHeatingMode === 1;
    }
}
```

### 3. Template Updates
Add display elements to your Vue template:
```html
<div class="status-panel">
    <div class="status-item">
        <span class="label">Weather Prediction:</span>
        <span class="value" :class="{ active: weatherPredictionUsed }">
            {{ weatherPredictionUsed ? 'Active' : 'Inactive' }}
        </span>
    </div>
    <div class="status-item">
        <span class="label">Early Heating:</span>
        <span class="value" :class="{ active: earlyHeatingMode }">
            {{ earlyHeatingMode ? 'Enabled' : 'Disabled' }}
        </span>
    </div>
</div>
```

### 4. Optional CSS Styling
Add to your styles to indicate status:
```css
.status-panel {
    margin: 10px 0;
    padding: 10px;
    border: 1px solid #ddd;
}

.status-item {
    display: flex;
    justify-content: space-between;
    margin: 5px 0;
}

.label {
    font-weight: bold;
}

.value.active {
    color: green;
    font-weight: bold;
}
```

## Complete Integration Requirements

- **File to modify**: Your Vue component file (likely in data/assets/ or similar)
- **Fields available**: weatherPredictionUsed, earlyHeatingMode (sent from backend)
- **Logic**: These fields will be 1 when active, 0 when inactive
- **Frequency**: Updated with each WebSocket message

## Verification Steps
1. Ensure WebSocket connection is working
2. Check that new fields appear in the console output
3. Verify data displays correctly in UI
4. Test with actual weather prediction and early heating scenarios

The backend now sends these fields through the existing WebSocket infrastructure, so no additional networking changes are required.