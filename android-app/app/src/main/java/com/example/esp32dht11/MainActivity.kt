package com.example.esp32dht11

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import java.util.ArrayDeque
import java.util.UUID

private val SERVICE_UUID: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
private val TEMP_CHAR_UUID: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
private val HUM_CHAR_UUID: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
private val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
private const val TARGET_NAME = "ESP32-DHT11"

class MainActivity : ComponentActivity() {

    private val viewModel: BleViewModel by viewModels()

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { results ->
            val granted = results.values.all { it }
            if (granted) {
                viewModel.startScan()
            } else {
                viewModel.updateStatus("Permission denied")
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        viewModel.initBle(this)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    val uiState by viewModel.state.collectAsState()
                    DhtScreen(
                        state = uiState,
                        onScan = { ensurePermissionsThenScan() },
                        onDisconnect = { viewModel.disconnect() },
                        onConnect = { viewModel.connectAddress(it) }
                    )
                }
            }
        }
    }

    private fun ensurePermissionsThenScan() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionLauncher.launch(
                arrayOf(
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT
                )
            )
        } else {
            permissionLauncher.launch(
                arrayOf(
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.BLUETOOTH
                )
            )
        }
    }
}

data class UiState(
    val status: String = "Idle",
    val temperature: String = "--",
    val humidity: String = "--",
    val connectedDevice: String? = null,
    val devices: List<DiscoveredDevice> = emptyList()
)

data class DiscoveredDevice(
    val address: String,
    val name: String?,
    val rssi: Int
)

class BleViewModel : ViewModel() {
    private var appContext: Context? = null
    private var adapter: BluetoothAdapter? = null
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var scanTimeoutJob: Job? = null
    private var scanning = false
    private val seenDevices = LinkedHashMap<String, DiscoveredDevice>()
    private val pendingDescriptors = ArrayDeque<BluetoothGattDescriptor>()
    private var writingDescriptor = false

    private val _state = MutableStateFlow(UiState())
    val state: StateFlow<UiState> = _state

    fun initBle(context: Context) {
        appContext = context.applicationContext
        val manager = appContext!!.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        adapter = manager.adapter
        scanner = adapter?.bluetoothLeScanner
    }

    fun startScan() {
        val leScanner = scanner ?: run {
            updateStatus("Bluetooth unavailable")
            return
        }
        if (adapter?.isEnabled != true) {
            updateStatus("Please enable Bluetooth")
            return
        }
        stopScan()
        updateStatus("Scanning…")
        seenDevices.clear()
        _state.value = _state.value.copy(devices = emptyList())
        scanning = true
        // Use no ScanFilter to avoid missing devices whose advertisements omit the service UUID.
        val filters = emptyList<ScanFilter>()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        leScanner.startScan(filters, settings, scanCallback)
        scanTimeoutJob = viewModelScope.launch {
            // Stop scanning after 10 seconds to save battery
            kotlinx.coroutines.delay(10_000)
            if (scanning) {
                stopScan()
                if (_state.value.connectedDevice == null) {
                    updateStatus("Device not found")
                }
            }
        }
    }

    fun stopScan() {
        scanning = false
        scanTimeoutJob?.cancel()
        scanTimeoutJob = null
        scanner?.stopScan(scanCallback)
    }

    fun disconnect() {
        stopScan()
        gatt?.close()
        gatt = null
        _state.value = _state.value.copy(
            status = "Disconnected",
            connectedDevice = null,
            temperature = "--",
            humidity = "--"
        )
    }

    fun updateStatus(message: String) {
        _state.value = _state.value.copy(status = message)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device ?: return
            val deviceName = device.name ?: result.scanRecord?.deviceName
            val advHasService = result.scanRecord?.serviceUuids?.any { it.uuid == SERVICE_UUID } == true
            val nameMatches = deviceName == TARGET_NAME
            val looksLikeEsp = deviceName?.contains("ESP", ignoreCase = true) == true
            val entry = DiscoveredDevice(device.address, deviceName, result.rssi)
            seenDevices[device.address] = entry
            _state.value = _state.value.copy(
                devices = seenDevices.values.sortedByDescending { it.rssi }
            )
            if (nameMatches || advHasService || looksLikeEsp) {
                stopScan()
                updateStatus("Found device ${device.address} (${deviceName ?: "unknown"})")
                connect(device)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            updateStatus("Scan failed: $errorCode")
        }
    }

