// Configuration
const DEVICE_NAME = "NanoESP32-Willow-BLE";
const SERVICE_UUID = "55f8a5ee-886f-4929-a3ab-5745cbbceab5";
const CMD_CHAR_UUID = "30bce33a-6e60-4306-ad36-8718f82ee801";
const DATA_CHAR_UUID = "fef211ef-bc20-4f2f-9e9d-3cb6c7b6f772";
const METRICS_CHAR_UUID = "555ff5a9-d76d-4945-b7a2-f26612fc5be5";
const ACK_CHAR_UUID = "a2a1c8b3-4d7e-4f2a-9b1c-8e7d5f3a2b1c";
const POSITION1_CHAR_UUID = "d8cfb4a5-7d1b-4d40-8f7f-f6a7f4f0c4e1";
const POSITION2_CHAR_UUID = "4f5cc7fd-1f5c-4aab-82fb-0d6f3dbe7b2b";
const ACCESS_PASSWORD = "Pocki06";

const COUNTS_PER_REVOLUTION = 1727 * 2;
const LIFT_MM_PER_REVOLUTION = 8;

// ACK status codes
const ACK_SUCCESS = 0x00;
const ACK_ERROR = 0x01;

// State variables
let unlocked = false;
let statusEl = document.getElementById("status");
let motorTestRunning = false;
let bluetoothTestRunning = false;
let currentViewTab = "testing";

// Connection state
let device = null;
let cmdChar = null;
let dataChar = null;
let metricsChar = null;
let ackChar = null;
let position1Char = null;
let position2Char = null;
let connected = false;
let isReconnecting = false;
let reconnectTimerId = null;
let reconnectAttempts = 0;
let manualDisconnectRequested = false;

const RECONNECT_INTERVAL_MS = 1000;
const MAX_AUTO_RECONNECT_ATTEMPTS = 10;

// Motor test configuration
let motorConfig = {
    degrees: 75, // Degrees to rotate (5-180)
    speed: 20, // Motor speed percentage (5-100)
    positionReset: false, // Whether to return motors to the website-tracked 0 degree position
    reverse: false // Whether to reverse motor direction
};
let totalRotation = 0; // Track total rotation for motor test
let activeMotorCommandLabel = "Motor Test";
let userViewStepMm = 5;
let userViewTargetMm = 30;
let userViewCommandedTotalMm = 0;
let userViewMotorCommandPending = false;

// Bluetooth test configuration
let bluetoothConfig = {
    sendIntervalMs: 1000, // Interval between packets for bluetooth test in milliseconds (50-2000)
    inSteps: false // Whether to wait for user prompt between packets instead of sending automatically
};
let packetSeq = 0; // Sequence number for bluetooth test packets
let minIntervalMs = 10; // Minimum interval between packets

// Metrics computation state
let lastMetricsArrivalUs = null;
let intervalDeltasUs = [];

let latestPosition1Counts = null;
let latestPosition2Counts = null;
let trackedPosition1Counts = null;
let trackedPosition2Counts = null;

const GRAPH_WINDOW_MS = 120000;
let graphHistory = [];
let graphAnimationFrameId = null;

const COUNTER_RESET_NEAR_ZERO_COUNTS = 20;
const COUNTER_RESET_JUMP_THRESHOLD_COUNTS = 100;

// Sequence-matched link metrics from data echo packets
const pendingPackets = new Map();
const MAX_PENDING_PACKETS = 400;
const RTT_SAMPLE_WINDOW = 100;
const MIN_PACKET_TIMEOUT_US = 400000;
const rttSamplesUs = [];

let sentPacketCount = 0;
let echoedPacketCount = 0;
let echoedPayloadBytes = 0;
let perErrorPackets = 0;
let timedOutPacketCount = 0;
let btTestStartUs = null;

// Command IDs
const STOP_MOTOR = 0b00;
const START_MOTOR = 0b01;
const STOP_BT = 0b10;
const START_BT = 0b11;

function updateUI() {
    const loginSection = document.getElementById("login-section");
    loginSection.classList.toggle("hidden", unlocked);

    const controlPanel = document.getElementById("control-panel");
    controlPanel.classList.toggle("hidden", !unlocked);

    const connectBtn = document.getElementById("connect-btn");
    const disconnectBtn = document.getElementById("disconnect-btn");

    connectBtn.disabled = connected || isReconnecting;
    if (connected) {
        connectBtn.textContent = "Connected";
    } else if (isReconnecting) {
        connectBtn.textContent = "Reconnecting...";
    } else {
        connectBtn.textContent = "Connect";
    }
    connectBtn.classList.toggle("connecting", isReconnecting && !connected);
    connectBtn.classList.toggle("connected", connected);
    disconnectBtn.classList.toggle("hidden", !connected);

    const configBoxes = document.querySelectorAll(".config-box");
    configBoxes.forEach(box => box.classList.toggle("hidden", !connected));

    const testButtons = document.querySelectorAll(".test-btn");

    testButtons.forEach(btn => {
        const testType = btn.dataset.test;

        const isRunning =
            (testType === "motor" && motorTestRunning) ||
            (testType === "bluetooth" && bluetoothTestRunning);

        btn.classList.toggle("hidden", !connected);

        if (connected && isRunning) {
            btn.disabled = true;
            btn.classList.add("running");
            btn.textContent = btn.dataset.runningLabel;
        } else {
            btn.disabled = !connected;
            btn.classList.remove("running");
            btn.textContent = btn.dataset.idleLabel;
        }
    });

    const stopBtn = document.getElementById("stop-btn");
    const anyTestRunning = motorTestRunning || bluetoothTestRunning;

    stopBtn.disabled = !connected || !anyTestRunning;
    stopBtn.classList.toggle("hidden", !connected || !anyTestRunning);

    if (!connected && isReconnecting) {
        statusEl.textContent = "Connection lost. Reconnecting...";
        statusEl.className = "status disconnected";
    } else if (!connected) {
        statusEl.textContent = "Disconnected";
        statusEl.className = "status disconnected";
    } else if (anyTestRunning) {
        statusEl.textContent = "Connected - Test Running";
        statusEl.className = "status connected";
    } else {
        statusEl.textContent = "Connected";
        statusEl.className = "status connected";
    }

    const testingTabBtn = document.getElementById("testing-view-tab-btn");
    const userTabBtn = document.getElementById("user-view-tab-btn");
    const testingView = document.getElementById("testing-view");
    const userView = document.getElementById("user-view");

    const showTestingTab = currentViewTab === "testing";
    testingTabBtn.classList.toggle("active", showTestingTab);
    userTabBtn.classList.toggle("active", !showTestingTab);
    testingView.classList.toggle("hidden", !showTestingTab);
    userView.classList.toggle("hidden", showTestingTab);

    updateUserViewControls();
}

