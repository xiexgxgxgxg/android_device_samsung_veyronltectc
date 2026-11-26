//SPDX-License-Identifier: GPL-2.0
package me.phh.sip

// Kotlin 1.3 does not have String?.toBooleanStrictOrNull(); provide the same
// strict semantics so the 1.6-era call sites keep working on LineageOS 18.1.
internal fun String?.toBooleanStrictOrNull(): Boolean? =
    when (this?.trim()?.toLowerCase()) {
        "true" -> true
        "false" -> false
        else -> null
    }
