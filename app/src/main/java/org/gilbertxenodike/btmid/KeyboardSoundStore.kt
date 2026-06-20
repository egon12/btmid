package org.gilbertxenodike.btmid

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val KEYBOARD_SOUND_KEY = stringPreferencesKey("keyboard_sound")

class KeyboardSoundStore(private val context: Context) {
    val keyboardSound: Flow<KeyboardSound> = context.dataStore.data
        .map { prefs ->
            KeyboardSound.entries.firstOrNull { it.name == prefs[KEYBOARD_SOUND_KEY] }
                ?: KeyboardSound.Piano
        }

    suspend fun save(sound: KeyboardSound) {
        context.dataStore.edit { prefs ->
            prefs[KEYBOARD_SOUND_KEY] = sound.name
        }
    }
}