function updateUserViewControls() {
    const userStepInput = document.getElementById("user-step-mm-input");
    const userStepHint = document.getElementById("user-step-cm-hint");
    const userTargetInput = document.getElementById("user-target-mm-input");
    const userTargetHint = document.getElementById("user-target-cm-hint");
    const userStartBtn = document.getElementById("user-start-btn");
    const userFeedback = document.getElementById("user-feedback");
    const progressEl = document.getElementById("user-rotation-progress");
    const graphTargetLabel = document.getElementById("graph-target-label");

    const remainingMm = Math.max(0, userViewTargetMm - userViewCommandedTotalMm);
    const canRun = connected && !userViewMotorCommandPending && remainingMm >= userViewStepMm;

    progressEl.textContent = `Total commanded: ${userViewCommandedTotalMm.toFixed(1)} mm / ${userViewTargetMm.toFixed(1)} mm`;
    userStepHint.textContent = `${mmToCm(userViewStepMm).toFixed(2)} cm per actuation`;
    userTargetHint.textContent = `${mmToCm(userViewTargetMm).toFixed(2)} cm target`;
    graphTargetLabel.textContent = `Target: ${userViewTargetMm.toFixed(1)} mm`;

    userStepInput.value = String(userViewStepMm);
    userTargetInput.value = String(userViewTargetMm);
    userStepInput.disabled = !connected || userViewMotorCommandPending;
    userTargetInput.disabled = !connected || userViewMotorCommandPending;
    userStartBtn.disabled = !canRun;

    if (!connected) {
        userFeedback.textContent = "Connect to enable User View actuation.";
        userFeedback.className = "status disconnected";
        return;
    }

    if (remainingMm < userViewStepMm) {
        userFeedback.textContent = "Lift target reached for this session.";
        userFeedback.className = "status disconnected";
        return;
    }

    if (userViewMotorCommandPending) {
        userFeedback.textContent = "Actuation in progress...";
        userFeedback.className = "status connected";
        return;
    }

    userFeedback.textContent = `Ready: next actuation ${userViewStepMm.toFixed(1)} mm at 5% speed.`;
    userFeedback.className = "status connected";
}

function unlock() {
    const entered = document.getElementById("password-input").value;
    const status = document.getElementById("login-status");

    if (entered === ACCESS_PASSWORD) {
        unlocked = true;
        updateUI();
    } else {
        status.textContent = "Incorrect password";
        status.style.color = "red";
    }
}

async function connectBLE() {
    if (!unlocked) return;

    try {
        clearReconnectTimer();
        manualDisconnectRequested = false;
        isReconnecting = false;
        reconnectAttempts = 0;

        document.getElementById("connect-btn").textContent = "Connecting...";
        document.getElementById("connect-btn").classList.add("connecting");

        if (!device) {
            device = await navigator.bluetooth.requestDevice({
                filters: [{
                    services: [SERVICE_UUID]
                }]
            });

            device.addEventListener("gattserverdisconnected", handleGattDisconnected);
        }

        await connectToDevice(device);
    } catch (error) {
        console.error(error);
        isReconnecting = false;
        statusEl.textContent = "Connection failed";
        statusEl.className = "status disconnected";
        document.getElementById("connect-btn").classList.remove("connecting");
        updateUI();
        logCommand("Connection failed");
    }
}

