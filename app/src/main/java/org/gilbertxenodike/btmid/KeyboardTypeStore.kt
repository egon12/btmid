package org.gilbertxenodike.btmid

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val KEYBOARD_TYPE_KEY = stringPreferencesKey("keyboard_type")

class KeyboardTypeStore(private val context: Context) {
    val keyboardType: Flow<KeyboardType> = context.dataStore.data
        .map { prefs ->
            KeyboardType.entries.firstOrNull { it.name == prefs[KEYBOARD_TYPE_KEY] }
                ?: KeyboardType.Piano
        }

    suspend fun save(type: KeyboardType) {
        context.dataStore.edit { prefs ->
            prefs[KEYBOARD_TYPE_KEY] = type.name
        }
    }
}
