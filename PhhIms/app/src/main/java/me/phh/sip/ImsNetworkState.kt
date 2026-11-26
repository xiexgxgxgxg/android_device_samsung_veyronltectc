// SPDX-License-Identifier: GPL-2.0
package me.phh.sip

import android.net.ConnectivityManager
import android.net.LinkProperties
import android.os.SystemProperties
import android.net.Network
import android.net.NetworkCapabilities
import android.telephony.Rlog
import android.telephony.TelephonyManager
import android.telephony.ims.stub.ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN
import android.telephony.ims.stub.ImsRegistrationImplBase.REGISTRATION_TECH_LTE
import java.net.Inet6Address
import java.net.InetAddress

internal sealed class ImsNetworkEndpointResolution {
    data class Success(
        val pcscfAddr: InetAddress,
        val localAddr: InetAddress
    ) : ImsNetworkEndpointResolution()

    object WaitingForPcscf : ImsNetworkEndpointResolution()
    object NoLocalAddress : ImsNetworkEndpointResolution()
}

internal object ImsNetworkState {
    fun registrationTechName(tech: Int): String =
        when (tech) {
            REGISTRATION_TECH_IWLAN -> "IWLAN"
            REGISTRATION_TECH_LTE -> "LTE"
            else -> "unknown($tech)"
        }

    fun detectRegistrationTech(
        connectivityManager: ConnectivityManager,
        network: Network?,
        lp: LinkProperties
    ): Int {
        val iface = lp.interfaceName ?: ""
        if (iface.startsWith("ipsec", ignoreCase = true)) {
            return REGISTRATION_TECH_IWLAN
        }

        val caps = if (network != null) {
            try {
                connectivityManager.getNetworkCapabilities(network)
            } catch (_: Throwable) {
                null
            }
        } else {
            null
        }

        return if (caps?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true) {
            REGISTRATION_TECH_IWLAN
        } else {
            REGISTRATION_TECH_LTE
        }
    }

    fun getPcscfServers(
        lp: LinkProperties,
        ipVersionPolicy: SipIpVersionPolicy = SipIpVersionPolicy.ANY
    ): List<InetAddress> {
        val addresses = (lp.javaClass.getMethod("getPcscfServers").invoke(lp) as List<*>)
            .filterIsInstance<InetAddress>()
            .sortedBy { if (it is Inet6Address) 0 else 1 }
        val matching = addresses.filter(ipVersionPolicy::accepts)
        val fromLink = matching.ifEmpty { addresses }
        if (fromLink.isNotEmpty()) return fromLink
        return pcscfFromRilProperties(ipVersionPolicy)
    }

    /*
     * The Samsung 6.0.1 vendor RIL answers the modem's IPv6 P-CSCF through QMI
     * IMS ("SDM SIP config") and stores it in ril.pcscfv4.* / ril.pcscfv6.*
     * properties.  The legacy RIL_Data_Call_Response_v11 ABI that Android 11
     * consumes has a single IPv4-only pcscf string, so the framework's
     * LinkProperties never carries these addresses.  Read them back from the
     * properties the RIL recorded for the modem IMS profile.
     */
    private const val TAG = "PHH ImsNetworkState"
    private val RIL_PCSCF_PROFILES = listOf("gsm2", "gsm1", "gsm0", "gsm3")

    private fun pcscfFromRilProperties(
        ipVersionPolicy: SipIpVersionPolicy
    ): List<InetAddress> {
        val found = mutableListOf<InetAddress>()
        for (ipVersion in listOf("v6", "v4")) {
            for (profile in RIL_PCSCF_PROFILES) {
                for (index in 0..2) {
                    val value = SystemProperties.get(
                        "ril.pcscf$ipVersion.$profile.$index", ""
                    ).trim()
                    if (value.isEmpty()) continue
                    val address = try {
                        InetAddress.getByName(value)
                    } catch (t: Throwable) {
                        continue
                    }
                    if (address.isAnyLocalAddress) continue
                    if (!ipVersionPolicy.accepts(address)) continue
                    if (found.any { it == address }) continue
                    found.add(address)
                }
            }
        }
        if (found.isNotEmpty()) {
            Rlog.d(TAG, "P-CSCF from RIL properties: $found")
        }
        return found
    }

    fun getImsLocalAddress(
        lp: LinkProperties,
        ipVersionPolicy: SipIpVersionPolicy = SipIpVersionPolicy.ANY,
        peerAddress: InetAddress? = null
    ): InetAddress? {
        val addresses = lp.linkAddresses
            .map { it.address }
            .filter { !it.isAnyLocalAddress && !it.isLoopbackAddress }
            .sortedBy { if (it is Inet6Address) 0 else 1 }
        val sameFamily = peerAddress?.let { peer ->
            addresses.filter { candidate ->
                (candidate is Inet6Address) == (peer is Inet6Address)
            }
        }.orEmpty()
        return sameFamily.firstOrNull(ipVersionPolicy::accepts)
            ?: addresses.firstOrNull(ipVersionPolicy::accepts)
            ?: sameFamily.firstOrNull()
            ?: addresses.firstOrNull()
    }