async function connectToDevice(targetDevice) {
    const server = await targetDevice.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);

    cmdChar = await service.getCharacteristic(CMD_CHAR_UUID);
    dataChar = await service.getCharacteristic(DATA_CHAR_UUID);
    metricsChar = await service.getCharacteristic(METRICS_CHAR_UUID);
    ackChar = await service.getCharacteristic(ACK_CHAR_UUID);
    position1Char = await service.getCharacteristic(POSITION1_CHAR_UUID);
    position2Char = await service.getCharacteristic(POSITION2_CHAR_UUID);

    dataChar.removeEventListener("characteristicvaluechanged", handleDataEchoNotification);
    metricsChar.removeEventListener("characteristicvaluechanged", handleMetricsNotification);
    ackChar.removeEventListener("characteristicvaluechanged", handleAckNotification);
    position1Char.removeEventListener("characteristicvaluechanged", handlePosition1Notification);
    position2Char.removeEventListener("characteristicvaluechanged", handlePosition2Notification);

    await dataChar.startNotifications();
    dataChar.addEventListener("characteristicvaluechanged", handleDataEchoNotification);

    await metricsChar.startNotifications();
    metricsChar.addEventListener("characteristicvaluechanged", handleMetricsNotification);

    await ackChar.startNotifications();
    ackChar.addEventListener("characteristicvaluechanged", handleAckNotification);

    await position1Char.startNotifications();
    position1Char.addEventListener("characteristicvaluechanged", handlePosition1Notification);

    await position2Char.startNotifications();
    position2Char.addEventListener("characteristicvaluechanged", handlePosition2Notification);

    connected = true;
    isReconnecting = false;
    reconnectAttempts = 0;

    document.getElementById("connect-btn").classList.remove("connecting");
    updateUI();
    logCommand(`Connected to ${DEVICE_NAME}`);
    logCommand("Data echo notifications enabled");
    logCommand("Metrics notifications enabled");
    logCommand("ACK notifications enabled");
    logCommand("Position notifications enabled for both motor pairs");
}

function clearReconnectTimer() {
    if (reconnectTimerId !== null) {
        clearTimeout(reconnectTimerId);
        reconnectTimerId = null;
    }
}

function scheduleReconnect() {
    if (!device || manualDisconnectRequested || !unlocked || reconnectTimerId !== null) {
        return;
    }

    if (reconnectAttempts >= MAX_AUTO_RECONNECT_ATTEMPTS) {
        isReconnecting = false;
        reconnectAttempts = 0;
        document.getElementById("connect-btn").classList.remove("connecting");
        updateUI();
        statusEl.textContent = "Connection failed";
        statusEl.className = "status disconnected";
        logCommand("Connection failed");
        return;
    }

    const delayMs = RECONNECT_INTERVAL_MS;
    reconnectAttempts++;
    isReconnecting = true;

    updateUI();
    logCommand(`Connection lost. Reconnect attempt ${reconnectAttempts} in ${delayMs}ms`);

    reconnectTimerId = setTimeout(async () => {
        reconnectTimerId = null;

        if (!device || manualDisconnectRequested || connected) return;

        try {
            document.getElementById("connect-btn").textContent = "Reconnecting...";
            document.getElementById("connect-btn").classList.add("connecting");
            await connectToDevice(device);
            logCommand("Auto-reconnect successful");
        } catch (error) {
            console.error("Auto-reconnect failed:", error);
            connected = false;
            cmdChar = null;
            dataChar = null;
            metricsChar = null;
            ackChar = null;
            position1Char = null;
            position2Char = null;
            scheduleReconnect();
        }
    }, delayMs);
}

function handleGattDisconnected() {
    connected = false;
    cmdChar = null;
    dataChar = null;
    metricsChar = null;
    ackChar = null;
    position1Char = null;
    position2Char = null;
    motorTestRunning = false;
    bluetoothTestRunning = false;
    userViewCommandedTotalMm = 0;
    userViewMotorCommandPending = false;
    resetPositionDisplay();

    if (manualDisconnectRequested) {
        isReconnecting = false;
        reconnectAttempts = 0;
        document.getElementById("connect-btn").classList.remove("connecting");
        updateUI();
        manualDisconnectRequested = false;
        return;
    }

    isReconnecting = true;
    updateUI();
    scheduleReconnect();
}

function disconnectBLE() {
    if (!device) return;

    manualDisconnectRequested = true;
    clearReconnectTimer();
    connected = false;
    isReconnecting = false;
    reconnectAttempts = 0;

    if (device?.gatt?.connected) {
        device.gatt.disconnect();
    }

    cmdChar = null;
    dataChar = null;
    metricsChar = null;
    ackChar = null;
    position1Char = null;
    position2Char = null;
    motorTestRunning = false;
    bluetoothTestRunning = false;
    userViewCommandedTotalMm = 0;
    userViewMotorCommandPending = false;
    resetPositionDisplay();

    document.getElementById("connect-btn").classList.remove("connecting");
    updateUI();
    logCommand(`Disconnected from ${DEVICE_NAME}`);
}

async function startMotorTest() {
    if (!connected) {
        alert("Not connected");
        logCommand("Failed to start motor test - not connected");
        return;
    }

    if (motorTestRunning) {
        alert("Motor test already running");
        return;
    }

    if (motorConfig.positionReset) {
        const sent = await sendPositionResetCommand();
        if (!sent) {
            return;
        }

        activeMotorCommandLabel = "Position Reset";
        motorTestRunning = true;
        updateUI();
        logCommand("Position Reset started, waiting for ACK");
        return;
    }

    activeMotorCommandLabel = "Motor Test";

    await sendCommandPacket(START_MOTOR);
    motorTestRunning = true;
    updateUI();
    logCommand("Motor test started, waiting for ACK");
}

