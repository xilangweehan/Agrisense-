package com.rdh.agrisense

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.ui.Modifier
import com.rdh.agrisense.ui.theme.AgriSenseTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            AgriSenseTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    AgriSenseApp(modifier = Modifier.padding(innerPadding))
                }
            }
        }
    }
}