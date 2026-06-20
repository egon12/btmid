package org.gilbertxenodike.btmid

import android.Manifest
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import org.gilbertxenodike.btmid.ui.MainScreen
import org.gilbertxenodike.btmid.ui.theme.BtmidTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            BtmidTheme {
                val vm: MainViewModel = viewModel()
                val uiState by vm.uiState.collectAsState()

                val permissionLauncher = rememberLauncherForActivityResult(
                    ActivityResultContracts.RequestMultiplePermissions()
                ) { results ->
                    vm.onPermissionsResult(results.values.all { it })
                }

                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    MainScreen(
                        uiState = uiState,
                        onGrantPermissions = {
                            permissionLauncher.launch(
                                arrayOf(
                                    Manifest.permission.BLUETOOTH_SCAN,
                                    Manifest.permission.BLUETOOTH_CONNECT,
                                )
                            )
                        },
                        onStartScan = { vm.startScan() },
                        onStopScan = { vm.stopScan() },
                        onConnect = { vm.connect(it) },
                        onDisconnect = { vm.disconnect() },
                        onSetDrumBackend = { vm.setDrumBackend(it) },
                        onSetKeyboardSound = { vm.setKeyboardSound(it) },
                        modifier = Modifier.padding(innerPadding),
                        showSelectEngineDialog = { vm.showSelectEngineDialog(it) },
                        onSelectEngine = { vm.selectEngine(it) },
                        onLoopRecord = { vm.loopRecord() },
                        onLoopStop   = { vm.loopStop()   },
                        onLoopClear  = { vm.loopClear()  }
                    )
                }
            }
        }
    }
}