async function startUserViewActuation() {
    if (!connected) {
        alert("Not connected");
        return;
    }

    if (userViewMotorCommandPending) {
        return;
    }

    const nextTotalMm = userViewCommandedTotalMm + userViewStepMm;
    if (nextTotalMm > userViewTargetMm) {
        logCommand("Blocked User View command - would exceed user target lift distance");
        updateUserViewControls();
        return;
    }

    userViewMotorCommandPending = true;
    activeMotorCommandLabel = "User View Actuation";
    motorTestRunning = true;
    updateUI();

    const userStepDeg = mmToDegrees(userViewStepMm);
    const motorDegrees = Math.max(1, Math.round(Math.abs(userStepDeg)));

    const sent = await sendCommandPacket(START_MOTOR, {
        motorDegrees: motorDegrees,
        motorSpeed: 5,
        motorReverse: false,
        commandLabel: "User View Start Motor"
    });

    if (!sent) {
        userViewMotorCommandPending = false;
        motorTestRunning = false;
        updateUI();
        return;
    }

    userViewCommandedTotalMm = nextTotalMm;
    logCommand(`User View actuation started: lift=${userViewStepMm.toFixed(1)}mm, degrees=${motorDegrees}, speed=5%, reverse=0`);
    updateUI();
}

async function startBluetoothTest() {
    if (!connected) {
        alert("Not connected");
        logCommand("Failed to start Bluetooth test - not connected");
        return;
    }

    if (bluetoothTestRunning) {
        alert("Bluetooth test already running");
        return;
    }

    packetSeq = 0;
    pendingPackets.clear();
    rttSamplesUs.length = 0;
    sentPacketCount = 0;
    echoedPacketCount = 0;
    echoedPayloadBytes = 0;
    perErrorPackets = 0;
    timedOutPacketCount = 0;
    btTestStartUs = performance.now() * 1000;

    bluetoothTestRunning = true;
    updateUI();

    await flushBLE();
    await sendCommandPacket(START_BT);

    await delay(150);
    sendDataPacket();
}

async function stopTests() {
    if (!connected) {
        alert("Not connected");
        return;
    }

    if (motorTestRunning) await sendCommandPacket(STOP_MOTOR);
    if (bluetoothTestRunning) await sendCommandPacket(STOP_BT);

    motorTestRunning = false;
    bluetoothTestRunning = false;
    updateUI();
    logCommand("Stopped all tests");
}

function getCurrentPositionCounts(pairNumber) {
    if (pairNumber === 1) {
        if (trackedPosition1Counts === null) {
            return null;
        }

        return trackedPosition1Counts;
    }

    if (pairNumber === 2) {
        if (trackedPosition2Counts === null) {
            return null;
        }

        return trackedPosition2Counts;
    }

    return null;
}

function getResetTargetPositionCounts() {
    const position1Counts = getCurrentPositionCounts(1);
    const position2Counts = getCurrentPositionCounts(2);

    if (position1Counts === null && position2Counts === null) {
        return null;
    }

    if (position1Counts === null) {
        return position2Counts;
    }

    if (position2Counts === null) {
        return position1Counts;
    }

    // Use the stronger signal so one stale/zeroed channel does not halve reset.
    return Math.abs(position1Counts) >= Math.abs(position2Counts)
        ? position1Counts
        : position2Counts;
}

async function sendPositionResetCommand() {
    const currentCounts = getResetTargetPositionCounts();
    if (currentCounts === null) {
        alert("No position data available yet");
        logCommand("Failed to start position reset - no position data available yet");
        return false;
    }

    const currentDegrees = countsToDegrees(currentCounts);
    const motorDegrees = Math.round(Math.abs(currentDegrees));

    if (motorDegrees === 0) {
        logCommand("Position reset skipped - motors are already at 0°");
        return false;
    }

    logCommand(`Position reset target: ${formatDegrees(currentDegrees)}`);

    await sendCommandPacket(START_MOTOR, {
        motorDegrees: motorDegrees,
        motorReverse: currentDegrees > 0,
        commandLabel: "Position Reset"
    });

    return true;
}

async function sendCommandPacket(commandId, options = {}) {
    if (!connected) {
        alert("Not connected");
        return false;
    }

    let inSteps = 0;
    let reverse = 0;
    let payload16 = 0;
    let commandLabel = null;

    const isMotorCmd = (commandId === 0b00 || commandId === 0b01);
    const isBtCmd = (commandId === 0b10 || commandId === 0b11);

    if (isMotorCmd) {
        inSteps = 0;
        reverse = options.motorReverse !== undefined ? (options.motorReverse ? 1 : 0) : (motorConfig.reverse ? 1 : 0);
        commandLabel = options.commandLabel || (commandId === START_MOTOR ? "Start Motor" : "Stop Motor");

        const motorDegrees = options.motorDegrees !== undefined ? options.motorDegrees : motorConfig.degrees;
        const motorSpeed = options.motorSpeed !== undefined ? options.motorSpeed : motorConfig.speed;

        payload16 =
            ((motorDegrees & 0xff) << 8) |
            (motorSpeed & 0xff);
    } else if (isBtCmd) {
        inSteps = bluetoothConfig.inSteps ? 1 : 0;
        reverse = 0;
        payload16 = bluetoothConfig.sendIntervalMs & 0xffff;
    } else {
        console.error("Invalid command ID:", commandId);
        return false;
    }

    const packet =
        ((commandId & 0b11) << 18) |
        ((inSteps & 0b1) << 17) |
        ((reverse & 0b1) << 16) |
        payload16;

    const buffer = new ArrayBuffer(3);
    const view = new DataView(buffer);
    view.setUint8(0, (packet >> 16) & 0xff);
    view.setUint8(1, (packet >> 8) & 0xff);
    view.setUint8(2, packet & 0xff);

    cmdChar.writeValue(buffer);

    if (isMotorCmd) {
        const motorDegrees = options.motorDegrees !== undefined ? options.motorDegrees : motorConfig.degrees;
        const motorSpeed = options.motorSpeed !== undefined ? options.motorSpeed : motorConfig.speed;
        logCommand(`Sent command: ${commandLabel}, ` +
            `degrees=${motorDegrees}, speed=${motorSpeed}%, ` +
            `inSteps=${inSteps}, reverse=${reverse}`);
    } else if (isBtCmd) {
        logCommand(`Sent command: ${commandId === START_BT ? "Start Bluetooth Test" : "Stop Bluetooth Test"}, ` +
            `interval=${bluetoothConfig.sendIntervalMs}ms, inSteps=${inSteps}`);
    } else {
        logCommand(`Sent unknown command ${commandId}`);
    }

    return true;
}

