package org.gilbertxenodike.btmid.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.DeviceUiState
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme

@Composable
fun DeviceListItem(
    device: DeviceUiState,
    isConnected: Boolean,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Card(modifier = modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.padding(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(text = device.name, style = MaterialTheme.typography.bodyLarge)
                Text(
                    text = device.address,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            if (isConnected) {
                OutlinedButton(onClick = onDisconnect) { Text("Disconnect") }
            } else {
                Button(onClick = onConnect) { Text("Connect") }
            }
        }
    }
}

@Preview(showBackground = true)
@Composable
private fun DeviceListItemPreview() {
    BtmidTheme {
        DeviceListItem(
            device = DeviceUiState(address = "AA:BB:CC:DD:EE:FF", name = "BLE MIDI Keyboard"),
            isConnected = false,
            onConnect = {},
            onDisconnect = {},
        )
    }
}

@Preview(showBackground = true)
@Composable
private fun DeviceListItemConnectedPreview() {
    BtmidTheme {
        DeviceListItem(
            device = DeviceUiState(address = "AA:BB:CC:DD:EE:FF", name = "BLE MIDI Keyboard"),
            isConnected = true,
            onConnect = {},
            onDisconnect = {},
        )
    }
}
