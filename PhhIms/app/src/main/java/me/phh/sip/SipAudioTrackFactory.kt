// SPDX-License-Identifier: GPL-2.0
package me.phh.sip

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack

object SipAudioTrackFactory {
    fun createVoiceCallTrack(
        audioCodec: NegotiatedAudioCodec = SipAudioCodecs.AMR_NB
    ): AudioTrack {
        val sampleRate = audioCodec.sampleRate
        val channelMask = AudioFormat.CHANNEL_OUT_MONO
        val encoding = AudioFormat.ENCODING_PCM_16BIT
        val minBufferSize = AudioTrack.getMinBufferSize(sampleRate, channelMask, encoding)
        val frameBytes = (sampleRate / 50) * 2
        val bufferSize = maxOf(minBufferSize, frameBytes * 4)

        // USAGE_VOICE_COMMUNICATION instead of the legacy STREAM_VOICE_CALL
        // constructor: the 6.0.1 audio HAL then serves the stream from its
        // "voip-call" usecase, which has both an earpiece (voip-call) and a
        // speaker (voip-call speaker) mixer path.  The plain PCM playback
        // usecase only had a speaker path, and it skipped the path update when
        // its cached device matched the requested one, so selecting the
        // earpiece left the sound on the speaker.
        val attributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
            .build()
        val format = AudioFormat.Builder()
            .setSampleRate(sampleRate)
            .setEncoding(encoding)
            .setChannelMask(channelMask)
            .build()

        return AudioTrack.Builder()
            .setAudioAttributes(attributes)
            .setAudioFormat(format)
            .setBufferSizeInBytes(bufferSize)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
    }
}
