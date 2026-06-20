package org.gilbertxenodike.btmid.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import org.gilbertxenodike.btmid.AudioEngine

enum class SelectedAudioEngine { Oboe, Wifi }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EngineSelector(
    selectEngineDialogVisible: Boolean,
    showSelectEngineDialog: (Boolean) -> Unit,
    currentEngine: AudioEngine,        // The engine currently active in your app backend
    onSelectEngine: (AudioEngine) -> Unit, // Dispatched ONLY when Save is clicked
) {
    val sheetState = rememberModalBottomSheetState()
    val ip = if (currentEngine is AudioEngine.Wifi) currentEngine.host else ""
    var typedIp by remember { mutableStateOf(ip) }

    val selectedEngineEnum =
        if (currentEngine == AudioEngine.Oboe) SelectedAudioEngine.Oboe else SelectedAudioEngine.Wifi

    var selectedEngine by remember { mutableStateOf(selectedEngineEnum) }

    val ipRegex =
        "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$".toRegex()
    val isIpValid = typedIp.matches(ipRegex)


    if (!selectEngineDialogVisible) {
        Button(
            onClick = {
                showSelectEngineDialog(true)
            }) {
            Text("Select Engine")
        }

        return
    }

    ModalBottomSheet(
        onDismissRequest = { showSelectEngineDialog(false) }, sheetState = sheetState
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 24.dp, end = 24.dp, bottom = 40.dp, top = 8.dp)
        ) {
            Text(
                text = "Audio Output Engine",
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.padding(bottom = 20.dp)
            )

            // Option 1: Local Oboe Engine
            EngineRow(
                title = "Local Device (Oboe)",
                description = "Ultra-low latency real-time synthesis on this phone.",
                selected = (selectedEngine == SelectedAudioEngine.Oboe),
                onClick = { selectedEngine = SelectedAudioEngine.Oboe })

            Spacer(modifier = Modifier.height(12.dp))


            // Option 2: WiFi UDP Opus Engine
            EngineRow(
                title = "Wireless Stream (WiFi UDP)",
                description = "Streams compressed Opus audio packets over your local network.",
                selected = (selectedEngine == SelectedAudioEngine.Wifi),
                onClick = { selectedEngine = SelectedAudioEngine.Wifi })

            // Animated IP Input Field
            AnimatedVisibility(
                visible = selectedEngine == SelectedAudioEngine.Wifi,
                enter = expandVertically(),
                exit = shrinkVertically()
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 48.dp, top = 16.dp, end = 8.dp)
                ) {
                    OutlinedTextField(
                        value = typedIp,
                        onValueChange = { typedIp = it },
                        label = { Text("Receiver IP Address") },
                        placeholder = { Text("192.168.1.50") },
                        isError = typedIp.isNotEmpty() && !isIpValid,
                        supportingText = {
                            if (typedIp.isNotEmpty() && !isIpValid) {
                                Text(
                                    "Invalid IP address format",
                                    color = MaterialTheme.colorScheme.error
                                )
                            } else {
                                Text("Enter target machine's IPv4 address")
                            }
                        },
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
            }

            Spacer(modifier = Modifier.height(32.dp))

            // The Action Save Button
            Button(
                onClick = {

                    val engine = when (selectedEngine) {
                        SelectedAudioEngine.Oboe -> AudioEngine.Oboe
                        SelectedAudioEngine.Wifi -> AudioEngine.Wifi(typedIp, 5004)
                    }


                    onSelectEngine(engine)
                    showSelectEngineDialog(false)
                }, enabled = selectedEngine == SelectedAudioEngine.Oboe || isIpValid, modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    text = if (selectedEngine == SelectedAudioEngine.Oboe) "Apply Local Engine"
                    else "Connect & Stream Audio"
                )
            }
        }
    }
}

@Composable
fun EngineRow(
    title: String, description: String, selected: Boolean, onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .selectable(
                selected = selected, onClick = onClick, role = Role.RadioButton
            )
            .padding(vertical = 8.dp), verticalAlignment = Alignment.CenterVertically
    ) {
        RadioButton(selected = selected, onClick = null)
        Column(
            modifier = Modifier
                .padding(start = 16.dp)
                .weight(1f)
        ) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            Text(
                text = description,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}
