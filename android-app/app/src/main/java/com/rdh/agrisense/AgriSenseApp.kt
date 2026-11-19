package com.rdh.agrisense

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
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay

@Composable
fun AgriSenseApp(modifier: Modifier = Modifier) {
    var input by remember { mutableStateOf("192.168.1.50:8080") }
    var active by remember { mutableStateOf("") }
    var data by remember { mutableStateOf<SensorData?>(null) }
    var status by remember { mutableStateOf("Not connected") }

    LaunchedEffect(active) {
        if (active.isEmpty()) return@LaunchedEffect
        while (true) {
            try {
                data = fetchSensorData(active)
                status = "Connected to $active"
            } catch (e: Exception) {
                status = "Lost connection to $active - retrying..."
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
                label = { Text("ESP32 address") },
                singleLine = true,
                modifier = Modifier.weight(1f)
            )
            Button(onClick = { active = input.trim() }) { Text("Connect") }
        }
        Text(status)
        data?.let { LiveScreen(data = it) }
    }
}