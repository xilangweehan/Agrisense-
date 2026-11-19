package com.rdh.agrisense

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val SoilColor = Color(0xFF2E7D32)
private val HumColor = Color(0xFF1565C0)
private val LightColor = Color(0xFFF9A825)
private val TempColor = Color(0xFFD84315)

private fun timeOf(r: HistoryRow): String =
    if (r.ts.length >= 16) r.ts.substring(11, 16) else r.ts

@Composable
fun HistoryChart(rows: List<HistoryRow>, modifier: Modifier = Modifier) {
    val gridColor = Color(0xFFBBBBBB)
    Canvas(modifier = modifier.fillMaxWidth().height(220.dp)) {
        if (rows.size < 2) return@Canvas
        val w = size.width
        val h = size.height
        val stepX = w / (rows.size - 1)

        for (k in 0..5) {
            val y = h - k / 5f * h
            drawLine(gridColor, Offset(0f, y), Offset(w, y), strokeWidth = 1f)
        }

        rows.forEachIndexed { i, r ->
            if (r.pump) {
                drawRect(
                    color = Color(0x331565C0),
                    topLeft = Offset(i * stepX - stepX / 2, 0f),
                    size = Size(stepX, h)
                )
            }
        }

        fun line(values: List<Float>, color: Color) {
            val path = Path()
            values.forEachIndexed { i, v ->
                val x = i * stepX
                val y = h - v.coerceIn(0f, 100f) / 100f * h
                if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
            }
            drawPath(path, color, style = Stroke(width = 4f))
        }

        line(rows.map { it.soil.toFloat() }, SoilColor)
        line(rows.map { it.hum.toFloat() }, HumColor)
        line(rows.map { it.light.toFloat() }, LightColor)
        line(rows.map { it.temp.toFloat() * 2f }, TempColor)
    }
}

@Composable
fun HistorySection(rows: List<HistoryRow>, modifier: Modifier = Modifier) {
    Column(
        modifier = modifier.padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Text("History", fontWeight = FontWeight.Bold)
        if (rows.size < 2) {
            Text("Waiting for logged readings (1 per minute)...", fontSize = 13.sp)
        } else {
            HistoryChart(rows)
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(timeOf(rows.first()), fontSize = 12.sp)
                Text(timeOf(rows.last()), fontSize = 12.sp)
            }
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Soil %", color = SoilColor, fontSize = 12.sp)
                Text("Humidity %", color = HumColor, fontSize = 12.sp)
                Text("Light %", color = LightColor, fontSize = 12.sp)
            }
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Temp °C (scale 0 to 50)", color = TempColor, fontSize = 12.sp)
                Text("Blue band = pump running", fontSize = 12.sp)
            }
        }
    }
}