    private fun connect(device: BluetoothDevice) {
        updateStatus("Connecting to ${device.address}")
        viewModelScope.launch(Dispatchers.Main) {
            val ctx = appContext
            if (ctx == null) {
                updateStatus("Context unavailable")
                return@launch
            }
            gatt = device.connectGatt(ctx, false, gattCallback)
        }
    }

    fun connectAddress(address: String) {
        val dev = adapter?.getRemoteDevice(address)
        if (dev != null) {
            connect(dev)
        } else {
            updateStatus("Device $address unavailable")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                _state.value = _state.value.copy(
                    status = "Discovering services…",
                    connectedDevice = gatt.device.address
                )
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                _state.value = _state.value.copy(
                    status = "Disconnected",
                    connectedDevice = null
                )
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            val service: BluetoothGattService? = gatt.getService(SERVICE_UUID)
            val tempChar = service?.getCharacteristic(TEMP_CHAR_UUID)
            val humChar = service?.getCharacteristic(HUM_CHAR_UUID)
            if (tempChar == null || humChar == null) {
                updateStatus("Characteristics not found")
                return
            }
            enableNotifications(gatt, tempChar)
            enableNotifications(gatt, humChar)
            updateStatus("Connected")
            // Read initial values
            gatt.readCharacteristic(tempChar)
            gatt.readCharacteristic(humChar)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            handleCharacteristic(characteristic)
        }

        override fun onCharacteristicRead(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            handleCharacteristic(characteristic)
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int
        ) {
            writeNextDescriptor(gatt)
        }
    }

    private fun handleCharacteristic(characteristic: BluetoothGattCharacteristic) {
        val value = characteristic.value?.toString(Charsets.UTF_8)?.trim() ?: return
        when (characteristic.uuid) {
            TEMP_CHAR_UUID -> _state.value = _state.value.copy(temperature = value)
            HUM_CHAR_UUID -> _state.value = _state.value.copy(humidity = value)
        }
    }

    private fun enableNotifications(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
        gatt.setCharacteristicNotification(characteristic, true)
        val cccd: BluetoothGattDescriptor? = characteristic.getDescriptor(CCCD_UUID)
        if (cccd == null) {
            updateStatus("CCCD missing for ${characteristic.uuid}")
            return
        }
        cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        pendingDescriptors.add(cccd)
        if (!writingDescriptor) {
            writeNextDescriptor(gatt)
        }
    }

    private fun writeNextDescriptor(gatt: BluetoothGatt) {
        val next = pendingDescriptors.poll() ?: run {
            writingDescriptor = false
            return
        }
        writingDescriptor = true
        gatt.writeDescriptor(next)
    }
}

@Composable
private fun DhtScreen(
    state: UiState,
    onScan: () -> Unit,
    onDisconnect: () -> Unit,
    onConnect: (String) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text(text = "状态: ${state.status}")
        Text(text = "已连接设备: ${state.connectedDevice ?: "-"}")
        Text(text = "温度(℃): ${state.temperature}")
        Text(text = "湿度(%): ${state.humidity}")
        Text(text = "版本: ${BuildConfig.VERSION_NAME}")
        Button(onClick = onScan) {
            Text("扫描并连接")
        }
        Button(onClick = onDisconnect, enabled = state.connectedDevice != null) {
            Text("断开连接")
        }
        Text(text = "扫描到的设备(${state.devices.size}):")
        LazyColumn(
            modifier = Modifier
                .weight(1f)
                .fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            items(state.devices) { d ->
                val label = "${d.name ?: "未知设备"} (${d.address}) RSSI:${d.rssi}"
                Button(onClick = { onConnect(d.address) }) {
                    Text(label)
                }
            }
        }
    }
}