async function sendDataPacket() {
    if (!bluetoothTestRunning) return;

    const buffer = new ArrayBuffer(16);
    const view = new DataView(buffer);
    const nowUs = performance.now() * 1000;
    const seq = packetSeq++;

    view.setUint32(0, seq, true);
    view.setUint32(4, Math.floor(nowUs), true);
    view.setUint32(8, 0xAAAAAAAA, true);
    view.setUint32(12, 0xBBBBBBBB, true);

    pendingPackets.set(seq, {
        sendUs: nowUs,
        p0: 0xAAAAAAAA,
        p1: 0xBBBBBBBB,
        sizeBytes: buffer.byteLength
    });

    if (pendingPackets.size > MAX_PENDING_PACKETS) {
        const oldestSeq = pendingPackets.keys().next().value;
        pendingPackets.delete(oldestSeq);
    }

    try {
        await dataChar.writeValueWithoutResponse(buffer);
        sentPacketCount++;
    } catch (e) {
        pendingPackets.delete(seq);
        console.error("Packet send failed:", e);
        setTimeout(sendDataPacket, bluetoothConfig.sendIntervalMs);
        return;
    }

    logCommand(`Sent Data packet #${seq}`);
    setTimeout(sendDataPacket, bluetoothConfig.sendIntervalMs);
}

function sweepTimedOutPackets(nowUs) {
    const timeoutUs = Math.max(
        MIN_PACKET_TIMEOUT_US,
        bluetoothConfig.sendIntervalMs * 3000
    );

    for (const [seq, pkt] of pendingPackets) {
        if ((nowUs - pkt.sendUs) > timeoutUs) {
            pendingPackets.delete(seq);
            timedOutPacketCount++;
        }
    }
}

function popcount32(x) {
    x >>>= 0;
    let count = 0;
    while (x) {
        x &= (x - 1) >>> 0;
        count++;
    }
    return count;
}

function handleDataEchoNotification(event) {
    const dv = event.target.value;
    if (dv.byteLength !== 16) return;

    const view = new DataView(dv.buffer, dv.byteOffset, dv.byteLength);
    const seq = view.getUint32(0, true);
    const echoedTimestampUs = view.getUint32(4, true);
    const echoedP0 = view.getUint32(8, true);
    const echoedP1 = view.getUint32(12, true);

    const nowUs = performance.now() * 1000;
    sweepTimedOutPackets(nowUs);

    const sent = pendingPackets.get(seq);
    if (!sent) return;

    pendingPackets.delete(seq);
    echoedPacketCount++;
    echoedPayloadBytes += sent.sizeBytes;

    const rttUs = nowUs - sent.sendUs;
    rttSamplesUs.push(rttUs);
    if (rttSamplesUs.length > RTT_SAMPLE_WINDOW) {
        rttSamplesUs.shift();
    }

    const sentTsUs = Math.floor(sent.sendUs) >>> 0;
    const bitErrors =
        popcount32((echoedTimestampUs ^ sentTsUs) >>> 0) +
        popcount32((echoedP0 ^ sent.p0) >>> 0) +
        popcount32((echoedP1 ^ sent.p1) >>> 0);

    if (bitErrors > 0) {
        perErrorPackets++;
    }
}

function handleMetricsNotification(event) {
    const nowUs = performance.now() * 1000;
    const dv = event.target.value;
    if (dv.byteLength !== 28) return;

    const view = new DataView(dv.buffer, dv.byteOffset, dv.byteLength);

    const pkt = {
        timestampMs: view.getUint32(0, true),
        txCount: view.getUint32(4, true),
        txBytes: view.getUint32(8, true),
        intervalMeanUs: view.getUint32(12, true),
        intervalJitterUs: view.getUint32(16, true),
        sendOverruns: view.getUint32(20, true),
        uptimeMs: view.getUint32(24, true)
    };

    if (lastMetricsArrivalUs !== null) {
        const deltaUs = nowUs - lastMetricsArrivalUs;
        intervalDeltasUs.push(deltaUs);
        if (intervalDeltasUs.length > 100) intervalDeltasUs.shift();
    }
    lastMetricsArrivalUs = nowUs;

    let arrivalJitterUs = 0;
    if (intervalDeltasUs.length > 1) {
        const mean =
            intervalDeltasUs.reduce((a, b) => a + b, 0) /
            intervalDeltasUs.length;

        const variance =
            intervalDeltasUs.reduce(
                (s, d) => s + (d - mean) ** 2,
                0
            ) / intervalDeltasUs.length;

        arrivalJitterUs = Math.sqrt(variance);
    }

    let avgRttUs = 0;
    if (rttSamplesUs.length) {
        avgRttUs =
            rttSamplesUs.reduce((a, b) => a + b, 0) /
            rttSamplesUs.length;
    }

    let throughputKbps = 0;
    const elapsedUs = btTestStartUs ? (nowUs - btTestStartUs) : 0;
    if (elapsedUs > 0) {
        throughputKbps =
            (echoedPayloadBytes * 8) / (elapsedUs / 1e6) / 1000;
    }

    sweepTimedOutPackets(nowUs);
    const erroredPackets = perErrorPackets + timedOutPacketCount;
    const per = sentPacketCount > 0 ? (erroredPackets / sentPacketCount) : 0;

    logData(
        `Metrics @ ${pkt.timestampMs}ms: ` +
        `RTT=${Math.round(avgRttUs)}us, ` +
        `PER=${(per * 100).toFixed(2)}%, ` +
        `HostJitter=${Math.round(arrivalJitterUs)}us, ` +
        `FwJitter=${pkt.intervalJitterUs}us, ` +
        `Throughput=${throughputKbps.toFixed(2)}kbps, ` +
        `Overruns=${pkt.sendOverruns}`
    );
}

