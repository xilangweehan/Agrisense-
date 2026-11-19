package com.rdh.agrisense

import android.content.Context
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun AgriSenseApp(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences("agrisense", Context.MODE_PRIVATE) }
    val saved = remember { prefs.getString("addr", "") ?: "" }

    var input by remember { mutableStateOf(saved) }
    var active by remember { mutableStateOf(saved) }
    var data by remember { mutableStateOf<SensorData?>(null) }
    var stale by remember { mutableStateOf(false) }
    var lastOk by remember { mutableStateOf("") }
    var status by remember {
        mutableStateOf(if (saved.isEmpty()) "Not connected" else "Connecting to $saved...")
    }

    LaunchedEffect(active) {
        if (active.isEmpty()) return@LaunchedEffect
        while (true) {
            try {
                data = fetchSensorData(active)
                stale = false
                lastOk = SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date())
                status = "Connected to $active - updated $lastOk"
            } catch (e: Exception) {
                stale = true
                status = if (lastOk.isEmpty()) {
                    "Could not reach $active - retrying..."
                } else {
                    "Lost connection to $active - showing values from $lastOk"
                }
            }
            delay(5000)
        }
    }

    Column(
        modifier = modifier
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            OutlinedTextField(
                value = input,
                onValueChange = { input = it },
                label = { Text("ESP32 address, e.g. 192.168.1.77") },
                singleLine = true,
                modifier = Modifier.weight(1f)
            )
            Button(onClick = {
                val addr = input.trim()
                if (addr.isNotEmpty()) {
                    if (addr != active) {
                        data = null
                        lastOk = ""
                    }
                    prefs.edit().putString("addr", addr).apply()
                    active = addr
                    status = "Connecting to $addr..."
                }
            }) { Text("Connect") }
        }
        Text(status)
        data?.let {
            LiveScreen(data = it, modifier = Modifier.alpha(if (stale) 0.5f else 1f))
        }
    }
}