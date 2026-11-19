package com.rdh.agrisense

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

data class HistoryRow(
    val ts: String,
    val temp: Double,
    val hum: Double,
    val soil: Int,
    val light: Int,
    val pump: Boolean
)

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

suspend fun fetchHistory(address: String): List<HistoryRow> = withContext(Dispatchers.IO) {
    val conn = URL("http://$address/history").openConnection() as HttpURLConnection
    conn.connectTimeout = 4000
    conn.readTimeout = 8000
    try {
        val text = conn.inputStream.bufferedReader().use { it.readText() }
        val arr = JSONArray(text)
        List(arr.length()) { i ->
            val o = arr.getJSONObject(i)
            HistoryRow(
                ts = o.getString("ts"),
                temp = o.getDouble("temp"),
                hum = o.getDouble("hum"),
                soil = o.getInt("soil"),
                light = o.getInt("light"),
                pump = o.getInt("pump") == 1
            )
        }
    } finally {
        conn.disconnect()
    }
}

suspend fun postRefilled(address: String): Int = withContext(Dispatchers.IO) {
    val conn = URL("http://$address/refilled").openConnection() as HttpURLConnection
    conn.requestMethod = "POST"
    conn.connectTimeout = 4000
    conn.readTimeout = 4000
    conn.doOutput = true
    try {
        conn.outputStream.use { it.write(ByteArray(0)) }
        conn.responseCode
    } finally {
        conn.disconnect()
    }
}