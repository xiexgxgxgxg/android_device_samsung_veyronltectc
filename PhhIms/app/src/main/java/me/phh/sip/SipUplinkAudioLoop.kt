//SPDX-License-Identifier: GPL-2.0
package me.phh.sip

import android.media.AudioRecord
import android.media.MediaCodec
import android.telephony.Rlog
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

internal object SipUplinkAudioLoop {
    fun run(
        logTag: String,
        audioRecord: AudioRecord,
        bufferSize: Int,
        encoder: MediaCodec,
        audioCodec: NegotiatedAudioCodec,
        echoCancellation: SipEchoCancellationSession,
        callStopped: AtomicBoolean,
        callGeneration: AtomicInteger,
        generation: Int,
        gainQ8: Int,
        nextSequenceNumber: () -> Int,
        nextTimestamp: () -> Int,
        sendFrame: (
            sequenceNumber: Int,
            timestamp: Int,
            storageFrame: ByteArray,
            marker: Boolean,
            frameType: Int,
            frameSize: Int,
            realFrameCount: Int
        ) -> Boolean
    ) {
        Rlog.d(
            logTag,
            "IMS uplink gain q8=$gainQ8 ${SipUplinkGain.propertySummary()}"
        )

        var firstPacket = true
        var realFrameCount = 0
        // The encoder takes exactly one 20 ms PCM frame per input buffer; the
        // AudioRecord read size depends on the HAL and can be several frames
        // (the 6.0.1 HAL returns 1280 bytes for AMR-WB).  Re-frame the capture
        // instead of handing the whole read to MediaCodec, which used to throw
        // BufferOverflowException.
        val frameBytes = (audioCodec.sampleRate / 50) * 2
        val frameBuffer = ByteArray(frameBytes)
        var frameFill = 0
        val buffer = ByteArray(if (bufferSize >= frameBytes) bufferSize else frameBytes)
        val aecOutput = ByteArray(
            bufferSize + (audioCodec.sampleRate / 100 * 2)
        )

        while (true) {
            if (callStopped.get() || callGeneration.get() != generation) break

            val nRead = audioRecord.read(buffer, 0, buffer.size)

            if (callStopped.get() || callGeneration.get() != generation) break
            if (nRead <= 0) continue

            val aecSize = echoCancellation.processCapture(
                pcm = buffer,
                sizeBytes = nRead,
                output = aecOutput,
                generation = generation
            )
            val encodeBuffer: ByteArray
            val encodeSize: Int
            if (aecSize == SipEchoCancellationSession.CAPTURE_BYPASS) {
                encodeBuffer = buffer
                encodeSize = nRead
            } else {
                if (aecSize == 0) continue
                encodeBuffer = aecOutput
                encodeSize = aecSize
            }

            var offset = 0
            while (offset < encodeSize) {
                val take = minOf(frameBytes - frameFill, encodeSize - offset)
                System.arraycopy(encodeBuffer, offset, frameBuffer, frameFill, take)
                frameFill += take
                offset += take
                if (frameFill < frameBytes) break

                SipUplinkAudioEncoder.queuePcmInput(
                    logTag = logTag,
                    encoder = encoder,
                    buffer = frameBuffer,
                    size = frameBytes,
                    gainQ8 = gainQ8,
                    logInput = realFrameCount < 5
                )

                val drainState = SipUplinkAudioEncoder.drainEncodedOutput(
                    logTag = logTag,
                    encoder = encoder,
                    audioCodec = audioCodec,
                    firstPacket = firstPacket,
                    realFrameCount = realFrameCount,
                    nextSequenceNumber = nextSequenceNumber,
                    nextTimestamp = nextTimestamp,
                    sendFrame = sendFrame
                )
                firstPacket = drainState.firstPacket
                realFrameCount = drainState.realFrameCount
                frameFill = 0
            }
        }
    }
}