function handlePosition1Notification(event) {
    const dv = event.target.value;
    if (dv.byteLength !== 4) return;

    const view = new DataView(dv.buffer, dv.byteOffset, dv.byteLength);
    const positionCounts = view.getInt32(0, true);

    if (trackedPosition1Counts === null || latestPosition1Counts === null) {
        trackedPosition1Counts = positionCounts;
    } else {
        const deltaCounts = positionCounts - latestPosition1Counts;

        const looksLikeCounterReset =
            Math.abs(positionCounts) <= COUNTER_RESET_NEAR_ZERO_COUNTS &&
            Math.abs(deltaCounts) >= COUNTER_RESET_JUMP_THRESHOLD_COUNTS;

        if (!looksLikeCounterReset) {
            trackedPosition1Counts += deltaCounts;
        }
    }

    latestPosition1Counts = positionCounts;

    if (trackedPosition1Counts !== null) {
        updatePositionDisplay(1, trackedPosition1Counts);
    }
}

function handlePosition2Notification(event) {
    const dv = event.target.value;
    if (dv.byteLength !== 4) return;

    const view = new DataView(dv.buffer, dv.byteOffset, dv.byteLength);
    const positionCounts = view.getInt32(0, true);

    if (trackedPosition2Counts === null || latestPosition2Counts === null) {
        trackedPosition2Counts = positionCounts;
    } else {
        const deltaCounts = positionCounts - latestPosition2Counts;

        const looksLikeCounterReset =
            Math.abs(positionCounts) <= COUNTER_RESET_NEAR_ZERO_COUNTS &&
            Math.abs(deltaCounts) >= COUNTER_RESET_JUMP_THRESHOLD_COUNTS;

        if (!looksLikeCounterReset) {
            trackedPosition2Counts += deltaCounts;
        }
    }

    latestPosition2Counts = positionCounts;

    if (trackedPosition2Counts !== null) {
        updatePositionDisplay(2, trackedPosition2Counts);
    }
}

function updatePositionDisplay(pairNumber, positionCounts) {
    const angleEl = document.getElementById(`position${pairNumber}-angle`);
    const countsEl = document.getElementById(`position${pairNumber}-counts`);
    const userAngleEl = document.getElementById(`user-position${pairNumber}-angle`);
    const userCountsEl = document.getElementById(`user-position${pairNumber}-counts`);

    const degrees = countsToDegrees(positionCounts);
    const distanceMm = countsToMillimeters(positionCounts);
    const signedDegrees = formatDegrees(degrees);
    const signedDistance = formatDistance(distanceMm);

    angleEl.textContent = signedDistance;
    countsEl.textContent = `${signedDegrees} relative to horizontal`;

    userAngleEl.textContent = signedDistance;
    userCountsEl.textContent = `${signedDegrees} relative to horizontal`;

    recordGraphPoint(pairNumber, distanceMm);
}

function countsToDegrees(positionCounts) {
    const degrees = (positionCounts / COUNTS_PER_REVOLUTION) * 360;
    const wrappedDegrees = degrees % 360;

    // Wrap to one turn while preserving sign so CW/CCW cancel correctly.
    return Object.is(wrappedDegrees, -0) ? 0 : wrappedDegrees;
}

function formatDegrees(degrees) {
    return `${degrees >= 0 ? "+" : ""}${degrees.toFixed(1)}°`;
}

function countsToMillimeters(positionCounts) {
    return (positionCounts / COUNTS_PER_REVOLUTION) * LIFT_MM_PER_REVOLUTION;
}

function mmToDegrees(mmValue) {
    return (mmValue / LIFT_MM_PER_REVOLUTION) * 360;
}

function mmToCm(mmValue) {
    return mmValue / 10;
}

function formatDistance(mmValue) {
    const absMm = Math.abs(mmValue);
    const sign = mmValue >= 0 ? "+" : "-";
    if (absMm >= 10) {
        return `${sign}${mmToCm(absMm).toFixed(2)} cm`;
    }

    return `${sign}${absMm.toFixed(1)} mm`;
}

