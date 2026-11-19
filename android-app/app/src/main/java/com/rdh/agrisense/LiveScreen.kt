package com.rdh.agrisense

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

val fakeData = SensorData(
    temp = 27.6, hum = 61.0, soil = 35, light = 75,
    pump = false, led = true, fan = false, ml = 320, cap = 500
)

@Composable
fun Tile(label: String, value: String, color: Color, modifier: Modifier = Modifier) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(color)
            .padding(16.dp)
    ) {
        Text(text = value, color = Color.White, fontSize = 26.sp, fontWeight = FontWeight.Bold)
        Text(text = label, color = Color.White, fontSize = 13.sp)
    }
}

@Composable
fun Badge(name: String, on: Boolean) {
    Text(
        text = "$name ${if (on) "ON" else "OFF"}",
        color = Color.White,
        fontSize = 12.sp,
        modifier = Modifier
            .clip(RoundedCornerShape(10.dp))
            .background(if (on) Color(0xFF2E7D32) else Color(0xFF999999))
            .padding(horizontal = 10.dp, vertical = 3.dp)
    )
}

@Composable
fun ReservoirBar(ml: Int, cap: Int) {
    val fraction = (ml.toFloat() / cap).coerceIn(0f, 1f)
    val percent = (fraction * 100).toInt()
    val barColor = when {
        percent <= 20 -> Color(0xFFC62828)
        percent <= 40 -> Color(0xFFEF6C00)
        else -> Color(0xFF2E7D32)
    }
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(text = "Reservoir (estimated): ~$ml ml of $cap ml ($percent%)")
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(14.dp)
                .clip(RoundedCornerShape(7.dp))
                .background(Color(0xFFE3E8E3))
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth(fraction)
                    .height(14.dp)
                    .background(barColor)
            )
        }
    }
}

@Composable
fun LiveScreen(data: SensorData, modifier: Modifier = Modifier) {
    Column(
        modifier = modifier.padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Tile("Temp", "${data.temp}°C", Color(0xFFD84315), Modifier.weight(1f))
            Tile("Humidity", "${data.hum}%", Color(0xFF1565C0), Modifier.weight(1f))
        }
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Tile("Soil moisture", "${data.soil}%", Color(0xFF2E7D32), Modifier.weight(1f))
            Tile("Light", "${data.light}%", Color(0xFFF9A825), Modifier.weight(1f))
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Badge("Pump", data.pump)
            Badge("LED", data.led)
            Badge("Fan", data.fan)
        }
        ReservoirBar(ml = data.ml, cap = data.cap)
    }
}