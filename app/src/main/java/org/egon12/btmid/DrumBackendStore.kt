package org.egon12.btmid

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

val Context.dataStore by preferencesDataStore(name = "settings")

private val DRUM_BACKEND_KEY = stringPreferencesKey("drum_backend")

class DrumBackendStore(private val context: Context) {
    val drumBackend: Flow<DrumBackend> = context.dataStore.data
        .map { prefs ->
            when (prefs[DRUM_BACKEND_KEY]) {
                "Fm" -> DrumBackend.Fm
                "Samples" -> DrumBackend.Samples
                else -> DrumBackend.Noise
            }
        }

    suspend fun save(backend: DrumBackend) {
        context.dataStore.edit { prefs ->
            prefs[DRUM_BACKEND_KEY] = backend.name
        }
    }
}
