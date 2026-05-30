package org.egon12.btmid.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.egon12.btmid.ConnectionStatus
import org.egon12.btmid.DeviceUiState
import org.egon12.btmid.DrumBackend
import org.egon12.btmid.MidiEventUiModel
import org.egon12.btmid.UiState
import org.egon12.btmid.ui.theme.BtmidTheme

@Composable
fun MainScreen(
    uiState: UiState,
    onGrantPermissions: () -> Unit,
    onStartScan: () -> Unit,
    onStopScan: () -> Unit,
    onConnect: (DeviceUiState) -> Unit,
    onDisconnect: () -> Unit,
    onSetDrumBackend: (DrumBackend) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        if (!uiState.permissionsGranted) {
            PermissionBanner(onGrantPermissions = onGrantPermissions)
        } else {
            StatusRow(
                connectionStatus = uiState.connectionStatus,
                connectedDeviceName = uiState.discoveredDevices
                    .find { it.address == uiState.connectedDeviceAddress }?.name,
                midiActivityPulse = uiState.midiActivityPulse,
            )

            ScanControls(
                connectionStatus = uiState.connectionStatus,
                onStartScan = onStartScan,
                onStopScan = onStopScan,
            )

            DrumEngineSelector(
                selected = uiState.drumBackend,
                samplesLoaded = uiState.samplesLoaded,
                onSelect = onSetDrumBackend,
            )

            if (uiState.discoveredDevices.isNotEmpty()) {
                Text("Discovered devices", style = MaterialTheme.typography.titleSmall)
                uiState.discoveredDevices.forEach { device ->
                    DeviceListItem(
                        device = device,
                        isConnected = device.address == uiState.connectedDeviceAddress,
                        onConnect = { onConnect(device) },
                        onDisconnect = onDisconnect,
                    )
                }
            }

            if (uiState.recentEvents.isNotEmpty()) {
                HorizontalDivider()
                EventLog(events = uiState.recentEvents)
            }
        }
    }
}

@Composable
private fun StatusRow(
    connectionStatus: ConnectionStatus,
    connectedDeviceName: String?,
    midiActivityPulse: Boolean,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        ConnectionStatusBadge(status = connectionStatus, deviceName = connectedDeviceName)
        if (connectionStatus == ConnectionStatus.Connected) {
            MidiActivityIndicator(pulse = midiActivityPulse)
        }
    }
}

@Composable
private fun ConnectionStatusBadge(status: ConnectionStatus, deviceName: String?) {
    val color = when (status) {
        ConnectionStatus.Idle      -> MaterialTheme.colorScheme.outline
        ConnectionStatus.Scanning  -> MaterialTheme.colorScheme.tertiary
        ConnectionStatus.Connected -> MaterialTheme.colorScheme.primary
    }
    val label = when (status) {
        ConnectionStatus.Idle      -> "Not connected"
        ConnectionStatus.Scanning  -> "Scanning..."
        ConnectionStatus.Connected -> "Connected: ${deviceName ?: "unknown"}"
    }
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Box(
            modifier = Modifier
                .size(8.dp)
                .background(color, CircleShape)
        )
        Text(label, style = MaterialTheme.typography.labelMedium, color = color)
    }
}

@Composable
private fun ScanControls(
    connectionStatus: ConnectionStatus,
    onStartScan: () -> Unit,
    onStopScan: () -> Unit,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        if (connectionStatus == ConnectionStatus.Scanning) {
            CircularProgressIndicator(modifier = Modifier.size(24.dp), strokeWidth = 2.dp)
            OutlinedButton(onClick = onStopScan) { Text("Stop Scan") }
        } else {
            Button(
                onClick = onStartScan,
                enabled = connectionStatus != ConnectionStatus.Connected,
            ) {
                Text("Scan for MIDI Devices")
            }
        }
    }
}

@Composable
private fun EventLog(events: List<MidiEventUiModel>) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text("Recent MIDI events", style = MaterialTheme.typography.titleSmall)
        events.asReversed().forEach { event ->
            Text(
                text = event.description,
                style = MaterialTheme.typography.bodySmall,
                fontFamily = FontFamily.Monospace,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun PermissionBanner(onGrantPermissions: () -> Unit) {
    Card(
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.errorContainer,
        ),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(
                text = "Bluetooth permissions required",
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onErrorContainer,
            )
            Text(
                text = "This app needs Bluetooth permissions to scan and connect to MIDI devices.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onErrorContainer,
            )
            Button(onClick = onGrantPermissions) { Text("Grant Permissions") }
        }
    }
}

@Preview(showBackground = true)
@Composable
private fun MainScreenPermissionNeededPreview() {
    BtmidTheme {
        MainScreen(
            uiState = UiState(permissionsGranted = false),
            onGrantPermissions = {},
            onStartScan = {},
            onStopScan = {},
            onConnect = {},
            onDisconnect = {},
            onSetDrumBackend = {},
        )
    }
}

@Preview(showBackground = true)
@Composable
private fun MainScreenConnectedPreview() {
    BtmidTheme {
        MainScreen(
            uiState = UiState(
                permissionsGranted = true,
                connectionStatus = ConnectionStatus.Connected,
                discoveredDevices = listOf(DeviceUiState("AA:BB:CC:DD:EE:FF", "BLE MIDI Keyboard")),
                connectedDeviceAddress = "AA:BB:CC:DD:EE:FF",
                recentEvents = listOf(
                    MidiEventUiModel("NoteOn  ch1 note=60 vel=100"),
                    MidiEventUiModel("NoteOff ch1 note=60"),
                    MidiEventUiModel("NoteOn  ch10 note=38 vel=80"),
                ),
                midiActivityPulse = true,
            ),
            onGrantPermissions = {},
            onStartScan = {},
            onStopScan = {},
            onConnect = {},
            onDisconnect = {},
            onSetDrumBackend = {},
        )
    }
}

@Preview(showBackground = true)
@Composable
private fun MainScreenScanningPreview() {
    BtmidTheme {
        MainScreen(
            uiState = UiState(
                permissionsGranted = true,
                connectionStatus = ConnectionStatus.Scanning,
                discoveredDevices = listOf(
                    DeviceUiState("AA:BB:CC:DD:EE:FF", "BLE MIDI Keyboard"),
                    DeviceUiState("11:22:33:44:55:66", "BLE MIDI Controller"),
                ),
            ),
            onGrantPermissions = {},
            onStartScan = {},
            onStopScan = {},
            onConnect = {},
            onDisconnect = {},
            onSetDrumBackend = {},
        )
    }
}