function resetPositionDisplay() {
    latestPosition1Counts = null;
    latestPosition2Counts = null;
    trackedPosition1Counts = null;
    trackedPosition2Counts = null;
    graphHistory = [];
    requestGraphDraw();
    document.getElementById("position1-angle").textContent = "--.- mm";
    document.getElementById("position1-counts").textContent = "Waiting for BLE position data";
    document.getElementById("position2-angle").textContent = "--.- mm";
    document.getElementById("position2-counts").textContent = "Waiting for BLE position data";
    document.getElementById("user-position1-angle").textContent = "--.- mm";
    document.getElementById("user-position1-counts").textContent = "Waiting for BLE position data";
    document.getElementById("user-position2-angle").textContent = "--.- mm";
    document.getElementById("user-position2-counts").textContent = "Waiting for BLE position data";
}

function recordGraphPoint(pairNumber, mmValue) {
    graphHistory.push({
        t: performance.now(),
        pair: pairNumber,
        mm: mmValue
    });

    trimGraphHistory();
    requestGraphDraw();
}

function trimGraphHistory() {
    const cutoff = performance.now() - GRAPH_WINDOW_MS;
    graphHistory = graphHistory.filter(point => point.t >= cutoff);
}

function requestGraphDraw() {
    if (graphAnimationFrameId !== null) {
        return;
    }

    graphAnimationFrameId = requestAnimationFrame(() => {
        graphAnimationFrameId = null;
        drawPositionGraph();
    });
}

function drawPositionGraph() {
    const canvas = document.getElementById("position-graph");
    if (!canvas) {
        return;
    }

    const ctx = canvas.getContext("2d");
    const cssWidth = canvas.clientWidth;
    const cssHeight = canvas.clientHeight;

    if (!cssWidth || !cssHeight) {
        return;
    }

    const dpr = window.devicePixelRatio || 1;
    const targetWidth = Math.floor(cssWidth * dpr);
    const targetHeight = Math.floor(cssHeight * dpr);
    if (canvas.width !== targetWidth || canvas.height !== targetHeight) {
        canvas.width = targetWidth;
        canvas.height = targetHeight;
    }

    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssWidth, cssHeight);

    const padding = { top: 14, right: 12, bottom: 24, left: 44 };
    const plotW = cssWidth - padding.left - padding.right;
    const plotH = cssHeight - padding.top - padding.bottom;

    if (plotW <= 0 || plotH <= 0) {
        return;
    }

    const now = performance.now();
    const windowStart = now - GRAPH_WINDOW_MS;
    const visiblePoints = graphHistory.filter(point => point.t >= windowStart);
    const mmValues = visiblePoints.map(point => point.mm);
    mmValues.push(0, userViewTargetMm);

    let minMm = Math.min(...mmValues);
    let maxMm = Math.max(...mmValues);

    if (Math.abs(maxMm - minMm) < 1) {
        maxMm += 1;
        minMm -= 1;
    }

    const padMm = Math.max(1, (maxMm - minMm) * 0.1);
    maxMm += padMm;
    minMm -= padMm;

    const xOf = t => padding.left + ((t - windowStart) / GRAPH_WINDOW_MS) * plotW;
    const yOf = mm => padding.top + ((maxMm - mm) / (maxMm - minMm)) * plotH;

    ctx.fillStyle = "#ffffff";
    ctx.fillRect(padding.left, padding.top, plotW, plotH);
    ctx.strokeStyle = "#dbe3ef";
    ctx.strokeRect(padding.left, padding.top, plotW, plotH);

    ctx.lineWidth = 1;
    ctx.setLineDash([4, 3]);
    ctx.strokeStyle = "#f43f5e";
    const targetY = yOf(userViewTargetMm);
    ctx.beginPath();
    ctx.moveTo(padding.left, targetY);
    ctx.lineTo(padding.left + plotW, targetY);
    ctx.stroke();
    ctx.setLineDash([]);

    drawGraphSeries(ctx, visiblePoints, 1, "#2563eb", xOf, yOf);
    drawGraphSeries(ctx, visiblePoints, 2, "#059669", xOf, yOf);

    ctx.fillStyle = "#64748b";
    ctx.font = "11px system-ui, sans-serif";
    ctx.fillText(`${minMm.toFixed(1)} mm`, 4, yOf(minMm) + 3);
    ctx.fillText(`${maxMm.toFixed(1)} mm`, 4, yOf(maxMm) + 3);
    ctx.fillText("120s", padding.left + plotW - 28, cssHeight - 6);
    ctx.fillText("Now", padding.left + plotW - 20, padding.top - 2);
}

function drawGraphSeries(ctx, points, pairNumber, color, xOf, yOf) {
    const series = points.filter(point => point.pair === pairNumber);
    if (series.length < 2) {
        return;
    }

    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    series.forEach((point, index) => {
        const x = xOf(point.t);
        const y = yOf(point.mm);
        if (index === 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }
    });
    ctx.stroke();
}

function handleAckNotification(event) {
    const data = event.target.value;
    if (data.byteLength !== 6) return;

    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);

    const commandId = view.getUint8(0);
    const status = view.getUint8(1);
    const timestampMs = view.getUint32(2, true);

    const statusStr = status === ACK_SUCCESS ? "SUCCESS" : "ERROR";
    const commandStr = getCommandName(commandId);

    logCommand(`ACK: ${commandStr} [${statusStr}] at ${timestampMs}ms`);

    if (commandId === START_MOTOR) {
        motorTestRunning = false;
        userViewMotorCommandPending = false;
        updateUI();

        if (status === ACK_SUCCESS) {
            logCommand(`${activeMotorCommandLabel} completed`);
        } else {
            logCommand(`${activeMotorCommandLabel} ended with error ACK`);
        }
    }

    if (commandId === STOP_MOTOR) {
        motorTestRunning = false;
        updateUI();
    }

    if (commandId === STOP_BT) {
        bluetoothTestRunning = false;
        updateUI();
    }
}

