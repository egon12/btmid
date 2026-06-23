package org.gilbertxenodike.btmid

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val WAVEFORM_KEY = stringPreferencesKey("synth_waveform")

class WaveformStore(private val context: Context) {
    val waveform: Flow<SynthWaveform> = context.dataStore.data
        .map { prefs ->
            SynthWaveform.entries.firstOrNull { it.name == prefs[WAVEFORM_KEY] }
                ?: SynthWaveform.Sine
        }

    suspend fun save(waveform: SynthWaveform) {
        context.dataStore.edit { prefs ->
            prefs[WAVEFORM_KEY] = waveform.name
        }
    }
}