    fun resolveEndpoint(
        tag: String,
        lp: LinkProperties,
        mnc: String,
        mcc: String,
        preferredPcscf: InetAddress? = null,
        ipVersionPolicy: SipIpVersionPolicy = SipIpVersionPolicy.ANY
    ): ImsNetworkEndpointResolution {
        val pcscfs = getPcscfServers(lp, ipVersionPolicy)
        val pcscf = preferredPcscf ?: if (pcscfs.isNotEmpty()) {
            pcscfs[0]
        } else {
            // RIL did not provide P-CSCF via LinkProperties. Try standard
            // 3GPP DNS discovery (TS 23.003 §13.2): resolve the well-known
            // IMS domain for this PLMN.
            val dnsFallback = try {
                InetAddress.getAllByName(
                    "ims.mnc${mnc}.mcc${mcc}.pub.3gppnetwork.org"
                ).firstOrNull(ipVersionPolicy::accepts)
            } catch (t: Throwable) {
                null
            } ?: try {
                InetAddress.getAllByName(
                    "ims.mnc${mnc}.mcc${mcc}.3gppnetwork.org"
                ).firstOrNull(ipVersionPolicy::accepts)
            } catch (t: Throwable) {
                null
            } ?: android.os.SystemProperties
                .get("persist.ims.pcscf_fallback", "")
                .takeIf { it.isNotEmpty() }
                ?.let {
                    try {
                        InetAddress.getByName(it)
                    } catch (t: Throwable) {
                        null
                    }
                }

            if (dnsFallback != null) {
                Rlog.w(tag, "No P-CSCF from RIL, using fallback: $dnsFallback")
                dnsFallback
            } else {
                Rlog.w(tag, "No P-CSCF and all fallbacks failed, waiting for onLinkPropertiesChanged")
                return ImsNetworkEndpointResolution.WaitingForPcscf
            }
        }

        val localAddr = getImsLocalAddress(lp, ipVersionPolicy, pcscf)
        if (localAddr == null) {
            Rlog.w(tag, "No usable local address on IMS link properties")
            return ImsNetworkEndpointResolution.NoLocalAddress
        }

        return ImsNetworkEndpointResolution.Success(pcscf, localAddr)
    }

    fun ratName(rat: Int): String =
        when (rat) {
            TelephonyManager.NETWORK_TYPE_LTE -> "LTE"
            TelephonyManager.NETWORK_TYPE_NR -> "NR"
            TelephonyManager.NETWORK_TYPE_IWLAN -> "IWLAN"
            TelephonyManager.NETWORK_TYPE_EDGE -> "EDGE"
            TelephonyManager.NETWORK_TYPE_GPRS -> "GPRS"
            TelephonyManager.NETWORK_TYPE_GSM -> "GSM"
            TelephonyManager.NETWORK_TYPE_UMTS -> "UMTS"
            TelephonyManager.NETWORK_TYPE_HSPA -> "HSPA"
            TelephonyManager.NETWORK_TYPE_HSDPA -> "HSDPA"
            TelephonyManager.NETWORK_TYPE_HSUPA -> "HSUPA"
            TelephonyManager.NETWORK_TYPE_UNKNOWN -> "UNKNOWN"
            else -> "rat($rat)"
        }

    fun isRatReadyForImsNetworkRequest(
        tag: String,
        telephonyManager: TelephonyManager
    ): Boolean {
        val dataRat = try {
            telephonyManager.dataNetworkType
        } catch (t: Throwable) {
            TelephonyManager.NETWORK_TYPE_UNKNOWN
        }

        val voiceRat = try {
            telephonyManager.voiceNetworkType
        } catch (t: Throwable) {
            TelephonyManager.NETWORK_TYPE_UNKNOWN
        }

        val ready =
            dataRat == TelephonyManager.NETWORK_TYPE_LTE ||
                dataRat == TelephonyManager.NETWORK_TYPE_NR ||
                dataRat == TelephonyManager.NETWORK_TYPE_IWLAN ||
                voiceRat == TelephonyManager.NETWORK_TYPE_LTE ||
                voiceRat == TelephonyManager.NETWORK_TYPE_NR

        Rlog.d(
            tag,
            "IMS network request RAT gate: data=${ratName(dataRat)} voice=${ratName(voiceRat)} ready=$ready"
        )

        return ready
    }
}