function getCommandName(commandId) {
    switch (commandId) {
        case STOP_MOTOR:
            return "STOP_MOTOR";
        case START_MOTOR:
            return "START_MOTOR";
        case STOP_BT:
            return "STOP_BT";
        case START_BT:
            return "START_BT";
        default:
            return `UNKNOWN(${commandId})`;
    }
}

function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function flushBLE() {
    await Promise.resolve();
}

function logCommand(command) {
    const log = document.getElementById("command-log");
    const entry = document.createElement("li");

    const time = new Date().toLocaleTimeString();
    entry.textContent = `[${time}] ${command}`;

    log.prepend(entry);
}

function logData(data) {
    const log = document.getElementById("data-log");
    const entry = document.createElement("li");

    entry.textContent = data;
    log.prepend(entry);
}

function downloadLog(log) {
    const items = Array.from(log.querySelectorAll("li")).map(li => li.textContent).reverse();
    const content = items.join("\n");
    const blob = new Blob([content], { type: "text/plain" });
    const url = URL.createObjectURL(blob);

    const a = document.createElement("a");
    a.href = url;
    a.download = log.id + ".txt";
    a.click();

    URL.revokeObjectURL(url);
}

function bindUIEvents() {
    const passwordInput = document.getElementById("password-input");
    const unlockBtn = document.getElementById("unlock-btn");
    const connectBtn = document.getElementById("connect-btn");
    const disconnectBtn = document.getElementById("disconnect-btn");
    const motorTestBtn = document.getElementById("motor-test-btn");
    const bluetoothTestBtn = document.getElementById("bluetooth-test-btn");
    const stopBtn = document.getElementById("stop-btn");
    const motorAngleSlider = document.getElementById("motor-angle-slider");
    const motorSpeedSlider = document.getElementById("motor-speed-slider");
    const positionResetCheckbox = document.getElementById("position-reset-checkbox");
    const motorReverseCheckbox = document.getElementById("motor-reverse-checkbox");
    const btIntervalSlider = document.getElementById("bt-interval-slider");
    const btStepsCheckbox = document.getElementById("bt-steps-checkbox");
    const downloadCommandLogBtn = document.getElementById("download-command-log-btn");
    const downloadDataLogBtn = document.getElementById("download-data-log-btn");
    const testingViewTabBtn = document.getElementById("testing-view-tab-btn");
    const userViewTabBtn = document.getElementById("user-view-tab-btn");
    const userStepMmInput = document.getElementById("user-step-mm-input");
    const userTargetMmInput = document.getElementById("user-target-mm-input");
    const userStartBtn = document.getElementById("user-start-btn");

    passwordInput.addEventListener("keydown", event => {
        if (event.key === "Enter") {
            unlock();
        }
    });

    unlockBtn.addEventListener("click", unlock);
    connectBtn.addEventListener("click", connectBLE);
    disconnectBtn.addEventListener("click", disconnectBLE);
    motorTestBtn.addEventListener("click", startMotorTest);
    bluetoothTestBtn.addEventListener("click", startBluetoothTest);
    stopBtn.addEventListener("click", stopTests);

    motorAngleSlider.addEventListener("input", event => {
        motorConfig.degrees = Number(event.target.value);
        document.getElementById("degreesVal").textContent = event.target.value;
    });

    motorSpeedSlider.addEventListener("input", event => {
        motorConfig.speed = Number(event.target.value);
        document.getElementById("speedVal").textContent = event.target.value;
    });

    positionResetCheckbox.addEventListener("change", event => {
        motorConfig.positionReset = event.target.checked;
    });

    motorReverseCheckbox.addEventListener("change", event => {
        motorConfig.reverse = event.target.checked;
    });

    btIntervalSlider.addEventListener("input", event => {
        bluetoothConfig.sendIntervalMs = Number(event.target.value);
        document.getElementById("btIntervalVal").textContent = event.target.value;
    });

    btStepsCheckbox.addEventListener("change", event => {
        bluetoothConfig.inSteps = event.target.checked;
    });

    downloadCommandLogBtn.addEventListener("click", () => {
        downloadLog(document.getElementById("command-log"));
    });

    downloadDataLogBtn.addEventListener("click", () => {
        downloadLog(document.getElementById("data-log"));
    });

    testingViewTabBtn.addEventListener("click", () => {
        currentViewTab = "testing";
        updateUI();
    });

    userViewTabBtn.addEventListener("click", () => {
        currentViewTab = "user";
        updateUI();
    });

    userStepMmInput.addEventListener("input", event => {
        const nextValue = Number(event.target.value);
        if (Number.isNaN(nextValue)) {
            return;
        }

        userViewStepMm = Math.min(40, Math.max(1, nextValue));
        updateUserViewControls();
    });

    userTargetMmInput.addEventListener("input", event => {
        const nextValue = Number(event.target.value);
        if (Number.isNaN(nextValue)) {
            return;
        }

        userViewTargetMm = Math.min(120, Math.max(5, nextValue));
        if (userViewCommandedTotalMm > userViewTargetMm) {
            userViewCommandedTotalMm = userViewTargetMm;
        }

        requestGraphDraw();
        updateUserViewControls();
    });

    userStartBtn.addEventListener("click", startUserViewActuation);

    window.addEventListener("resize", requestGraphDraw);
}

bindUIEvents();
updateUserViewControls();
requestGraphDraw();
