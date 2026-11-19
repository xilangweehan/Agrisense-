package com.rdh.agrisense

data class SensorData(
    val temp: Double,
    val hum: Double,
    val soil: Int,
    val light: Int,
    val pump: Boolean,
    val led: Boolean,
    val fan: Boolean,
    val ml: Int,
    val cap: Int
)