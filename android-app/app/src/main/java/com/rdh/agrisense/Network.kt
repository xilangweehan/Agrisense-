package com.rdh.agrisense

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

suspend fun fetchSensorData(address: String): SensorData = withContext(Dispatchers.IO) {
    val conn = URL("http://$address/data").openConnection() as HttpURLConnection
    conn.connectTimeout = 4000
    conn.readTimeout = 4000
    try {
        val text = conn.inputStream.bufferedReader().use { it.readText() }
        val j = JSONObject(text)
        SensorData(
            temp = j.getDouble("temp"),
            hum = j.getDouble("hum"),
            soil = j.getInt("soil"),
            light = j.getInt("light"),
            pump = j.getBoolean("pump"),
            led = j.getBoolean("led"),
            fan = j.getBoolean("fan"),
            ml = j.getInt("ml"),
            cap = j.getJSONObject("cfg").getInt("cap")
        )
    } finally {
        conn.disconnect()
    }
}